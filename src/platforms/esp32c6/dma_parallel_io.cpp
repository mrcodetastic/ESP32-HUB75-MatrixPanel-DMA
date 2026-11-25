#include "dma_parallel_io.hpp"

#ifdef CONFIG_IDF_TARGET_ESP32C6

#pragma message "Compiling experimental Bus_Parallel16 for ESP32-C6 (triggered PARLIO)"

#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#endif

#include <esp_attr.h>
#include <esp_clk_tree.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include <soc/parl_io_struct.h>
#include <soc/parl_io_reg.h>
#include <hal/parlio_ll.h>
#include <hal/parlio_types.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
  #include <hal/hal_utils.h>
  #include <esp_private/periph_ctrl.h>
#elif __has_include(<esp_private/periph_ctrl.h>)
  #include <esp_private/periph_ctrl.h>
#else
  #include <driver/periph_ctrl.h>
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  #include <hal/parlio_periph.h>
  #define PARLIO_TX_SIG_GROUP soc_parlio_signals[0]
#else
  #include <soc/parlio_periph.h>
  #define PARLIO_TX_SIG_GROUP parlio_periph_signals.groups[0]
#endif

static const char* TAG = "Bus_Parallel16_C6";

// HUB75 OE is active LOW (BIT_OE = 1 << 7). Keep OE HIGH whenever PARLIO is idle
// between segments or frames so the last latched row does not glow brightly.
static constexpr uint32_t HUB75_IDLE_OE_HIGH = (1U << 7);

// ESP32-C6 PARLIO TX byte counter (tx_bytelen) is 16-bit (max 0xFFFF = 65,535 bytes).
// Keep each segment <= 65,532 bytes so it stays 4-byte aligned and within hardware limits.
static constexpr size_t MAX_PARLIO_SEG_BYTES = 65532;

// Recover automatically if a frame stalls (e.g., interrupts paused during SPI flash write).
static constexpr int64_t C6_DMA_STALL_TIMEOUT_US = 150000;

Bus_Parallel16::Bus_Parallel16() = default;

Bus_Parallel16::~Bus_Parallel16()
{
    release();
}

void Bus_Parallel16::config(const config_t &cfg)
{
    _cfg = cfg;
}

bool Bus_Parallel16::init(void)
{
    if (_initialized) {
        return true;
    }

    if (_cfg.parallel_width != 16) {
        ESP_LOGE(TAG, "Parallel bus width must be 16 bits for HUB75");
        return false;
    }

    if (_cfg.pin_wr < 0) {
        ESP_LOGE(TAG, "Invalid clock pin (pin_wr < 0)");
        return false;
    }

    ESP_LOGI(TAG, "Initializing experimental PARLIO triggered bus for ESP32-C6...");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_enable_bus_clock(0, true);
        parlio_ll_reset_register(0);
    }
#else
    periph_module_enable(PERIPH_PARLIO_MODULE);
    periph_module_reset(PERIPH_PARLIO_MODULE);
#endif

    const parlio_clock_source_t clk_src = PARLIO_CLK_SRC_DEFAULT;
    uint32_t periph_src_clk_hz = 0;
    esp_err_t err = esp_clk_tree_src_get_freq_hz((soc_module_clk_t)clk_src,
                                                 ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                                 &periph_src_clk_hz);
    if (err != ESP_OK || periph_src_clk_hz == 0) {
        ESP_LOGE(TAG, "Failed to query PARLIO clock source frequency (%s)", esp_err_to_name(err));
        return false;
    }

    const uint32_t req_freq = (_cfg.bus_freq > 0) ? _cfg.bus_freq : 10000000UL;
    uint32_t div = (periph_src_clk_hz + (req_freq / 2)) / req_freq;
    if (div < 1) {
        div = 1;
    }
#ifdef PARLIO_LL_TX_MAX_CLK_INT_DIV
    if (div > PARLIO_LL_TX_MAX_CLK_INT_DIV) {
        div = PARLIO_LL_TX_MAX_CLK_INT_DIV;
    }
