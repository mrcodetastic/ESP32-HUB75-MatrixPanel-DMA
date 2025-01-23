/*********************************************************************************************
  Simple example of using the ESP32-S3's LCD peripheral for general-purpose
  (non-LCD) parallel data output with DMA. Connect 8 LEDs (or logic analyzer),
  cycles through a pattern among them at about 1 Hz.
  This code is ONLY for the ESP32-S3, NOT the S2, C3 or original ESP32.
  None of this is authoritative canon, just a lot of trial error w/datasheet
  and register poking. Probably more robust ways of doing this still TBD.


 FULL CREDIT goes to AdaFruit and  https://github.com/PaintYourDragon
 
 https://blog.adafruit.com/2022/06/21/esp32uesday-more-s3-lcd-peripheral-hacking-with-code/
 
 https://github.com/adafruit/Adafruit_Protomatter/blob/master/src/arch/esp32-s3.h

 PLEASE SUPPORT THEM!

 ********************************************************************************************/
#if __has_include (<hal/lcd_ll.h>)
// Stop compile errors: /src/platforms/esp32s3/gdma_lcd_parallel16.hpp:64:10: fatal error: hal/lcd_ll.h: No such file or directory
  #pragma message "Compiling for ESP32-S3"

  #ifdef ARDUINO_ARCH_ESP32
     #include <Arduino.h>
  #endif

  #include "gdma_lcd_parallel16.hpp"
  #include "esp_attr.h"
  #include "esp_idf_version.h"

