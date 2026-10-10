#include "esp32_parlio_parallel_dma.hpp"

#if defined(CONFIG_IDF_TARGET_ESP32P4)  || \
    defined(CONFIG_IDF_TARGET_ESP32S31) || \
    defined(SOC_PARLIO_SUPPORTED)

static const char* TAG = "Bus_Parallel16_PARLIO";

/**
 * @brief Enforces an I/O read/write memory fence.
 */
static inline void memoryFence() {
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

/**
 * @brief Construct a new Bus_Parallel16 object and initialize member variables.
 */
Bus_Parallel16::Bus_Parallel16()
    : _gdma_channel(nullptr),
      _gdma_link_list_a(nullptr),
      _gdma_link_list_b(nullptr),
      _dma_buffer_alignment(0),
      _cache_alignment(0),
      _double_dma_buffer(false),
      _total_payload_bytes_a(0),
      _total_payload_bytes_b(0),
      _dmadesc_count(0),
      _dmadesc_last(0),
      _dmadesc_a_idx(0),
      _dmadesc_b_idx(0),
      _is_transmitting(false),
      _active_buffer_id(0)
{
}

/**
 * @brief Destroy the Bus_Parallel16 object and release resources.
 */
Bus_Parallel16::~Bus_Parallel16()
{
    release();
}

/**
 * @brief Apply configuration settings to the parallel bus.
 * @param cfg Configuration structure parameters.
 */
void Bus_Parallel16::config(const config_t& cfg)
{
    ESP_LOGI(TAG, "Configuring Bus_Parallel16 (PARLIO direct LL)");
    _cfg = cfg;
}

/**
 * @brief Configure and route GPIO pins for parallel data and clock signals.
 */
void Bus_Parallel16::configure_pins(void)
{
    for (int i = 0; i < _cfg.parallel_width; i++) {
        if (_cfg.pin_data[i] >= 0) {
            gpio_config_t io_conf = {
                .pin_bit_mask = (1ULL << _cfg.pin_data[i]),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&io_conf);

            esp_rom_gpio_connect_out_signal(
                _cfg.pin_data[i],
                parlio_periph_signals.groups[0].tx_units[0].data_sigs[i],
                false,
                false);
        }
    }

    if (_cfg.pin_wr >= 0) {
        gpio_config_t clk_conf = {
            .pin_bit_mask = (1ULL << _cfg.pin_wr),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&clk_conf);

        esp_rom_gpio_connect_out_signal(
            _cfg.pin_wr,
            parlio_periph_signals.groups[0].tx_units[0].clk_out_sig,
            _cfg.invert_pclk,
            false);
    }
}

/**
 * @brief Synchronize memory payload caches for DMA transfers.
 */
void Bus_Parallel16::sync_payload_for_dma(void)
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (_gdma_link_list_a && _dmadesc_count > 0) {
        size_t desc_size = _dmadesc_count * 16;
        uint32_t start = (uint32_t)_gdma_link_list_a;
        uint32_t aligned_start = start & ~(64 - 1);
        uint32_t aligned_size  = ((start + desc_size + 63) & ~(64 - 1)) - aligned_start;

        esp_cache_msync((void*)aligned_start, aligned_size, 
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    }

    if (_gdma_link_list_b && _dmadesc_count > 0) {
        size_t desc_size = _dmadesc_count * 16;
        uint32_t start = (uint32_t)_gdma_link_list_b;
        uint32_t aligned_start = start & ~(64 - 1);
        uint32_t aligned_size  = ((start + desc_size + 63) & ~(64 - 1)) - aligned_start;

        esp_cache_msync((void*)aligned_start, aligned_size, 
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    }
#endif
    memoryFence();
}

/**
 * @brief Configure the low-level PARLIO peripheral clock and settings.
 */
void Bus_Parallel16::configure_parlio_ll(void)
{
    PERIPH_RCC_ATOMIC() {
        parlio_ll_enable_bus_clock(0, true);
        parlio_ll_reset_register(0);
    }

    hal_utils_clk_div_t divider = {};
	
	divider.integer = 4;
    auto freq = (_cfg.bus_freq);
	if (freq > 10000000L) {      
		divider.integer = 2;
	}
 
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_set_clock_source(&PARL_IO, PARLIO_CLK_SRC_XTAL);
        parlio_ll_tx_set_clock_div(&PARL_IO, &divider);
        parlio_ll_tx_enable_clock(&PARL_IO, true);
        parlio_ll_tx_reset_clock(&PARL_IO);
    }

    esp_rom_delay_us(10);
    parlio_ll_tx_reset_fifo(&PARL_IO);
    esp_rom_delay_us(10);

    parlio_ll_tx_start(&PARL_IO, false);
    
    parlio_ll_tx_set_bus_width(&PARL_IO, _cfg.parallel_width);
    parlio_ll_tx_set_idle_data_value(&PARL_IO, 0);

    parlio_ll_tx_enable_clock_gating(&PARL_IO, false);
    parlio_ll_tx_set_trans_bit_len(&PARL_IO, 0);
	parlio_ll_tx_set_eof_condition(&PARL_IO, PARLIO_LL_TX_EOF_COND_DMA_EOF);

    parlio_ll_enable_interrupt(&PARL_IO, PARLIO_LL_EVENT_TX_MASK, false);
    parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_MASK);
	
    memoryFence();
}