#elif defined(PARLIO_LL_TX_MAX_CLOCK_DIV)
    if (div > PARLIO_LL_TX_MAX_CLOCK_DIV) {
        div = PARLIO_LL_TX_MAX_CLOCK_DIV;
    }
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    hal_utils_clk_div_t clk_div = {};
    clk_div.integer = div;
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_enable_clock(&PARL_IO, true);
        parlio_ll_tx_set_clock_source(&PARL_IO, clk_src);
        parlio_ll_tx_set_clock_div(&PARL_IO, &clk_div);
    }
#else
    parlio_ll_tx_enable_clock(&PARL_IO, true);
    parlio_ll_tx_set_clock_source(&PARL_IO, (parlio_ll_clock_source_t)clk_src);
    parlio_ll_tx_set_clock_div(&PARL_IO, div);
#endif

    _cfg.bus_freq = periph_src_clk_hz / div;
    ESP_LOGI(TAG, "PARLIO clock src=%u Hz, div=%u, output=%u Hz",
             (unsigned int)periph_src_clk_hz, (unsigned int)div, (unsigned int)_cfg.bus_freq);

    // Allocate AHB GDMA TX channel and connect to PARLIO TX
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    gdma_channel_alloc_config_t dma_chan_config = {};
#if defined(CONFIG_GDMA_CTRL_FUNC_IN_IRAM)
    dma_chan_config.flags.isr_cache_safe = 1;
#endif
    err = gdma_new_ahb_channel(&dma_chan_config, &_dma_chan, NULL);
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    gdma_channel_alloc_config_t dma_chan_config = {};
    dma_chan_config.direction = GDMA_CHANNEL_DIRECTION_TX;
#if defined(CONFIG_GDMA_CTRL_FUNC_IN_IRAM)
    dma_chan_config.flags.isr_cache_safe = 1;
#endif
    err = gdma_new_ahb_channel(&dma_chan_config, &_dma_chan);
#else
    gdma_channel_alloc_config_t dma_chan_config = {};
    dma_chan_config.direction = GDMA_CHANNEL_DIRECTION_TX;
    err = gdma_new_channel(&dma_chan_config, &_dma_chan);
#endif
    if (err != ESP_OK || !_dma_chan) {
        ESP_LOGE(TAG, "Failed to allocate GDMA TX channel: %s", esp_err_to_name(err));
        return false;
    }

    err = gdma_connect(_dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_PARLIO, 0));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gdma_connect failed: %s", esp_err_to_name(err));
        release();
        return false;
    }

    gdma_strategy_config_t strategy_config = {};
    strategy_config.owner_check = false;
    strategy_config.auto_update_desc = false;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    strategy_config.eof_till_data_popped = true;
#endif
    gdma_apply_strategy(_dma_chan, &strategy_config);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    gdma_transfer_config_t transfer_config = {};
    transfer_config.max_data_burst_size = 32;
    transfer_config.access_ext_mem = false;
    gdma_config_transfer(_dma_chan, &transfer_config);
#else
    gdma_transfer_ability_t ability = {};
    ability.sram_trans_align = 32;
    ability.psram_trans_align = 0;
    gdma_set_transfer_ability(_dma_chan, &ability);
#endif

    // Configure PARLIO TX unit for 16-bit data-length-terminated transfers
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_reset_clock(&PARL_IO);
    }
#else
    parlio_ll_tx_reset_clock(&PARL_IO);
#endif
    parlio_ll_tx_reset_fifo(&PARL_IO);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_enable_clock(&PARL_IO, false);
    }
#else
    parlio_ll_tx_enable_clock(&PARL_IO, false);
#endif

    parlio_ll_tx_enable_clock_gating(&PARL_IO, false);
    parlio_ll_tx_set_bus_width(&PARL_IO, 16);
    parlio_ll_tx_treat_msb_as_valid(&PARL_IO, false);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    parlio_ll_tx_set_eof_condition(&PARL_IO, PARLIO_LL_TX_EOF_COND_DATA_LEN);
    parlio_ll_tx_set_shift_clock_edge(&PARL_IO, PARLIO_SHIFT_EDGE_POS);
