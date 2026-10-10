/*----------------------------------------------------------------------------/
  Parallel I2S DMA Bus Driver for ESP32 and ESP32-S2 SoCs.

  Based on LovyanGFX parallel I2S engine and Sprite_TM's original parallel 
  I2S hack, heavily modified for HUB75 LED matrix driving by mrcodetastic.

  Refactor Improvements:
  - Preserved original peripheral register documentation and timing notes.
  - Standardized dual-buffer safety bounds checking to prevent heap corruption.
  - Polished code formatting and inline explanation blocks for hardware registers.
/----------------------------------------------------------------------------*/
#include <sdkconfig.h>
#if defined (CONFIG_IDF_TARGET_ESP32) || defined (CONFIG_IDF_TARGET_ESP32S2)

#include "esp32_i2s_parallel_dma.hpp"

#if defined (CONFIG_IDF_TARGET_ESP32S2)
  #pragma message "Compiling for ESP32-S2"
#else
  #pragma message "Compiling for original ESP32 (released 2016)"  
#endif


#include <sdkconfig.h>
#include <esp_idf_version.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>

// Version-safe peripheral control header
#if (ESP_IDF_VERSION_MAJOR >= 5)
#include <esp_private/periph_ctrl.h>
#else
#include <driver/periph_ctrl.h>
#endif

#include <soc/gpio_sig_map.h>

// Version-safe I2S header inclusion matching i2s_dma.cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include <driver/i2s_std.h>
#include <soc/i2s_reg.h>
#else
#include <soc/i2s_periph.h>
#endif

#if defined (ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif



#include <esp_err.h>
#include <esp_log.h>
#include <soc/rtc.h>

static const char* TAG = "Bus_Parallel16_I2S";

/**
 * @brief Retrieves the target I2S hardware peripheral device structure pointer.
 */
i2s_dev_t* getDev()
{
  #if defined (CONFIG_IDF_TARGET_ESP32S2)
    return &I2S0;
  #else
    return (ESP32_I2S_DEVICE == 0) ? &I2S0 : &I2S1;
  #endif
}

/**
 * @brief Configures output direction and drive strength for designated GPIO pins.
 */
void _gpio_pin_init(int pin)
{
    if (pin >= 0) {
      gpio_pad_select_gpio(pin);
      gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
      gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3); // Max drive strength     
    }
}

/**
 * @brief Computes required memory word alignment width based on parallel bus bit width.
 */
inline int i2s_parallel_get_memory_width(int port, int width) {
  switch(width) {
    case 8:
    #if !defined (CONFIG_IDF_TARGET_ESP32S2)   
      // Only I2S1 on legacy ESP32 supports single-byte (8-bit) parallel memory packing
      if (port == 1) return 1;
      else return 2;
    #else 
      return 1;
    #endif

    case 16:
      return 2;
    case 24:
      return 4;
    default:
      return -ESP_ERR_INVALID_ARG;
  }
}

void Bus_Parallel16::config(const config_t& cfg)
{
    ESP_LOGI(TAG, "Performing configuration for ESP32 / ESP32-S2 parallel bus");
    _cfg = cfg;
    _dev = getDev();
}

/**
 * @brief Initializes the I2S peripheral parallel LCD mode, clock prescalers, and DMA engine.
 */