/*
  dma_descriptor_t desc;          // DMA descriptor for testing

  uint8_t data[8][312];           // Transmit buffer (2496 bytes total)  
  uint16_t* dmabuff2;
*/

  DRAM_ATTR volatile bool previousBufferFree = true;
 
  // End-of-DMA-transfer callback
  IRAM_ATTR bool gdma_on_trans_eof_callback(gdma_channel_handle_t dma_chan,
                                    gdma_event_data_t *event_data, void *user_data) {
                                        
  // This DMA callback seems to trigger a moment before the last data has
  // issued (buffering between DMA & LCD peripheral?), so pause a moment
  // before stopping LCD data out. The ideal delay may depend on the LCD
  // clock rate...this one was determined empirically by monitoring on a
  // logic analyzer. YMMV.
    esp_rom_delay_us(100);        
  // The LCD peripheral stops transmitting at the end of the DMA xfer, but
  // clear the lcd_start flag anyway -- we poll it in loop() to decide when
  // the transfer has finished, and the same flag is set later to trigger
  // the next transfer.

    //LCD_CAM.lcd_user.lcd_start = 0;
    
    previousBufferFree = true;
    
    return true;
  }  

  lcd_cam_dev_t* getDev()
  {
    return &LCD_CAM;
  }

  // ------------------------------------------------------------------------------

  void Bus_Parallel16::config(const config_t& cfg)
  {
    _cfg = cfg;
    //auto port = cfg.port;
    _dev = getDev();
  }


 //https://github.com/adafruit/Adafruit_Protomatter/blob/master/src/arch/esp32-s3.h
 bool Bus_Parallel16::init(void)
 {
    ///dmabuff2 = (uint16_t*)heap_caps_malloc(sizeof(uint16_t) * 64*32, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // LCD_CAM peripheral isn't enabled by default -- MUST begin with this:
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Reset LCD bus
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(1000);

//    uint32_t lcd_clkm_div_num = ((160000000 + 1) / _cfg.bus_freq);
//    ESP_LOGI("", "Clock divider is %d", lcd_clkm_div_num);     

    // Configure LCD clock. Since this program generates human-perceptible
    // output and not data for LED matrices or NeoPixels, use almost the
    // slowest LCD clock rate possible. The S3-mini module used on Feather
    // ESP32-S3 has a 40 MHz crystal. A 2-stage clock division of 1:16000
    // is applied (250*64), yielding 2,500 Hz. Still much too fast for
    // human eyes, so later we set up the data to repeat each output byte
    // many times over.
    //LCD_CAM.lcd_clock.clk_en = 0;             // Enable peripheral clock

    // LCD_CAM_LCD_CLK_SEL Select LCD module source clock. 0: clock source is disabled. 1: XTAL_CLK. 2: PLL_D2_CLK. 3: PLL_F160M_CLK. (R/W)
    LCD_CAM.lcd_clock.lcd_clk_sel = 3;        // Use 160Mhz Clock Source -> PLL_F160M_CLK
    
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;    // PCLK low in 1st half cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;   // PCLK low idle
    
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1; // Should never be zero
    
    //LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 0; // PCLK = CLK / (CLKCNT_N+1)    
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1; // PCLK = CLK / 1 (... so 160Mhz still)


    // https://esp32.com/viewtopic.php?f=5&t=24459&start=80#p94487
    /* Re: ESP32-S3 LCD and I2S FULL documentation 
     *  by ESP_Sprite » Fri Mar 25, 2022 2:06 am
     *
     *  Are you sure you are staying within the limits of the psram throughput? If GDMA can't fetch data fast 
     *  enough it leads to corruption. Also keep in mind that worst case scenario, the gdma can only use half of 
     *  the bandwidth of the psram peripheral (as it's round-robin shared with the CPUs).
     */ 
     
    // Fastest speed I can get with Octoal PSRAM to work before nothing shows. Based on manual testing.
    // If using an ESP32-S3 with slower (half the bandwidth) Q-SPI (Quad), then the divisor will need to be '20' (8Mhz) which wil be flickery! 
    if (_cfg.psram_clk_override) 
    {
        ESP_LOGI("S3", "DMA buffer is on PSRAM. Limiting clockspeed....");   
        //LCD_CAM.lcd_clock.lcd_clkm_div_num = 10; //16mhz is the fasted the Octal PSRAM can support it seems from faptastic's testing using an N8R8 variant (Octal SPI PSRAM).
        
        // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/441#issuecomment-1513631890
		// and 
		// https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/442
        LCD_CAM.lcd_clock.lcd_clkm_div_num = 12; // 13Mhz is the fastest output from PSRAM if we want to be able to service other peripherals as well.
    }
    else
    {

#if defined(S3_LCD_DIV_NUM)      
      auto _div_num = S3_LCD_DIV_NUM;
#else      
      auto  freq     = (_cfg.bus_freq);
      auto  _div_num = 16; // 10Mhz 
      if (freq <= 10000000L) {      
      } else if (freq < 20000000L) {
            _div_num = 10; // 16Mhz
      } else {
            _div_num = 7; // 22Mhz --- likely to have noise without a good connection         
      }     

#endif      
      LCD_CAM.lcd_clock.lcd_clkm_div_num = _div_num;     

    }

    ESP_LOGI("S3", "Clock divider is %d", (int)LCD_CAM.lcd_clock.lcd_clkm_div_num);
    ESP_LOGD("S3", "Resulting output clock frequency: %d Mhz",  (int)(160000000L/LCD_CAM.lcd_clock.lcd_clkm_div_num)); 


    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;     // 0/1 fractional divide
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
    
    // See section 26.3.3.1 of the ESP32­S3 Technical Reference Manual
    // for information on other clock sources and dividers.

    // Configure LCD frame format. This is where we fiddle the peripheral
    // to provide generic 8-bit output rather than actually driving an LCD.
    // There's also a 16-bit mode but that's not shown here.
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;    // i8080 mode (not RGB)
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0; // Disable RGB/YUV converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;  // Do NOT auto-frame

    LCD_CAM.lcd_misc.lcd_bk_en = 1;          // https://esp32.com/viewtopic.php?t=24459&start=60#p91835
    
    LCD_CAM.lcd_data_dout_mode.val = 0;      // No data delays
    LCD_CAM.lcd_user.lcd_always_out_en = 1;  // Enable 'always out' mode
    LCD_CAM.lcd_user.lcd_8bits_order = 0;    // Do not swap bytes
    LCD_CAM.lcd_user.lcd_bit_order = 0;      // Do not reverse bit order
    LCD_CAM.lcd_user.lcd_2byte_en = 1;       // 8-bit data mode
    LCD_CAM.lcd_user.lcd_dummy = 1;          // Dummy phase(s) @ LCD start
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 1; // 1+1 dummy phase
    LCD_CAM.lcd_user.lcd_cmd = 0;            // No command at LCD start
    // "Dummy phases" are initial LCD peripheral clock cycles before data
    // begins transmitting when requested. After much testing, determined
    // that at least one dummy phase MUST be enabled for DMA to trigger
    // reliably. A problem with dummy phase(s) is if we're also using the
    // LCD_PCLK_IDX signal (not used in this code, but Adafruit_Protomatter
    // does)...the clock signal will start a couple of pulses before data,
    // which may or may not be problematic in some situations. You can
    // disable the dummy phase but need to keep the LCD TX FIFO primed
    // in that case, which gets complex.
    // always_out_en is set above to allow aribtrary-length transfers,
    // else lcd_dout_cyclelen is used...but is limited to 8K. Long (>4K)
    // transfers need DMA linked lists, not used here but mentioned later.

    // Route 8 LCD data signals to GPIO pins
    int8_t* pins = _cfg.pin_data;  

    for(int i = 0; i < 16; i++) 
    {
      if (pins[i] >= 0) { // -1 value will CRASH the ESP32!
        esp_rom_gpio_connect_out_signal(pins[i], LCD_DATA_OUT0_IDX + i, false, false);
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pins[i]], PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)pins[i], (gpio_drive_cap_t)3);    
      }
    }

    /*
    const struct {
      int8_t  pin;
      uint8_t signal;
    } mux[] = {
      { 43,   LCD_DATA_OUT0_IDX }, // These are 8 consecutive pins down one
      { 42,   LCD_DATA_OUT1_IDX }, // side of the ESP32-S3 Feather. The ESP32
      { 2,   LCD_DATA_OUT2_IDX }, // has super flexible pin MUX capabilities,
      { 9,   LCD_DATA_OUT3_IDX }, // so any signal can go to any pin!
      { 10,  LCD_DATA_OUT4_IDX },
      { 11,  LCD_DATA_OUT5_IDX },
      { 12,  LCD_DATA_OUT6_IDX },
      { 13,  LCD_DATA_OUT7_IDX },
    };
    for (int i = 0; i < 8; i++) {
      esp_rom_gpio_connect_out_signal(mux[i].pin, LCD_DATA_OUT0_IDX + i, false, false);
      gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[mux[i].pin], PIN_FUNC_GPIO);
      gpio_set_drive_capability((gpio_num_t)mux[i].pin, (gpio_drive_cap_t)3);
    }
*/
    // Clock
      esp_rom_gpio_connect_out_signal(_cfg.pin_wr,  LCD_PCLK_IDX, _cfg.invert_pclk, false);
      gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[_cfg.pin_wr], PIN_FUNC_GPIO);
      gpio_set_drive_capability((gpio_num_t)_cfg.pin_wr, (gpio_drive_cap_t)3);  

    // This program has a known fixed-size data buffer (2496 bytes) that fits
    // in a single DMA descriptor (max 4095 bytes). Large transfers would
    // require a linked list of descriptors, but here it's just one...

