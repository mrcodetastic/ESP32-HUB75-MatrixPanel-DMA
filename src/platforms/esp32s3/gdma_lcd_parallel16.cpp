/*********************************************************************************************
  ESP32-S3 LCD Peripheral Driver for General-Purpose Parallel Data Output via GDMA.

  This driver utilizes the ESP32-S3's LCD peripheral in i8080 mode for high-speed parallel 
  data output to drive HUB75 LED matrices.
  
  This implementation is designed specifically for the ESP32-S3 SoC (not S2, C3, or original ESP32).

  CREDIT & ACKNOWLEDGMENTS:
  Full credit goes to Adafruit and Phil "PaintYourDragon" Burgess for their pioneering work on 
  ESP32-S3 LCD peripheral hacking in the Adafruit Protomatter library.
  
  References:
  - Adafruit Protomatter: https://github.com/adafruit/Adafruit_Protomatter
  - Technical Insights: https://blog.adafruit.com/2022/06/21/esp32uesday-more-s3-lcd-peripheral-hacking-with-code/

  Please support Adafruit and open-source hardware development!
 ********************************************************************************************/
 
#include <sdkconfig.h>
#if defined(CONFIG_IDF_TARGET_ESP32S3)
 
#if __has_include (<hal/lcd_ll.h>)
  #pragma message "Compiling for ESP32-S3"

  #ifdef ARDUINO_ARCH_ESP32
     #include <Arduino.h>
  #endif

  #include "gdma_lcd_parallel16.hpp"
  #include "esp_attr.h"
  #include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)		
  #include "esp_private/gpio.h"
#endif  

  static const char* TAG = "Bus_Parallel16_S3";

  /**
   * @brief Accesses the hardware LCD_CAM peripheral device register structure.
   */
  lcd_cam_dev_t* getDev()
  {
    return &LCD_CAM;
  }

  void Bus_Parallel16::config(const config_t& cfg)
  {
    _cfg = cfg;
    _dev = getDev();
  }

  /**
   * @brief Initializes the LCD_CAM peripheral, GDMA engine, GPIO mappings, and clock routing.
   */
  bool Bus_Parallel16::init(void)
  {
    // The LCD_CAM peripheral is disabled by default upon boot and must be explicitly enabled.
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Issue a hardware reset to the LCD bus state machine
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(1000);

    // Configure the LCD peripheral source clock:
    // Select LCD module source clock: 0=Disabled, 1=XTAL_CLK, 2=PLL_D2_CLK, 3=PLL_F160M_CLK
    LCD_CAM.lcd_clock.lcd_clk_sel = 3;        // Use 160 MHz Clock Source (PLL_F160M_CLK)
    
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;    // PCLK driven LOW during the first half cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;   // PCLK remains LOW when idle
    
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1;       // Primary clock counter divider (must never be 0)
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1; // PCLK = CLK / 1 (maintains 160 MHz base clock)

    /*
     * PSRAM Throughput Considerations:
     * When fetching DMA payloads directly from External PSRAM (SPIRAM), GDMA bandwidth 
     * is shared round-robin with CPU instruction/data caches. Slower Quad-SPI (QSPI) 
     * PSRAM requires a larger clock divisor (e.g., 8 MHz) to prevent FIFO underflows. 
     * Faster Octal-SPI (OPI) PSRAM can sustain up to ~13-16 MHz output cleanly.
     */
    bool psram_clkspeed_limit = false;
#if defined(SPIRAM_DMA_BUFFER)
    psram_clkspeed_limit = true;
#endif	 
     
    if (psram_clkspeed_limit) 
    {
        ESP_LOGI(TAG, "DMA buffer is located in PSRAM. Enforcing clock speed limits for bus stability...");   
        // Divisor 12 yields ~13.3 MHz output, balancing PSRAM bandwidth with CPU peripheral access.
        LCD_CAM.lcd_clock.lcd_clkm_div_num = 12; 
    }
    else
    {
      auto freq = (_cfg.bus_freq);
      auto _div_num = 16; // Default to 10 MHz output (160 MHz / 16)
      if (freq <= 10000000L) {      
            _div_num = 16; // 10 MHz
      } else if (freq < 20000000L) {
            _div_num = 10; // 16 MHz
      } else {
            _div_num = 7;  // ~22.8 MHz (High speed: requires short, clean signal wiring)
      }     
	  
#if defined(S3_LCD_DIV_NUM)      
      _div_num = S3_LCD_DIV_NUM; // User override via build flag
#endif      

      LCD_CAM.lcd_clock.lcd_clkm_div_num = _div_num;     
    }

    ESP_LOGI(TAG, "LCD_CAM clock divider configured to %d", (int)LCD_CAM.lcd_clock.lcd_clkm_div_num);
    ESP_LOGD(TAG, "Resulting output pixel clock frequency: %d MHz", (int)(160000000L / LCD_CAM.lcd_clock.lcd_clkm_div_num)); 

    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;     // Fractional divide denominator (0/1)
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;     // Fractional divide numerator

    // Frame format configuration:
    // Configures the LCD peripheral for generic parallel data streaming rather than panel display output.
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;    // Enable i8080 mode (disable RGB mode)
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0; // Bypass RGB/YUV color converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;  // Disable auto-framing
    LCD_CAM.lcd_misc.lcd_bk_en = 1;          // Enable backlight/blanking control
    
    LCD_CAM.lcd_data_dout_mode.val = 0;      // Zero data output delay
    LCD_CAM.lcd_user.lcd_always_out_en = 1;  // Enable continuous "always out" streaming mode
    LCD_CAM.lcd_user.lcd_8bits_order = 0;    // Standard byte ordering (no swap)
    LCD_CAM.lcd_user.lcd_bit_order = 0;      // Standard bit ordering (no reverse)
    LCD_CAM.lcd_user.lcd_2byte_en = 1;       // 16-bit word mode (2 bytes per cycle)
    
    /*
     * Dummy Phase Explanation:
     * Initial dummy clock cycles are required at the start of an LCD transfer to reliably 
     * trigger GDMA output. Without dummy cycles enabled, the TX FIFO must be kept manually 
     * primed, which adds unnecessary runtime complexity.
     */
    LCD_CAM.lcd_user.lcd_dummy = 1;          // Enable dummy phase at transaction start
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 1; // Set dummy phase length to 2 clock cycles (1 + 1)
    LCD_CAM.lcd_user.lcd_cmd = 0;            // No command phase at start

    // Route parallel data signals to designated GPIO pins
    int8_t* pins = _cfg.pin_data;  

    for (int i = 0; i < 16; i++) 
    {
      if (pins[i] >= 0) { // Note: Unassigned pins set to -1 are safely ignored
        esp_rom_gpio_connect_out_signal(pins[i], LCD_DATA_OUT0_IDX + i, false, false);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)		
        gpio_func_sel((gpio_num_t)pins[i], PIN_FUNC_GPIO);
#else
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pins[i]], PIN_FUNC_GPIO);
#endif		
        gpio_set_drive_capability((gpio_num_t)pins[i], (gpio_drive_cap_t)3); // High drive strength   
      }
    }

    // Connect Write Clock (PCLK) to GPIO pin
    esp_rom_gpio_connect_out_signal(_cfg.pin_wr, LCD_PCLK_IDX, _cfg.invert_pclk, false);
	  
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    gpio_func_sel((gpio_num_t)_cfg.pin_wr, PIN_FUNC_GPIO);
#else	  
    gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[_cfg.pin_wr], PIN_FUNC_GPIO);