bool Bus_Parallel16::init(void) 
{
    ESP_LOGI(TAG, "Performing DMA bus init() for ESP32 / ESP32-S2");

    if (_cfg.parallel_width != 16) {
      ESP_LOGE(TAG, "Error: Parallel bus width must be configured to 16 bits!");
      return false;
    }   
	
	#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
		// Peripheral module reset & enable (IDF < 6.0 only - auto-managed in 6.0+)
		if (ESP32_I2S_DEVICE == I2S_NUM_0) {
			periph_module_reset(PERIPH_I2S0_MODULE);
			periph_module_enable(PERIPH_I2S0_MODULE);
		} else {
			#if !defined (CONFIG_IDF_TARGET_ESP32S2)  
			periph_module_reset(PERIPH_I2S1_MODULE);
			periph_module_enable(PERIPH_I2S1_MODULE);
			#endif 
		}
	#endif	

    auto dev = _dev;
    volatile int iomux_signal_base;
    volatile int iomux_clock;

    // Peripheral module reset & enable
    if (ESP32_I2S_DEVICE == I2S_NUM_0) {

        iomux_clock = I2S0O_WS_OUT_IDX;

        switch(_cfg.parallel_width) {
          case 8:
          case 16:
            iomux_signal_base = I2S0O_DATA_OUT8_IDX;
            break;
          case 24:
            iomux_signal_base = I2S0O_DATA_OUT0_IDX;
            break;
          default:
            return false;
        }
    } 
    #if !defined (CONFIG_IDF_TARGET_ESP32S2)  
    else {

        iomux_clock = I2S1O_WS_OUT_IDX;

        switch(_cfg.parallel_width) {
          case 16:
            iomux_signal_base = I2S1O_DATA_OUT8_IDX;
            break;
          case 8:
          case 24:
            iomux_signal_base = I2S1O_DATA_OUT0_IDX;
            break;
          default:
            return false;
        }
    }
    #endif 

    int bus_width = _cfg.parallel_width;

    // GPIO pin setup
    _gpio_pin_init(_cfg.pin_rd); 
    _gpio_pin_init(_cfg.pin_wr); // Write clock pin
    _gpio_pin_init(_cfg.pin_rs); 

    int8_t* pins = _cfg.pin_data;  
    for (int i = 0; i < bus_width; i++) {
        _gpio_pin_init(pins[i]);
    }

    // Connect clock signal to write clock pin via GPIO matrix
    gpio_matrix_out(_cfg.pin_wr, iomux_clock, _cfg.invert_pclk, 0); 
  
    for (size_t i = 0; i < bus_width; i++) {
      if (pins[i] >= 0) {
        gpio_matrix_out(pins[i], iomux_signal_base + i, false, false);
      }
    }

    // Clock divider configuration
    unsigned int freq = _cfg.bus_freq;    
    dev->sample_rate_conf.val = 0;
    dev->sample_rate_conf.rx_bits_mod = bus_width;
    dev->sample_rate_conf.tx_bits_mod = bus_width;

#if defined (CONFIG_IDF_TARGET_ESP32S2) 
    /*
     * ESP32-S2 TRM Reference (Page 675):
     * In I2S LCD parallel mode accessing external RAM via EDMA, clock frequencies 
     * should remain <= 12.5 MHz for 9-16 bit parallel outputs to prevent DMA underflows.
     */
    dev->clkm_conf.clk_sel = 2;         // 160 MHz Source (PLL_160M_CLK)
    dev->clkm_conf.clkm_div_a = 1;      // Denominator
    dev->clkm_conf.clkm_div_b = 0;      // Numerator

    unsigned int _div_num = (freq > 8000000) ? 2 : 4; 
    dev->clkm_conf.clkm_div_num = _div_num;	 
    dev->clkm_conf.clk_en = 1;

    // TRM Note: I2S_TX_BCK_DIV_NUM must never be configured as 1 on ESP32-S2.
    dev->sample_rate_conf.rx_bck_div_num = 2;
    dev->sample_rate_conf.tx_bck_div_num = 2;   
#else  
    /*
     * Legacy ESP32 Clock Configuration:
     * In LCD master transmit mode, the WS clock frequency is half of f-bck.
     */
    dev->sample_rate_conf.tx_bck_div_num = 2; // Mandatory div 2 per ESP32 manual   
    dev->sample_rate_conf.rx_bck_div_num = 2;

    dev->clkm_conf.clka_en    = 0;            // Use 80 MHz system clock (PLL_D2_CLK)
    dev->clkm_conf.clkm_div_a = 1;      
    dev->clkm_conf.clkm_div_b = 0;      

    unsigned int _div_num = (freq > 8000000) ? 2 : 4; // 20 MHz or 10 MHz output
    ESP_LOGD(TAG, "I2S pll_d2_clock clkm_div_num set to: %u", _div_num);    		
    dev->clkm_conf.clkm_div_num = _div_num;  
#endif

    // I2S Configuration 2 Register Setup
    dev->conf2.val = 0;
    dev->conf2.lcd_en = 1;          // Enable LCD mode
    dev->conf2.lcd_tx_wrx2_en = 0; 
    dev->conf2.lcd_tx_sdx2_en = 0;    

    dev->conf.val = 0;   
    
#if defined (CONFIG_IDF_TARGET_ESP32S2)  
    dev->conf.tx_dma_equal = 1;     // S2 specific: Ensure equal length DMA buffers
    dev->conf.pre_req_en = 1;       // S2 specific: Enable I2S data pre-request
#endif

    // Setup DMA FIFO registers
    dev->fifo_conf.val = 0;  
    dev->fifo_conf.rx_data_num = 32; 
    dev->fifo_conf.tx_data_num = 32;  
    dev->fifo_conf.dscr_en     = 1;  // Enable descriptors for DMA transfers

#if !defined (CONFIG_IDF_TARGET_ESP32S2)  
    if (_cfg.parallel_width == 8) {
      dev->conf2.lcd_tx_wrx2_en = 1;  // Duplicate single byte output on half-clocks
    }

    if (_cfg.parallel_width == 24) {
      dev->fifo_conf.tx_fifo_mod = 3; // 32-bit linear loading
    } else {
      dev->fifo_conf.tx_fifo_mod = 1; // 16-bit single channel mode
    }
  
    dev->fifo_conf.rx_fifo_mod_force_en = 1;
    dev->fifo_conf.tx_fifo_mod_force_en = 1;
    
    dev->conf_chan.val = 0;
    dev->conf_chan.tx_chan_mod = 1;
    dev->conf_chan.rx_chan_mod = 1;
#endif  

    // Reset FIFO buffers
    dev->conf.rx_fifo_reset = 1;
#if defined (CONFIG_IDF_TARGET_ESP32S2)
    while(dev->conf.rx_fifo_reset_st); 
#endif

    dev->conf.rx_fifo_reset = 0;
    dev->conf.tx_fifo_reset = 1;

#if defined (CONFIG_IDF_TARGET_ESP32S2)
    while(dev->conf.tx_fifo_reset_st); 
#endif
    dev->conf.tx_fifo_reset = 0;

    // Reset DMA peripheral
    dev->lc_conf.in_rst = 1;
    dev->lc_conf.in_rst = 0;
    dev->lc_conf.out_rst = 1;
    dev->lc_conf.out_rst = 0;
    
    dev->lc_conf.ahbm_rst = 1;
    dev->lc_conf.ahbm_rst = 0;

    dev->in_link.val = 0;
    dev->out_link.val = 0;    

    // Reset device state machine
    dev->conf.rx_reset = 1;
    dev->conf.tx_reset = 1;
    dev->conf.rx_reset = 0;
    dev->conf.tx_reset = 0;  

    dev->conf1.val = 0;
    dev->conf1.tx_stop_en = 0; 
    dev->timing.val = 0;

    return true;
}

