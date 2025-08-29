/*
 * ESP32-P4 Parallel TX Driver using PARLIO + Direct GDMA Link API
 * 
 * This implementation uses PARLIO for GPIO/clock management but accesses
 * the internal GDMA link lists directly for precise HUB75 BCM control.
 * 
 * KEY APPROACH:
 * 1. PARLIO sets up GPIO routing and clock generation
 * 2. We access PARLIO's internal GDMA channel and link lists
 * 3. Manual DMA descriptor management for BCM timing control
 * 4. Full compatibility with ESP32-S3 HUB75 library architecture
 */

#if SOC_PARLIO_SUPPORTED

#pragma message "Compiling for ESP32-P4 with PARLIO support"

#ifdef ARDUINO_ARCH_ESP32
   #include <Arduino.h>
#endif

#include "parlio_tx_parallel16.hpp"
#include "esp_attr.h"
#include "esp_idf_version.h"
#include "hal/parlio_ll.h"
#include "parlio_priv.h"     // Local copy of parlio private headers
#include "gdma_link.h"       // Local copy of GDMA link API

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)		
  #include "esp_private/gpio.h"
#endif

static const char* TAG = "P4_PARLIO_BCM";

// Callback function prototypes
static IRAM_ATTR bool parlio_tx_buffer_switched_callback(parlio_tx_unit_handle_t tx_unit, 
                                                        const parlio_tx_buffer_switched_event_data_t *event_data, 
                                                        void *user_data);

static IRAM_ATTR bool parlio_tx_done_callback(parlio_tx_unit_handle_t tx_unit, 
                                              const parlio_tx_done_event_data_t *event_data, 
                                              void *user_data);

// Static callback data
static volatile bool transmission_complete = false;

// ------------------------------------------------------------------------------

void Bus_Parallel16::config(const config_t& cfg)
{
    _cfg = cfg;
}