#else
    parlio_ll_tx_set_sample_clock_edge(&PARL_IO, PARLIO_SAMPLE_EDGE_POS);
#endif
    parlio_ll_tx_set_idle_data_value(&PARL_IO, HUB75_IDLE_OE_HIGH);
    parlio_ll_tx_set_trans_bit_len(&PARL_IO, 0);

    parlio_ll_enable_interrupt(&PARL_IO, PARLIO_LL_EVENT_TX_MASK, false);
    parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_MASK);

    // Register PARLIO TX_EOF interrupt (fires after PARLIO finishes shifting all bytes)
#if defined(CONFIG_GDMA_CTRL_FUNC_IN_IRAM)
    constexpr int intr_flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM;
#else
    constexpr int intr_flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_SHARED;
#endif
    err = esp_intr_alloc_intrstatus(
        PARLIO_TX_SIG_GROUP.tx_irq_id,
        intr_flags,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parlio_ll_get_interrupt_status_reg(&PARL_IO))),
        PARLIO_LL_EVENT_TX_MASK,
        parlio_tx_isr,
        this,
        &_parlio_intr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate PARLIO TX interrupt: %s", esp_err_to_name(err));
        release();
        return false;
    }

    // Configure data and clock GPIOs (no pull-ups on push-pull outputs)
    gpio_config_t gpio_conf = {};
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;

    for (int i = 0; i < 16; i++) {
        if (_cfg.pin_data[i] >= 0) {
            gpio_conf.pin_bit_mask = BIT64(_cfg.pin_data[i]);
            gpio_config(&gpio_conf);
            esp_rom_gpio_connect_out_signal(_cfg.pin_data[i],
                                            PARLIO_TX_SIG_GROUP.tx_units[0].data_sigs[i],
                                            false, false);
            gpio_set_drive_capability((gpio_num_t)_cfg.pin_data[i], GPIO_DRIVE_CAP_3);
        }
    }

    // Invert clock output solely in the GPIO matrix to match S3/P4 clkphase polarity
    gpio_conf.pin_bit_mask = BIT64(_cfg.pin_wr);
    gpio_config(&gpio_conf);
    esp_rom_gpio_connect_out_signal(_cfg.pin_wr,
                                    PARLIO_TX_SIG_GROUP.tx_units[0].clk_out_sig,
                                    _cfg.invert_pclk, false);
    gpio_set_drive_capability((gpio_num_t)_cfg.pin_wr, GPIO_DRIVE_CAP_3);

    _initialized = true;
    return true;
}

void Bus_Parallel16::free_dma_desc_memory(void)
{
    dma_transfer_stop();

    if (_dmadesc_a) {
        heap_caps_free(_dmadesc_a);
        _dmadesc_a = nullptr;
    }

    _segments.clear();
    _segments_ptr = nullptr;
    _seg_count = 0;
    _seg_idx = 0;
    _dmadesc_count = 0;
    _dmadesc_a_idx = 0;
}

void Bus_Parallel16::release(void)
{
    free_dma_desc_memory();

    if (_parlio_intr) {
        esp_intr_free(_parlio_intr);
        _parlio_intr = nullptr;
    }

    if (_dma_chan) {
        gdma_disconnect(_dma_chan);
        gdma_del_channel(_dma_chan);
        _dma_chan = nullptr;
    }

    _initialized = false;
}

void Bus_Parallel16::enable_double_dma_desc(void)
{
    ESP_LOGW(TAG, "Double DMA descriptors are not supported on ESP32-C6; ignoring.");
}

bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
{
    free_dma_desc_memory();

    if (len == 0) {
        ESP_LOGE(TAG, "Invalid descriptor count (0)");
        return false;
    }

    _dmadesc_count = len;
    ESP_LOGD(TAG, "Allocating %u bytes for %u DMA descriptors.",
             (unsigned int)(sizeof(HUB75_DMA_DESCRIPTOR_T) * len), (unsigned int)len);

    _dmadesc_a = static_cast<HUB75_DMA_DESCRIPTOR_T *>(
        heap_caps_calloc(len, sizeof(HUB75_DMA_DESCRIPTOR_T), MALLOC_CAP_DMA));

    if (_dmadesc_a == nullptr) {
        ESP_LOGE(TAG, "Couldn't malloc _dmadesc_a. Not enough DMA memory.");
        _dmadesc_count = 0;
        return false;
    }

    _dmadesc_a_idx = 0;
    return true;
}