/**
 * @brief Frees allocated descriptor arrays in DMA memory.
 */
void Bus_Parallel16::release(void)
{
    if (_dmadesc_a) {
      heap_caps_free(_dmadesc_a);
      _dmadesc_a = nullptr;
    }
    if (_dmadesc_b) {
      heap_caps_free(_dmadesc_b);
      _dmadesc_b = nullptr;
    }
    _dmadesc_count = 0;
    _dmadesc_last = 0;
    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;
}

void Bus_Parallel16::enable_double_dma_desc(void)
{
    _double_dma_buffer = true;
}

/**
 * @brief Allocates linked-list descriptor memory from DMA-capable RAM caps.
 */
bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
{
    release(); 
    
    _dmadesc_count = len; 
    _dmadesc_last  = len - 1; 

    ESP_LOGI(TAG, "Allocating memory for %u DMA descriptors.", (unsigned int)len);    

    _dmadesc_a = (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);
    if (_dmadesc_a == nullptr) {
      ESP_LOGE(TAG, "ERROR: Unable to allocate Primary DMA Descriptor Buffer (A).");
      return false;
    }

    if (_double_dma_buffer) {
      _dmadesc_b = (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);
      if (_dmadesc_b == nullptr) {
        ESP_LOGE(TAG, "ERROR: Unable to allocate Secondary DMA Descriptor Buffer (B). Rolling back.");
        heap_caps_free(_dmadesc_a);
        _dmadesc_a = nullptr;
        _double_dma_buffer = false;
        return false;
      }
    }
    
    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;

    return true;
}

/**
 * @brief Configures a single hardware DMA linked-list descriptor link.
 */