#endif  
    gpio_set_drive_capability((gpio_num_t)_cfg.pin_wr, (gpio_drive_cap_t)3);  

    // Allocate GDMA channel and attach it to the LCD_CAM peripheral
    static gdma_channel_alloc_config_t dma_chan_config = {
      .sibling_chan = NULL,
      .direction = GDMA_CHANNEL_DIRECTION_TX,
      .flags = { .reserve_sibling = 0 }
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    esp_err_t err = gdma_new_ahb_channel(&dma_chan_config, &dma_chan);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to allocate AHB GDMA channel: %s", esp_err_to_name(err));
      return false;
    }
#else
    gdma_new_channel(&dma_chan_config, &dma_chan);
#endif

    gdma_connect(dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    
    static gdma_strategy_config_t strategy_config = {
      .owner_check = false,
      .auto_update_desc = false
    };
    gdma_apply_strategy(dma_chan, &strategy_config);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    gdma_transfer_config_t transfer_config = {
#ifdef SPIRAM_DMA_BUFFER
      .max_data_burst_size = 64,
      .access_ext_mem = true
#else
      .max_data_burst_size = 32,
      .access_ext_mem = false
#endif
    };
    gdma_config_transfer(dma_chan, &transfer_config);
#else
    gdma_transfer_ability_t ability = {
        .sram_trans_align = 32,
        .psram_trans_align = 64,
    };
    gdma_set_transfer_ability(dma_chan, &ability);
#endif

    // Wait for any prior peripheral operation to complete
    while (LCD_CAM.lcd_user.lcd_start); 

    // Reset GDMA channel and LCD hardware state machine to ensure a clean start
    gdma_reset(dma_chan);                 
    esp_rom_delay_us(1000);
     
    LCD_CAM.lcd_user.lcd_dout        = 1; // Enable parallel data output
    LCD_CAM.lcd_user.lcd_update      = 1; // Apply updated register settings
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1; // Reset LCD asynchronous TX FIFO

    return true; 
  }

  /**
   * @brief Releases allocated DMA memory descriptors and tears down peripheral bus instances.
   */
  void Bus_Parallel16::release(void)
  {
    if (_i80_bus) {
      esp_lcd_del_i80_bus(_i80_bus);
      _i80_bus = nullptr;
    }
    if (_dmadesc_a) {
      heap_caps_free(_dmadesc_a);
      _dmadesc_a = nullptr;
    }
    if (_dmadesc_b) {
      heap_caps_free(_dmadesc_b);
      _dmadesc_b = nullptr;
    }
    _dmadesc_count = 0;
    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;
  }

  void Bus_Parallel16::enable_double_dma_desc(void)
  {
    ESP_LOGI(TAG, "Enabled support for secondary (double buffered) DMA descriptors.");    
    _double_dma_buffer = true;
  }

  /**
   * @brief Allocates contiguous DMA descriptor arrays in DMA-capable memory.
   */
  bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
  {
    release(); // Free any existing allocations cleanly

    _dmadesc_count = len;
    ESP_LOGD(TAG, "Allocating %u descriptors (%u bytes per buffer).", 
             (unsigned int)len, (unsigned int)(sizeof(HUB75_DMA_DESCRIPTOR_T) * len));        

    _dmadesc_a = (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);
    if (_dmadesc_a == nullptr) {
      ESP_LOGE(TAG, "ERROR: Unable to allocate memory for Primary DMA Descriptor Buffer (A).");
      return false;
    }

    if (_double_dma_buffer) {
      _dmadesc_b = (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);
      if (_dmadesc_b == nullptr) {
        ESP_LOGE(TAG, "ERROR: Unable to allocate memory for Secondary DMA Descriptor Buffer (B). Rolling back.");
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
   * @brief Configures an individual DMA descriptor link within the chained linked-list.
   */
  void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
  {
    static constexpr size_t MAX_DMA_LEN = (4096 - 4); // Standard ESP32 hardware DMA chunk ceiling

    if (size > MAX_DMA_LEN) {
      size = MAX_DMA_LEN;
      ESP_LOGW(TAG, "Descriptor payload size exceeds hardware ceiling! Capped at MAX_DMA_LEN.");            
    }

    // Safety boundary check: Prevents heap corruption if descriptor generation exceeds allocation
    uint32_t current_idx = dmadesc_b ? _dmadesc_b_idx : _dmadesc_a_idx;
    if (current_idx >= _dmadesc_count) {
      ESP_LOGE(TAG, "Attempted to populate descriptor %u beyond allocated length %u (Buffer %c)",
               current_idx, _dmadesc_count, dmadesc_b ? 'B' : 'A');
      return;
    }

    HUB75_DMA_DESCRIPTOR_T* target_array = dmadesc_b ? _dmadesc_b : _dmadesc_a;
    HUB75_DMA_DESCRIPTOR_T* desc = &target_array[current_idx];
    bool is_last = (current_idx == (_dmadesc_count - 1));

    desc->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    desc->dw0.suc_eof = is_last ? 1 : 0;
    desc->dw0.size = size;
    desc->dw0.length = size;
    desc->buffer = data;

    // Link next descriptor pointer in chain, or wrap back to head [0] if at EOF
    if (is_last) {
      desc->next = (dma_descriptor_t*)&target_array[0]; 
      ESP_LOGV(TAG, "Final descriptor in Buffer %c created; linking back to head [0].", dmadesc_b ? 'B' : 'A');
    } else {
      desc->next = (dma_descriptor_t*)&target_array[current_idx + 1]; 
    }

    if (dmadesc_b) {
      _dmadesc_b_idx++;
    } else {
      _dmadesc_a_idx++;
    }
  }

  /**
   * @brief Begins hardware GDMA transmission to the LCD peripheral.
   */
  void Bus_Parallel16::dma_transfer_start()
  {
    gdma_start(dma_chan, (intptr_t)&_dmadesc_a[0]); // Point GDMA controller to head descriptor
    esp_rom_delay_us(100);                          // Allow pipeline to settle before triggering transmission
    LCD_CAM.lcd_user.lcd_start = 1;                 // Trigger LCD peripheral data transmission
  }

  /**
   * @brief Halts hardware GDMA transmission and resets peripheral flags.
   */
  void Bus_Parallel16::dma_transfer_stop()
  {
    LCD_CAM.lcd_user.lcd_reset = 1;        
    LCD_CAM.lcd_user.lcd_update = 1;        
    gdma_stop(dma_chan);   
  }

  /**
  * @brief Selects the repeating DMA linked-list loop for the requested framebuffer.
   */
  void Bus_Parallel16::set_dma_output_buffer(int back_buffer_id)
  {
    if (back_buffer_id == 1) {
       _dmadesc_b[_dmadesc_count - 1].next = (dma_descriptor_t*)&_dmadesc_b[0]; // Maintain Buffer B loop
       _dmadesc_a[_dmadesc_count - 1].next = (dma_descriptor_t*)&_dmadesc_b[0]; // Cross-link Buffer A -> Buffer B
    } else {
       _dmadesc_a[_dmadesc_count - 1].next = (dma_descriptor_t*)&_dmadesc_a[0]; // Maintain Buffer A loop
       _dmadesc_b[_dmadesc_count - 1].next = (dma_descriptor_t*)&_dmadesc_a[0]; // Cross-link Buffer B -> Buffer A
    }
  }

#endif
#endif