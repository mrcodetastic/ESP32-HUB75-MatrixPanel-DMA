#include "esp32_parlio_parallel_dma.hpp"

#if defined(CONFIG_IDF_TARGET_ESP32P4)  || \
    defined(CONFIG_IDF_TARGET_ESP32S31) || \
    defined(SOC_PARLIO_SUPPORTED)

static const char* TAG = "Bus_Parallel16_PARLIO";

Bus_Parallel16::Bus_Parallel16()
    : _parlio_unit(nullptr),
      _gdma_channel(nullptr),
      _gdma_link_list_a(nullptr),
      _gdma_link_list_b(nullptr),
      _double_dma_buffer(false),
      _dmadesc_count(0),
      _dmadesc_last(0),
      _dmadesc_a_idx(0),
      _dmadesc_b_idx(0),
      _is_transmitting(false),
      _active_buffer_id(0)
{
}

Bus_Parallel16::~Bus_Parallel16()
{
    release();
}

void Bus_Parallel16::config(const config_t& cfg)
{
#if defined(CONFIG_IDF_TARGET_ESP32S31)
    ESP_LOGI(TAG, "Configuring Bus_Parallel16 for ESP32-S31 (PARLIO)");
#else
    ESP_LOGI(TAG, "Configuring Bus_Parallel16 for ESP32-P4 (PARLIO)");
#endif
    _cfg = cfg;
}

bool Bus_Parallel16::init(void)
{
    ESP_LOGI(TAG, "Performing PARLIO bus init()");

    if (_cfg.parallel_width != 16) {
        ESP_LOGE(TAG, "Only 16-bit parallel width is supported!");
        return false;
    }

    parlio_tx_unit_config_t parlio_config = {
        .clk_src = PARLIO_CLK_SRC_DEFAULT,
        .clk_in_gpio_num = -1,
        .input_clk_src_freq_hz = 0,
        .output_clk_freq_hz = _cfg.bus_freq,
        .data_width = 16,
        .data_gpio_nums = {-1},
        .clk_out_gpio_num = _cfg.pin_wr,
        .valid_gpio_num = -1,
        .valid_start_delay = 0,
        .valid_stop_delay = 0,
        .trans_queue_depth = 4,
        .max_transfer_size = 64 * 1024,
        .dma_burst_size = 16,
        .sample_edge = _cfg.invert_pclk ? PARLIO_SAMPLE_EDGE_NEG : PARLIO_SAMPLE_EDGE_POS,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB,
        .flags = {
            .clk_gate_en = 0,
            .allow_pd = 0,
            .invert_valid_out = 0
        }
    };

    for (size_t i = 0; i < 16; i++) {
        parlio_config.data_gpio_nums[i] = _cfg.pin_data[i];
    }

    esp_err_t ret = parlio_new_tx_unit(&parlio_config, &_parlio_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PARLIO TX unit: %s", esp_err_to_name(ret));
        return false;
    }

    ret = parlio_tx_unit_enable(_parlio_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable PARLIO TX unit: %s", esp_err_to_name(ret));
        parlio_del_tx_unit(_parlio_unit);
        _parlio_unit = nullptr;
        return false;
    }

    // Access the internal GDMA channel created by PARLIO driver
    parlio_tx_unit_t* internal_tx_unit = (parlio_tx_unit_t*)_parlio_unit;
    _gdma_channel = internal_tx_unit->dma_chan;

    ESP_LOGI(TAG, "PARLIO TX unit initialized successfully.");
    return true;
}

void Bus_Parallel16::release(void)
{
    dma_transfer_stop();
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
}

void Bus_Parallel16::enable_double_dma_desc(void)
{
    _double_dma_buffer = true;
    ESP_LOGI(TAG, "Double DMA buffer enabled.");
}

bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
{
    dma_bus_deinit();

    if (len == 0 || !_gdma_channel) {
        ESP_LOGE(TAG, "Invalid descriptor count or GDMA channel uninitialized.");
        return false;
    }

    _dmadesc_count = len;
    _dmadesc_last  = len - 1;

    gdma_link_list_config_t link_config = {
        .num_items = (uint32_t)len,
        .item_alignment = 8,
        .buffer_alignment = 1,
        .flags = {
            .items_in_ext_mem = false,
            .check_owner = true
        }
    };

    esp_err_t ret = gdma_new_link_list(&link_config, &_gdma_link_list_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate GDMA link list A: %s", esp_err_to_name(ret));
        return false;
    }

    if (_double_dma_buffer) {
        ret = gdma_new_link_list(&link_config, &_gdma_link_list_b);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to allocate GDMA link list B: %s", esp_err_to_name(ret));
            dma_bus_deinit();
            return false;
        }
    }

    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;

    return true;
}

void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
{
    gdma_link_list_handle_t target_list = dmadesc_b ? _gdma_link_list_b : _gdma_link_list_a;
    uint32_t* current_idx = dmadesc_b ? &_dmadesc_b_idx : &_dmadesc_a_idx;

    if (!target_list) {
        ESP_LOGE(TAG, "Target GDMA link list is not allocated!");
        return;
    }

    if (*current_idx >= _dmadesc_count) {
        ESP_LOGE(TAG, "Attempted to create more DMA descriptors than allocated (%u)", (unsigned int)_dmadesc_count);
        return;
    }

    bool is_final = (*current_idx == _dmadesc_last);

    gdma_buffer_mount_config_t buf_config = {
        .buffer = data,
        .length = size,
        .flags = {
            .mark_eof = is_final ? 1u : 0u,
            .mark_final = is_final ? 1u : 0u,
            .bypass_buffer_align_check = 0u
        }
    };

    int end_item_index = 0;
    esp_err_t ret = gdma_link_mount_buffers(target_list, (int)*current_idx, &buf_config, 1, &end_item_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount buffer to GDMA link list: %s", esp_err_to_name(ret));
        return;
    }

    (*current_idx)++;
}

void Bus_Parallel16::dma_transfer_start()
{
    if (!_gdma_channel || !_gdma_link_list_a) {
        ESP_LOGE(TAG, "Cannot start DMA: missing GDMA channel or link list!");
        return;
    }

    gdma_link_list_handle_t active_list = (_double_dma_buffer && _active_buffer_id == 1) ? _gdma_link_list_b : _gdma_link_list_a;

    uintptr_t head_addr = gdma_link_get_head_addr(active_list);
    esp_err_t ret = gdma_start(_gdma_channel, head_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start GDMA engine: %s", esp_err_to_name(ret));
        return;
    }

    parlio_hal_context_t* hal = &((parlio_tx_unit_t*)_parlio_unit)->base.group->hal;
    parlio_ll_tx_start(hal->regs, true);

    _is_transmitting = true;
}

void Bus_Parallel16::dma_transfer_stop()
{
    if (_gdma_channel && _is_transmitting) {
        gdma_stop(_gdma_channel);

        if (_parlio_unit) {
            parlio_hal_context_t* hal = &((parlio_tx_unit_t*)_parlio_unit)->base.group->hal;
            parlio_ll_tx_start(hal->regs, false);
        }

        _is_transmitting = false;
    }
}

void Bus_Parallel16::flip_dma_output_buffer(int buffer_id)
{
    if (!_double_dma_buffer || !_gdma_link_list_b) {
        return;
    }

    _active_buffer_id = buffer_id;

    gdma_link_list_handle_t active_list = (_active_buffer_id == 1) ? _gdma_link_list_b : _gdma_link_list_a;

    if (_is_transmitting) {
        gdma_stop(_gdma_channel);
        gdma_start(_gdma_channel, gdma_link_get_head_addr(active_list));
    }
}

#endif // CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31