bool Bus_Parallel16::init(void)
{
    ESP_LOGI(TAG, "Initializing ESP32-P4 PARLIO TX unit");
    
    // Fixed 16-bit data width for HUB75 compatibility
    const size_t data_width = 16;
    
    ESP_LOGI(TAG, "Using 16-bit data width for HUB75");

    // Configure PARLIO TX unit
    parlio_tx_unit_config_t parlio_config = {
        .clk_src = PARLIO_CLK_SRC_DEFAULT,
        .clk_in_gpio_num = -1,  // Use internal clock
        .input_clk_src_freq_hz = 0,  // Not used with internal clock
        .output_clk_freq_hz = _cfg.bus_freq,
        .data_width = data_width,
        .data_gpio_nums = {-1}, // Will be set below
        .clk_out_gpio_num = _cfg.pin_wr,
        .valid_gpio_num = -1,  // No valid signal
        .valid_start_delay = 0,
        .valid_stop_delay = 0,
        .trans_queue_depth = 4,
        .max_transfer_size = 64 * 1024,  // PARLIO will create DMA link lists internally for this size
        .dma_burst_size = 16,
        .sample_edge = _cfg.invert_pclk ? PARLIO_SAMPLE_EDGE_NEG : PARLIO_SAMPLE_EDGE_POS,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB,
        .flags = {
            .clk_gate_en = 0,
            .allow_pd = 0,
            .invert_valid_out = 0
        }
    };
    
    // Copy data pin configuration - all 16 pins for HUB75
    for (size_t i = 0; i < 16; i++) {
        parlio_config.data_gpio_nums[i] = _cfg.pin_data[i];
    }
    
    // Create PARLIO TX unit
    esp_err_t ret = parlio_new_tx_unit(&parlio_config, &_parlio_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PARLIO TX unit: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Setup transmit configuration
    _transmit_config.idle_value = 0x00;
    _transmit_config.bitscrambler_program = NULL;
    _transmit_config.flags.queue_nonblocking = 0;
    _transmit_config.flags.loop_transmission = 1; // Enable loop transmission for continuous operation
    
    // Register event callbacks
    parlio_tx_event_callbacks_t callbacks = {
        .on_trans_done = parlio_tx_done_callback,
        .on_buffer_switched = parlio_tx_buffer_switched_callback
    };
    
    ret = parlio_tx_unit_register_event_callbacks(_parlio_unit, &callbacks, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register callbacks: %s", esp_err_to_name(ret));
        parlio_del_tx_unit(_parlio_unit);
        _parlio_unit = nullptr;
        return false;
    }
    
    // Enable the PARLIO TX unit
    ret = parlio_tx_unit_enable(_parlio_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable PARLIO TX unit: %s", esp_err_to_name(ret));
        parlio_del_tx_unit(_parlio_unit);
        _parlio_unit = nullptr;
        return false;
    }
    
    _is_transmitting = false;
    _current_buffer_id = 0;
    
    ESP_LOGI(TAG, "PARLIO TX unit initialized successfully");
    return true;
}

void Bus_Parallel16::release(void)
{
    dma_bus_deinit();
    
    if (_parlio_unit) {
        parlio_tx_unit_disable(_parlio_unit);
        parlio_del_tx_unit(_parlio_unit);
        _parlio_unit = nullptr;
    }
    
    _dmadesc_count = 0;
    _is_transmitting = false;
}

void Bus_Parallel16::dma_bus_deinit(void)
{
    // Free GDMA link lists
    if (_gdma_link_list_a) {
        gdma_del_link_list(_gdma_link_list_a);
        _gdma_link_list_a = nullptr;
    }
    
    if (_gdma_link_list_b) {
        gdma_del_link_list(_gdma_link_list_b);
        _gdma_link_list_b = nullptr;
    }
    
    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;
    _gdma_channel = nullptr;
    
    ESP_LOGI(TAG, "DMA bus deinitialized");
}

void Bus_Parallel16::enable_double_dma_desc(void)
{
    ESP_LOGI(TAG, "Enabled support for secondary DMA buffer.");    
    _double_dma_buffer = true;
}

bool Bus_Parallel16::allocate_dma_desc_memory(uint16_t num_descriptors)
{
    if (num_descriptors == 0) {
        ESP_LOGE(TAG, "Number of descriptors must be greater than 0");
        return false;
    }
    
    // Free existing allocations if any
    dma_bus_deinit();
    
    ESP_LOGI(TAG, "Allocating GDMA link lists for %d descriptors each", num_descriptors);
    
    _dmadesc_count = num_descriptors;
    _max_descriptor_count = num_descriptors;
    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;
    
    // Access the internal GDMA channel from PARLIO unit
    if (!_parlio_unit) {
        ESP_LOGE(TAG, "PARLIO unit not initialized");
        return false;
    }
    
    // Get PARLIO's internal GDMA channel (accessing private structure)
    parlio_tx_unit_t* tx_unit = (parlio_tx_unit_t*)_parlio_unit;
    _gdma_channel = tx_unit->dma_chan;
    
    // Create GDMA link lists for manual descriptor management
    gdma_link_list_config_t link_config = {
        .num_items = num_descriptors,
        .flags = {
            .check_owner = true,
            .items_in_ext_mem = false
        }
    };
    
    // Allocate link list A
    esp_err_t ret = gdma_new_link_list(&link_config, &_gdma_link_list_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GDMA link list A: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Allocate link list B if double buffering is enabled
    if (_double_dma_buffer) {
        ret = gdma_new_link_list(&link_config, &_gdma_link_list_b);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create GDMA link list B: %s", esp_err_to_name(ret));
            gdma_del_link_list(_gdma_link_list_a);
            _gdma_link_list_a = nullptr;
            return false;
        }
    }
    
    ESP_LOGI(TAG, "Successfully allocated GDMA link lists for manual descriptor management");
    return true;
}

void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
{
    // This function now creates actual GDMA link items for precise BCM control
    // Each call adds one descriptor to the link list, just like ESP32-S3
    
    if (size == 0 || data == nullptr) {
        ESP_LOGW(TAG, "Invalid data or size in create_dma_desc_link");
        return;
    }
    
    if (size > PARLIO_DMA_DESCRIPTOR_BUFFER_MAX_SIZE) {
        size = PARLIO_DMA_DESCRIPTOR_BUFFER_MAX_SIZE;
        ESP_LOGW(TAG, "Limiting descriptor size to %d bytes", PARLIO_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
    }
    
    gdma_link_list_handle_t target_list = dmadesc_b ? _gdma_link_list_b : _gdma_link_list_a;
    uint32_t* current_idx = dmadesc_b ? &_dmadesc_b_idx : &_dmadesc_a_idx;
    
    if (target_list == nullptr) {
        ESP_LOGE(TAG, "Target GDMA link list not allocated");
        return;
    }
    
    if (*current_idx >= _max_descriptor_count) {
        ESP_LOGE(TAG, "Exceeded maximum descriptor count: %d", _max_descriptor_count);
        return;
    }
    
    // Configure buffer mount for this descriptor
    gdma_buffer_mount_config_t buf_config = {
        .buffer = data,
        .length = size,
        .flags = {
            .mark_eof = 0,      // Don't mark EOF for individual descriptors
            .mark_final = (*current_idx == _dmadesc_count - 1), // Mark final only for last descriptor
            .bypass_buffer_align_check = 0
        }
    };
    
    // Mount buffer to the link list item
    int end_item_index;
    esp_err_t ret = gdma_link_mount_buffers(target_list, *current_idx, &buf_config, 1, &end_item_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount buffer to GDMA link list: %s", esp_err_to_name(ret));
        return;
    }
    
    (*current_idx)++;
    
    ESP_LOGV(TAG, "Created DMA descriptor %d for buffer %c: ptr=%p, size=%zu", 
             *current_idx - 1, dmadesc_b ? 'B' : 'A', data, size);
}

void Bus_Parallel16::dma_transfer_start()
{
    if (!_parlio_unit || !_gdma_channel || !_gdma_link_list_a) {
        ESP_LOGE(TAG, "PARLIO unit, GDMA channel, or link list not initialized");
        return;
    }
    
    // Use the primary link list (A) for transmission
    gdma_link_list_handle_t active_list = _gdma_link_list_a;
    
    // If double buffering and we want to use list B
    if (_double_dma_buffer && _current_buffer_id == 1 && _gdma_link_list_b) {
        active_list = _gdma_link_list_b;
    }
    
    // Connect the link list to the GDMA channel
    esp_err_t ret = gdma_channel_connect_link_list(_gdma_channel, active_list);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect link list to GDMA channel: %s", esp_err_to_name(ret));
        return;
    }
    
    // Start the GDMA transfer
    ret = gdma_start(_gdma_channel, gdma_link_get_head_addr(active_list));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start GDMA transfer: %s", esp_err_to_name(ret));
        return;
    }
    
    _is_transmitting = true;
    ESP_LOGD(TAG, "Started GDMA transfer with link list %c", 
             (active_list == _gdma_link_list_a) ? 'A' : 'B');
}