void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
{
    static constexpr size_t MAX_DMA_LEN = (4096 - 4);

    if (size > MAX_DMA_LEN) {
      size = MAX_DMA_LEN;
      ESP_LOGW(TAG, "DMA descriptor payload size capped at MAX_DMA_LEN!");            
    }

    // Bounds safety guard: Prevents buffer overflow if descriptor allocation count is exceeded
    uint32_t current_idx = dmadesc_b ? _dmadesc_b_idx : _dmadesc_a_idx;
    if (current_idx >= _dmadesc_count) {
      ESP_LOGE(TAG, "Attempted to populate descriptor %u beyond allocated length %u (Buffer %c)",
               current_idx, _dmadesc_count, dmadesc_b ? 'B' : 'A');
      return;
    }

    HUB75_DMA_DESCRIPTOR_T* target_array = dmadesc_b ? _dmadesc_b : _dmadesc_a;
    volatile lldesc_t* dmadesc = &target_array[current_idx];
    bool is_last = (current_idx == _dmadesc_last);

    // If at EOF, link next pointer back to head [0] of the buffer
    volatile lldesc_t* next = is_last ? target_array : &target_array[current_idx + 1];

    if (is_last) {
      ESP_LOGW(TAG, "Final DMA descriptor [%u] populated; linking back to head [0].", current_idx);             
    } 

    dmadesc->size     = size;
    dmadesc->length   = size;
    dmadesc->buf      = (uint8_t*)data; 
    dmadesc->eof      = is_last ? 1 : 0;         
    dmadesc->sosf     = 0;         
    dmadesc->owner    = 1;         
    dmadesc->qe.stqe_next = (lldesc_t*)next;         
    dmadesc->offset   = 0;       

    if (dmadesc_b) {
      _dmadesc_b_idx++; 
    } else {
      _dmadesc_a_idx++; 
    }
}

/**
 * @brief Starts parallel I2S DMA transfers.
 */
void Bus_Parallel16::dma_transfer_start()
{
    auto dev = _dev;
    dev->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN; // Enable burst transfers
    dev->out_link.addr = (uint32_t)_dmadesc_a;                      // Point descriptor link to Buffer A
    dev->out_link.stop  = 0; 
    dev->out_link.start = 1;
    dev->conf.tx_start  = 1;
}

/**
 * @brief Stops parallel I2S DMA transfers.
 */
void Bus_Parallel16::dma_transfer_stop()
{
    auto dev = _dev;
    dev->out_link.stop = 1;
    dev->out_link.start = 0;
    dev->conf.tx_start = 0;
}

/**
 * @brief Flips the circular DMA link chain between Buffer A and Buffer B for double buffering.
 */
int Bus_Parallel16::flip_dma_output_buffer()
{
    if (!_double_dma_buffer || !_dmadesc_a || !_dmadesc_b) {
      return _draw_buffer_id;
    }

    const int displayed_buffer_id = _draw_buffer_id;

    // Ensure prior CPU writes to descriptor chain are committed to memory
    __asm__ __volatile__("" ::: "memory");

    if (displayed_buffer_id == 1) { 
      // Point EOF of Buffer B to repeat Buffer B, and transition Buffer A's EOF over to Buffer B
      _dmadesc_b[_dmadesc_last].qe.stqe_next = (lldesc_t*)&_dmadesc_b[0];  
      
      // Memory barrier between pointer updates
      __asm__ __volatile__("" ::: "memory");
      
      _dmadesc_a[_dmadesc_last].qe.stqe_next = (lldesc_t*)&_dmadesc_b[0]; 
    } else { 
      // Point EOF of Buffer A to repeat Buffer A, and transition Buffer B's EOF over to Buffer A
      _dmadesc_a[_dmadesc_last].qe.stqe_next = (lldesc_t*)&_dmadesc_a[0];

      // Memory barrier between pointer updates
      __asm__ __volatile__("" ::: "memory");

      _dmadesc_b[_dmadesc_last].qe.stqe_next = (lldesc_t*)&_dmadesc_a[0]; 
    }

    // Final memory barrier to guarantee visibility before returning
    __asm__ __volatile__("" ::: "memory");

    _draw_buffer_id ^= 1;
    return _draw_buffer_id;
}
#endif