/**
 * @brief Initialize the PARLIO low-level bus and GDMA channels.
 * @return true on success, false on failure.
 */
bool Bus_Parallel16::init(void)
{
    ESP_LOGI(TAG, "Initializing PARLIO Low-Level Bus...");

    configure_pins();

    gdma_channel_alloc_config_t channelConfig = {};
    channelConfig.direction = GDMA_CHANNEL_DIRECTION_TX;
    esp_err_t ret = gdma_new_axi_channel(&channelConfig, &_gdma_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gdma_new_axi_channel failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = gdma_connect(_gdma_channel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_PARLIO, 0));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gdma_connect failed: %s", esp_err_to_name(ret));
        return false;
    }

    gdma_transfer_config_t transferConfig = {};
    transferConfig.max_data_burst_size = 16;
    transferConfig.access_ext_mem = false;
    ret = gdma_config_transfer(_gdma_channel, &transferConfig);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gdma_config_transfer failed: %s", esp_err_to_name(ret));
        return false;
    }

    gdma_strategy_config_t strategy = {};
    strategy.owner_check = false;
    strategy.auto_update_desc = false;
    strategy.eof_till_data_popped = true;
    ret = gdma_apply_strategy(_gdma_channel, &strategy);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gdma_apply_strategy failed: %s", esp_err_to_name(ret));
        return false;
    }

    size_t internal_align = 0, external_align = 0;
    gdma_get_alignment_constraints(_gdma_channel, &internal_align, &external_align);
    _dma_buffer_alignment = internal_align ? internal_align : 1;

    esp_cache_get_alignment(MALLOC_CAP_INTERNAL, &_cache_alignment);

    configure_parlio_ll();

    ESP_LOGI(TAG, "PARLIO Low-Level Bus initialized successfully.");
    return true;
}

/**
 * @brief Release all allocated DMA and bus resources.
 */
void Bus_Parallel16::release(void)
{
    dma_transfer_stop();
    dma_bus_deinit();

    if (_gdma_channel) {
        gdma_del_channel(_gdma_channel);
        _gdma_channel = nullptr;
    }

    _dmadesc_count = 0;
    _is_transmitting = false;
}

/**
 * @brief Deinitialize and free DMA descriptor link lists.
 */
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
    _total_payload_bytes_a = 0;
    _total_payload_bytes_b = 0;
}

/**
 * @brief Enable double buffering for DMA descriptor lists.
 */
void Bus_Parallel16::enable_double_dma_desc(void)
{
    _double_dma_buffer = true;
    ESP_LOGI(TAG, "Double DMA buffer enabled.");
}

/**
 * @brief Allocate memory for GDMA descriptor link lists.
 * @param len Number of descriptors to allocate.
 * @return true on success, false on failure.
 */
bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
{
    dma_bus_deinit();

    if (len == 0) {
        ESP_LOGE(TAG, "Invalid descriptor count.");
        return false;
    }
    _dmadesc_count = len;
    _dmadesc_last  = len - 1;

    gdma_link_list_config_t listConfig = {};
    listConfig.num_items = len;
    listConfig.item_alignment = 8;

    esp_err_t ret = gdma_new_link_list(&listConfig, &_gdma_link_list_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate GDMA link list A: %s", esp_err_to_name(ret));
        return false;
    }

    if (_double_dma_buffer) {
        ret = gdma_new_link_list(&listConfig, &_gdma_link_list_b);
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

/**
 * @brief Create and mount a link to a DMA descriptor buffer.
 * @param data Pointer to data payload buffer.
 * @param size Size of payload buffer in bytes.
 * @param dmadesc_b Target list selector (false for list A, true for list B).
 */
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

    gdma_buffer_mount_config_t mount = {};
    mount.buffer = data;
    mount.buffer_alignment = _dma_buffer_alignment;
    mount.length = size;

    int last_mounted = -1;
    esp_err_t ret = gdma_link_mount_buffers(target_list, (int)*current_idx, &mount, 1, &last_mounted);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gdma_link_mount_buffers failed: %s", esp_err_to_name(ret));
        return;
    }

    if (*current_idx == _dmadesc_last) {
        gdma_link_concat(target_list, (int)_dmadesc_last, target_list, 0);
    }

    if (dmadesc_b) {
        _total_payload_bytes_b += size;
    } else {
        _total_payload_bytes_a += size;
    }

    (*current_idx)++;
}

/**
 * @brief Start continuous DMA data transmission.
 */
void Bus_Parallel16::dma_transfer_start()
{
    if (!_gdma_channel || !_gdma_link_list_a) {
        ESP_LOGE(TAG, "Cannot start DMA: missing GDMA channel or link list!");
        return;
    }

    if (!_is_transmitting) {
        sync_payload_for_dma();

        PERIPH_RCC_ATOMIC() {
            parlio_ll_tx_enable_clock(&PARL_IO, true);
            parlio_ll_tx_reset_clock(&PARL_IO);
        }

        esp_rom_delay_us(10);
        parlio_ll_tx_reset_fifo(&PARL_IO);

        gdma_reset(_gdma_channel);
        memoryFence();

        bool use_buffer_b = (_double_dma_buffer && _active_buffer_id == 1);
        gdma_link_list_handle_t active_list = use_buffer_b ? _gdma_link_list_b : _gdma_link_list_a;

        esp_err_t ret = gdma_start(_gdma_channel, gdma_link_get_head_addr(active_list));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "gdma_start failed: %s", esp_err_to_name(ret));
            return;
        }

        parlio_ll_tx_start(&PARL_IO, true);
        _is_transmitting = true;
    }
}

/**
 * @brief Stop active DMA data transmission.
 */
void Bus_Parallel16::dma_transfer_stop()
{
    if (_is_transmitting) {
        parlio_ll_tx_start(&PARL_IO, false);

        PERIPH_RCC_ATOMIC() {
            parlio_ll_tx_enable_clock(&PARL_IO, false);
            parlio_ll_tx_reset_clock(&PARL_IO);
        }

        if (_gdma_channel) {
            gdma_stop(_gdma_channel);
            gdma_reset(_gdma_channel);
        }

        parlio_ll_tx_reset_fifo(&PARL_IO);
        memoryFence();
        _is_transmitting = false;
    }
}

/**
 * @brief Flip active output buffer index for double-buffered rendering.
 * @param buffer_id Target buffer ID (0 or 1).
 */
int Bus_Parallel16::flip_dma_output_buffer()
{
    if (!_double_dma_buffer || !_gdma_link_list_b) {
        return _draw_buffer_id;
    }

    _active_buffer_id = _draw_buffer_id;

    if (_is_transmitting) {
        gdma_link_list_handle_t active_list = (_active_buffer_id == 1) ? _gdma_link_list_b : _gdma_link_list_a;
        gdma_start(_gdma_channel, gdma_link_get_head_addr(active_list));
    }

    _draw_buffer_id ^= 1;
    return _draw_buffer_id;
}

#endif // CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31