void Bus_Parallel16::dma_transfer_stop()
{
    if (_gdma_channel && _is_transmitting) {
        esp_err_t ret = gdma_stop(_gdma_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop GDMA transfer: %s", esp_err_to_name(ret));
        }
        _is_transmitting = false;
        ESP_LOGD(TAG, "Stopped GDMA transfer");
    }
}

void Bus_Parallel16::flip_dma_output_buffer(int back_buffer_id)
{
    if (!_double_dma_buffer || !_gdma_link_list_b) {
        return;
    }
    
    // For ESP32-P4 with manual GDMA control:
    // 1. Stop current transfer
    // 2. Switch to the other link list
    // 3. Restart transfer
    
    _current_buffer_id = back_buffer_id;
    
    if (_is_transmitting) {
        // Stop current transfer
        gdma_stop(_gdma_channel);
        
        // Select the appropriate link list
        gdma_link_list_handle_t target_list = (back_buffer_id == 0) ? _gdma_link_list_a : _gdma_link_list_b;
        
        // Connect new link list and restart
        esp_err_t ret = gdma_channel_connect_link_list(_gdma_channel, target_list);
        if (ret == ESP_OK) {
            ret = gdma_start(_gdma_channel, gdma_link_get_head_addr(target_list));
        }
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to switch to buffer %d: %s", back_buffer_id, esp_err_to_name(ret));
            _is_transmitting = false;
        } else {
            ESP_LOGD(TAG, "Switched to GDMA link list %c", (back_buffer_id == 0) ? 'A' : 'B');
        }
    }
}

// Callback implementations
static IRAM_ATTR bool parlio_tx_buffer_switched_callback(parlio_tx_unit_handle_t tx_unit, 
                                                        const parlio_tx_buffer_switched_event_data_t *event_data, 
                                                        void *user_data)
{
    Bus_Parallel16* bus = (Bus_Parallel16*)user_data;
    
    if (bus && event_data) {
        // This callback is triggered when PARLIO internally switches buffers in loop mode
        // It indicates that the hardware has switched from one DMA buffer to another
        // This is useful for timing-sensitive applications like display refresh
        
        ESP_EARLY_LOGD(TAG, "PARLIO buffer switched: %p -> %p", 
                      event_data->old_buffer_addr, event_data->new_buffer_addr);
        
        // You can use this callback to:
        // 1. Update display driver state
        // 2. Prepare the next frame data
        // 3. Synchronize with display refresh timing
        // 4. Signal application that it's safe to modify the old buffer
    }
    
    return false; // Don't yield from ISR
}

static IRAM_ATTR bool parlio_tx_done_callback(parlio_tx_unit_handle_t tx_unit, 
                                              const parlio_tx_done_event_data_t *event_data, 
                                              void *user_data)
{
    Bus_Parallel16* bus = (Bus_Parallel16*)user_data;
    
    // This callback is triggered when a non-loop transmission completes
    // In loop mode, this might not be called as frequently
    transmission_complete = true;
    
    if (bus) {
        // Mark transmission as complete
        // Note: In loop mode, this may indicate end of one loop iteration
        ESP_EARLY_LOGD(TAG, "PARLIO transmission done");
    }
    
    return false; // Don't yield from ISR
}

#endif // SOC_PARLIO_SUPPORTED