void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
{
    if (dmadesc_b) {
        return; // Single-buffer only on ESP32-C6
    }

    if (!_dmadesc_a || size == 0) {
        return;
    }

    if (size > DMA_MAX) {
        size = DMA_MAX;
        ESP_LOGW(TAG, "Descriptor size capped at DMA_MAX (%u bytes)", (unsigned int)DMA_MAX);
    }

    if (_dmadesc_a_idx >= _dmadesc_count) {
        ESP_LOGE(TAG, "Attempted to create more DMA descriptors than allocated (%u)",
                 (unsigned int)_dmadesc_count);
        return;
    }

    HUB75_DMA_DESCRIPTOR_T *desc = &_dmadesc_a[_dmadesc_a_idx];
    desc->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    desc->dw0.suc_eof = 1;
    desc->dw0.size = size;
    desc->dw0.length = size;
    desc->buffer = data;
    desc->next = nullptr;

    // Auto-split into <= MAX_PARLIO_SEG_BYTES (65,532 B) non-circular segments
    if (_segments.empty() || (_segments.back().byte_len + size > MAX_PARLIO_SEG_BYTES)) {
        _segments.push_back(segment_t{ _dmadesc_a_idx, static_cast<uint32_t>(size) });
    } else {
        HUB75_DMA_DESCRIPTOR_T *prev = &_dmadesc_a[_dmadesc_a_idx - 1];
        prev->dw0.suc_eof = 0;
        prev->next = desc;
        _segments.back().byte_len += static_cast<uint32_t>(size);
    }

    _segments_ptr = _segments.data();
    _seg_count = _segments.size();
    _dmadesc_a_idx++;
}

void IRAM_ATTR Bus_Parallel16::start_segment_hw(const segment_t &seg)
{
    parlio_ll_tx_set_idle_data_value(&PARL_IO, HUB75_IDLE_OE_HIGH);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_reset_clock(&PARL_IO);
    }
#else
    parlio_ll_tx_reset_clock(&PARL_IO);
#endif

    // Ensure >= 2 clock cycles between reset_clock and disable/FIFO reset
    parlio_ll_tx_set_trans_bit_len(&PARL_IO, seg.byte_len * 8);
    gdma_reset(_dma_chan);
    esp_rom_delay_us(1);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_enable_clock(&PARL_IO, false);
    }
#else
    parlio_ll_tx_enable_clock(&PARL_IO, false);
#endif

    parlio_ll_tx_reset_fifo(&PARL_IO);
    parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_MASK);
    gdma_start(_dma_chan, (intptr_t)&_dmadesc_a[seg.start_idx]);

    uint32_t ready_wait = 100000;
    while (!parlio_ll_tx_is_ready(&PARL_IO) && --ready_wait) {
    }

    if (!parlio_ll_tx_is_ready(&PARL_IO)) {
        _frame_active = false;
        return;
    }

    parlio_ll_tx_start(&PARL_IO, true);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_enable_clock(&PARL_IO, true);
    }
#else
    parlio_ll_tx_enable_clock(&PARL_IO, true);
#endif
}