/*
    desc.dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    desc.dw0.suc_eof = 0; // Last descriptor
    desc.next = &desc;     // No linked list
*/
   
    // Remaining descriptor elements are initialized before each DMA transfer.

    // Allocate DMA channel and connect it to the LCD peripheral
    static gdma_channel_alloc_config_t dma_chan_config = {
      .sibling_chan = NULL,
      .direction = GDMA_CHANNEL_DIRECTION_TX,
      .flags = {
        .reserve_sibling = 0
      }
    };
    gdma_new_channel(&dma_chan_config, &dma_chan);
    gdma_connect(dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    static gdma_strategy_config_t strategy_config = {
      .owner_check = false,
      .auto_update_desc = false
    };
    gdma_apply_strategy(dma_chan, &strategy_config);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
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

    // Enable DMA transfer callback
    static gdma_tx_event_callbacks_t tx_cbs = {
       // .on_trans_eof is literally the only gdma tx event type available
      .on_trans_eof = gdma_on_trans_eof_callback 
    };
    gdma_register_tx_event_callbacks(dma_chan, &tx_cbs, NULL);


    // This uses a busy loop to wait for each DMA transfer to complete...
    // but the whole point of DMA is that one's code can do other work in
    // the interim. The CPU is totally free while the transfer runs!
    while (LCD_CAM.lcd_user.lcd_start); // Wait for DMA completion callback

    // After much experimentation, each of these steps is required to get
    // a clean start on the next LCD transfer:
    gdma_reset(dma_chan);                 // Reset DMA to known state
    esp_rom_delay_us(1000);
     
    LCD_CAM.lcd_user.lcd_dout        = 1; // Enable data out
    LCD_CAM.lcd_user.lcd_update      = 1; // Update registers
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1; // Reset LCD TX FIFO


    return true; // no return val = illegal instruction
 }


  void Bus_Parallel16::release(void)
  {
    if (_i80_bus)
    {
      esp_lcd_del_i80_bus(_i80_bus);
      _i80_bus = nullptr;
    }
    if (_dmadesc_a)
    {
      heap_caps_free(_dmadesc_a);
      _dmadesc_a = nullptr;
      _dmadesc_count = 0;
    }

  }

  void Bus_Parallel16::enable_double_dma_desc(void)
  {
        ESP_LOGI("S3", "Enabled support for secondary DMA buffer.");    
       _double_dma_buffer = true;
  }

  // Need this to work for double buffers etc.
  bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
  {
    if (_dmadesc_a) heap_caps_free(_dmadesc_a); // free all dma descrptios previously
    _dmadesc_count = len;

    ESP_LOGD("S3", "Allocating %d bytes memory for DMA descriptors.", (int)sizeof(HUB75_DMA_DESCRIPTOR_T) * len);        

    _dmadesc_a= (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);
  
    if (_dmadesc_a == nullptr)
    {
      ESP_LOGE("S3", "ERROR: Couldn't malloc _dmadesc_a. Not enough memory.");
      return false;
    }

    if (_double_dma_buffer)
    {
      _dmadesc_b= (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);
    
      if (_dmadesc_b == nullptr)
      {
        ESP_LOGE("S3", "ERROR: Couldn't malloc _dmadesc_b. Not enough memory.");
        _double_dma_buffer = false;
      }
    }

    /// override static 
    _dmadesc_a_idx = 0;
    _dmadesc_b_idx = 0;

    return true;

  }

  void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
  {
    static constexpr size_t MAX_DMA_LEN = (4096-4);

    if (size > MAX_DMA_LEN) {
      size = MAX_DMA_LEN;
      ESP_LOGW("S3", "Creating DMA descriptor which links to payload with size greater than MAX_DMA_LEN!");            
    }

    if ( dmadesc_b == true)
    {

      _dmadesc_b[_dmadesc_b_idx].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
      //_dmadesc_b[_dmadesc_b_idx].dw0.suc_eof = 0;   
      _dmadesc_b[_dmadesc_b_idx].dw0.suc_eof = (_dmadesc_b_idx == (_dmadesc_count-1));
      _dmadesc_b[_dmadesc_b_idx].dw0.size = _dmadesc_b[_dmadesc_b_idx].dw0.length = size; //sizeof(data);
      _dmadesc_b[_dmadesc_b_idx].buffer = data; //data;

      if (_dmadesc_b_idx == _dmadesc_count-1) {
          _dmadesc_b[_dmadesc_b_idx].next =  (dma_descriptor_t *) &_dmadesc_b[0]; 
      }
      else {
          _dmadesc_b[_dmadesc_b_idx].next =  (dma_descriptor_t *) &_dmadesc_b[_dmadesc_b_idx+1]; 
      }

      _dmadesc_b_idx++;


    }
    else
    {
        
      if ( _dmadesc_a_idx >= _dmadesc_count)
      {
        ESP_LOGE("S3", "Attempted to create more DMA descriptors than allocated. Expecting max %u descriptors.", (unsigned int)_dmadesc_count);          
        return;
      }

      _dmadesc_a[_dmadesc_a_idx].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
      //_dmadesc_a[_dmadesc_a_idx].dw0.suc_eof = 0;
      _dmadesc_a[_dmadesc_a_idx].dw0.suc_eof = (_dmadesc_a_idx == (_dmadesc_count-1));    
      _dmadesc_a[_dmadesc_a_idx].dw0.size = _dmadesc_a[_dmadesc_a_idx].dw0.length = size; //sizeof(data);
      _dmadesc_a[_dmadesc_a_idx].buffer = data; //data;

      if (_dmadesc_a_idx == _dmadesc_count-1) {
          _dmadesc_a[_dmadesc_a_idx].next =  (dma_descriptor_t *) &_dmadesc_a[0]; 
      }
      else {
          _dmadesc_a[_dmadesc_a_idx].next =  (dma_descriptor_t *) &_dmadesc_a[_dmadesc_a_idx+1]; 
      }

      _dmadesc_a_idx++;


    }

  } // end create_dma_desc_link

  void Bus_Parallel16::dma_transfer_start()
  {
    gdma_start(dma_chan, (intptr_t)&_dmadesc_a[0]); // Start DMA w/updated descriptor(s)
    esp_rom_delay_us(100);              // Must 'bake' a moment before...
    LCD_CAM.lcd_user.lcd_start = 1;        // Trigger LCD DMA transfer
    
  } // end 

  void Bus_Parallel16::dma_transfer_stop()
  {

        LCD_CAM.lcd_user.lcd_reset = 1;        // Trigger LCD DMA transfer
        LCD_CAM.lcd_user.lcd_update = 1;        // Trigger LCD DMA transfer

        gdma_stop(dma_chan);   
        
  } // end   


  void Bus_Parallel16::flip_dma_output_buffer(int back_buffer_id)
  {

   // if ( _double_dma_buffer == false) return;

    if ( back_buffer_id == 1) // change across to everything 'b''
    {
       _dmadesc_a[_dmadesc_count-1].next =  (dma_descriptor_t *) &_dmadesc_b[0];       
       _dmadesc_b[_dmadesc_count-1].next =  (dma_descriptor_t *) &_dmadesc_b[0];       
    }
    else
    {
       _dmadesc_b[_dmadesc_count-1].next =  (dma_descriptor_t *) &_dmadesc_a[0];       
       _dmadesc_a[_dmadesc_count-1].next =  (dma_descriptor_t *) &_dmadesc_a[0];   
    }

    //current_back_buffer_id ^= 1;
    
    previousBufferFree = false;  
    
    //while (i2s_parallel_is_previous_buffer_free() == false) {}      
    while (!previousBufferFree);

        

    
  } // end flip


#endif