void IRAM_ATTR Bus_Parallel16::parlio_tx_isr(void *arg)
{
    auto *self = static_cast<Bus_Parallel16 *>(arg);
    const uint32_t status = parlio_ll_tx_get_interrupt_status(&PARL_IO);
    if (!(status & PARLIO_LL_EVENT_TX_EOF)) {
        return;
    }

    parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_EOF);
    parlio_ll_tx_start(&PARL_IO, false);

    portENTER_CRITICAL_ISR(&self->_spinlock);
    if (!self->_frame_active || !self->_segments_ptr || self->_seg_count == 0) {
        self->_frame_active = false;
        self->_retrigger_pending = false;
        portEXIT_CRITICAL_ISR(&self->_spinlock);
        return;
    }

    if (self->_seg_idx + 1 < self->_seg_count) {
        self->_seg_idx++;
        const segment_t seg = self->_segments_ptr[self->_seg_idx];
        self->start_segment_hw(seg);
        portEXIT_CRITICAL_ISR(&self->_spinlock);
        return;
    }

    if (self->_retrigger_pending) {
        self->_retrigger_pending = false;
        self->_seg_idx = 0;
        self->_frame_start_us = esp_timer_get_time();
        const segment_t seg = self->_segments_ptr[0];
        self->start_segment_hw(seg);
        portEXIT_CRITICAL_ISR(&self->_spinlock);
        return;
    }

    self->_frame_active = false;
    portEXIT_CRITICAL_ISR(&self->_spinlock);
}

void Bus_Parallel16::dma_transfer_start()
{
    if (!_initialized || !_dma_chan || !_dmadesc_a || !_segments_ptr || _seg_count == 0) {
        return;
    }

    bool need_stall_recovery = false;
    portENTER_CRITICAL(&_spinlock);
    if (_frame_active) {
        const int64_t elapsed = esp_timer_get_time() - _frame_start_us;
        if (elapsed > C6_DMA_STALL_TIMEOUT_US) {
            need_stall_recovery = true;
        } else {
            _retrigger_pending = true;
            portEXIT_CRITICAL(&_spinlock);
            return;
        }
    }

    if (!need_stall_recovery) {
        _frame_active = true;
        _retrigger_pending = false;
        _seg_idx = 0;
        _frame_start_us = esp_timer_get_time();
        const segment_t seg = _segments_ptr[0];
        parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_MASK);
        parlio_ll_enable_interrupt(&PARL_IO, PARLIO_LL_EVENT_TX_MASK, true);
        start_segment_hw(seg);
        portEXIT_CRITICAL(&_spinlock);
        return;
    }
    portEXIT_CRITICAL(&_spinlock);

    ESP_LOGW(TAG, "PARLIO frame stall detected - resetting and restarting");
    dma_transfer_stop();

    portENTER_CRITICAL(&_spinlock);
    _frame_active = true;
    _retrigger_pending = false;
    _seg_idx = 0;
    _frame_start_us = esp_timer_get_time();
    const segment_t seg = _segments_ptr[0];
    parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_MASK);
    parlio_ll_enable_interrupt(&PARL_IO, PARLIO_LL_EVENT_TX_MASK, true);
    start_segment_hw(seg);
    portEXIT_CRITICAL(&_spinlock);
}

void Bus_Parallel16::dma_transfer_stop()
{
    if (!_initialized) {
        return;
    }

    portENTER_CRITICAL(&_spinlock);
    _frame_active = false;
    _retrigger_pending = false;
    _seg_idx = 0;

    parlio_ll_enable_interrupt(&PARL_IO, PARLIO_LL_EVENT_TX_MASK, false);
    parlio_ll_clear_interrupt_status(&PARL_IO, PARLIO_LL_EVENT_TX_MASK);
    parlio_ll_tx_set_idle_data_value(&PARL_IO, HUB75_IDLE_OE_HIGH);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    PERIPH_RCC_ATOMIC() {
        parlio_ll_tx_enable_clock(&PARL_IO, false);
        parlio_ll_tx_reset_clock(&PARL_IO);
    }
#else
    parlio_ll_tx_enable_clock(&PARL_IO, false);
    parlio_ll_tx_reset_clock(&PARL_IO);
#endif

    if (_dma_chan) {
        gdma_stop(_dma_chan);
        gdma_reset(_dma_chan);
    }
    parlio_ll_tx_reset_fifo(&PARL_IO);
    parlio_ll_tx_start(&PARL_IO, false);
    portEXIT_CRITICAL(&_spinlock);
}

void Bus_Parallel16::set_dma_output_buffer(int buffer_id)
{
    (void)buffer_id; // Single-buffer only on ESP32-C6
}

#endif // CONFIG_IDF_TARGET_ESP32C6
