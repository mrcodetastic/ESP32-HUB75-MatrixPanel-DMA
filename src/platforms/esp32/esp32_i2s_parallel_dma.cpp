/*----------------------------------------------------------------------------/
  Lovyan GFX - Graphics library for embedded devices.

Original Source:
 https://github.com/lovyan03/LovyanGFX/

Licence:
 [FreeBSD](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt)

Author:
 [lovyan03](https://twitter.com/lovyan03)

Contributors:
 [ciniml](https://github.com/ciniml)
 [mongonta0716](https://github.com/mongonta0716)
 [tobozo](https://github.com/tobozo)

Modified heavily for the ESP32 HUB75 DMA library by:
 [mrfaptastic](https://github.com/mrfaptastic)
  
/----------------------------------------------------------------------------*/
#include <sdkconfig.h>
#if defined (CONFIG_IDF_TARGET_ESP32) || defined (CONFIG_IDF_TARGET_ESP32S2)

#include "esp32_i2s_parallel_dma.hpp"

#if defined (CONFIG_IDF_TARGET_ESP32S2)
  #pragma message "Compiling for ESP32-S2"
#else
  #pragma message "Compiling for original ESP32 (released 2016)"  
#endif

#include <driver/gpio.h>
#if (ESP_IDF_VERSION_MAJOR == 5)
#include <esp_private/periph_ctrl.h>
#else
#include <driver/periph_ctrl.h>
#endif
#include <soc/gpio_sig_map.h>
#include <soc/i2s_periph.h> //includes struct and reg


#if defined (ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif

#include <esp_err.h>
#include <esp_log.h>

// Get CPU freq function.
#include <soc/rtc.h>


	volatile bool previousBufferFree = true;

	static void IRAM_ATTR i2s_isr(void* arg) {

		// From original Sprite_TM Code
		//REG_WRITE(I2S_INT_CLR_REG(1), (REG_READ(I2S_INT_RAW_REG(1)) & 0xffffffc0) | 0x3f);
		
		// Clear flag so we can get retriggered
		SET_PERI_REG_BITS(I2S_INT_CLR_REG(ESP32_I2S_DEVICE), I2S_OUT_EOF_INT_CLR_V, 1, I2S_OUT_EOF_INT_CLR_S);                      
		
		// at this point, the previously active buffer is free, go ahead and write to it
		previousBufferFree = true;
	}

	bool DRAM_ATTR i2s_parallel_is_previous_buffer_free() {
		return previousBufferFree;
	}


	// Static
	i2s_dev_t* getDev()
	{
	  #if defined (CONFIG_IDF_TARGET_ESP32S2)
		  return &I2S0;
	  #else
		  return (ESP32_I2S_DEVICE == 0) ? &I2S0 : &I2S1;
	  #endif

	}

	// Static
	void _gpio_pin_init(int pin)
	{
		if (pin >= 0)
		{
		  gpio_pad_select_gpio(pin);
		  //gpio_hi(pin);
		  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
		  gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3);      // esp32s3 as well?
		}
	}
  
	inline int i2s_parallel_get_memory_width(int port, int width) {
	  switch(width) {
		case 8:
		
		#if !defined (CONFIG_IDF_TARGET_ESP32S2)   
		
			  // Only I2S1 on the legacy ESP32 WROOM MCU supports space saving single byte 8 bit parallel access
			  if(port == 1)
			  {
				return 1;
			  } else {
				return 2;
			  }
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
      ESP_LOGI("ESP32/S2", "Performing config for ESP32 or ESP32-S2");
      _cfg = cfg;
      _dev = getDev();
  }
 
 bool Bus_Parallel16::init(void) // The big one that gets everything setup.
 {
    ESP_LOGI("ESP32/S2", "Performing DMA bus init() for ESP32 or ESP32-S2");

    /*
    if(_cfg.parallel_width < 8 || _cfg.parallel_width >= 24) {
      return false;
    }
    */

    // Only 16 bit mode used for this library
    if(_cfg.parallel_width != 16) {
      return false;
    }   

    auto dev = _dev;
    volatile int iomux_signal_base;
    volatile int iomux_clock;
    int irq_source;

    // Initialize I2S0 peripheral
    if (ESP32_I2S_DEVICE == I2S_NUM_0) 
    {
        periph_module_reset(PERIPH_I2S0_MODULE);
        periph_module_enable(PERIPH_I2S0_MODULE);

        iomux_clock = I2S0O_WS_OUT_IDX;
        irq_source = ETS_I2S0_INTR_SOURCE;

        switch(_cfg.parallel_width) {
          case 8:
          case 16:
            iomux_signal_base = I2S0O_DATA_OUT8_IDX;
            break;
          case 24:
            iomux_signal_base = I2S0O_DATA_OUT0_IDX;
            break;
          default:
            return ESP_ERR_INVALID_ARG;
        }
    } 

    #if !defined (CONFIG_IDF_TARGET_ESP32S2)  
    // Can't compile if I2S1 if it doesn't exist with that hardware's IDF.... 
    else {
        periph_module_reset(PERIPH_I2S1_MODULE);
        periph_module_enable(PERIPH_I2S1_MODULE);
        iomux_clock = I2S1O_WS_OUT_IDX;
        irq_source = ETS_I2S1_INTR_SOURCE;

        switch(_cfg.parallel_width) {
          case 16:
            iomux_signal_base = I2S1O_DATA_OUT8_IDX;
            break;
          case 8:
          case 24:
            iomux_signal_base = I2S1O_DATA_OUT0_IDX;
            break;
          default:
            return ESP_ERR_INVALID_ARG;
        }
    }
    #endif 

    // Setup GPIOs
    int bus_width = _cfg.parallel_width;

    // Clock output GPIO setup
    _gpio_pin_init(_cfg.pin_rd); // not used
    _gpio_pin_init(_cfg.pin_wr); // clock
    _gpio_pin_init(_cfg.pin_rs); // not used

    // Data output GPIO setup
    int8_t* pins = _cfg.pin_data;  

    for(int i = 0; i < bus_width; i++) 
     _gpio_pin_init(pins[i]);

    // Route clock signal to clock pin
    gpio_matrix_out(_cfg.pin_wr, iomux_clock, _cfg.invert_pclk, 0); // inverst clock if required
  
    for (size_t i = 0; i < bus_width; i++) {

      if (pins[i] >= 0) {
        gpio_matrix_out(pins[i], iomux_signal_base + i, false, false);
      }
    }

    ////////////////////////////// Clock configuration //////////////////////////////
    unsigned int freq 		= (_cfg.bus_freq); // requested freq    

    // Setup i2s clock (sample rate)
    dev->sample_rate_conf.val = 0;
    
    // Third stage config, width of data to be written to IO (I think this should always be the actual data width?)
    dev->sample_rate_conf.rx_bits_mod = bus_width;
    dev->sample_rate_conf.tx_bits_mod = bus_width;
        
    // Hard-code clock divider for ESP32-S2 to 8Mhz
    #if defined (CONFIG_IDF_TARGET_ESP32S2) 

		  // I2S_CLK_SEL Set this bit to select I2S module clock source. 
		  // 0: No clock. 1: APLL_CLK. 2: PLL_160M_CLK. 3: No clock. (R/W)
  		dev->clkm_conf.clk_sel = 2;         // 160 Mhz    
      
      dev->clkm_conf.clkm_div_a   = 1;      // Clock denominator 			
      dev->clkm_conf.clkm_div_b   = 0;      // Clock numerator

      // Output Frequency = (160Mhz / clkm_div_num) / (tx_bck_div_num*2)
		  unsigned int _div_num = (freq > 8000000) ? 2:4; // 20 mhz or 10mhz 

      /*
        Page 675 of ESP-S2 TRM.

        In LCD and camera modes:
          • Support to connect to external LCD, and configured as 8- ~ 24-bit parallel output mode.

          – I2S LCD accesses internal memory via DMA.
          * Clock frequency should be less than 40 MHz when LCD data bus is configured as 8- ~ 16-bit parallel output.
          * Clock frequency should be less than 26.7 MHz when LCD data bus is configured as 17- ~ 24-bit parallel output.
          * 
          – I2S LCD accesses external RAM via EDMA.
          * Clock frequency should be less than 25 MHz when LCD data bus is configured as 8-bit parallel output.
          * Clock frequency should be less than 12.5 MHz when LCD data bus is configured as 9- ~ 16-bit parallel output.
          * Clock frequency should be less than 6.25 MHz when LCD data bus is configured as 17- ~ 24-bit parallel output.

      */

      dev->clkm_conf.clkm_div_num = _div_num;	 
      dev->clkm_conf.clk_en  = 1;

      // Binary clock 
      /*
      In LCD mode, WS clock frequency is:
      fWS = fi2s / W ∗ 2

      W is an integer in the range 1 to 64. W corresponds to the value of I2S_TX_BCK_DIV_NUM[5:0] in register
      I2S_SAMPLE_RATE_CONF_REG as follows.
      • When I2S_TX_BCK_DIV_NUM[5:0] = 0, W = 64.
      • When I2S_TX_BCK_DIV_NUM[5:0] is set to other values, W = I2S_TX_BCK_DIV_NUM[5:0      

      */

      dev->sample_rate_conf.rx_bck_div_num = 2;

      // ESP32 and ESP32-S2 TRM clearly say that "Note that I2S_TX_BCK_DIV_NUM[5:0] must not be configured as 1." 
      // Testing has revealed that setting it to 1 causes problems on S2.      
      dev->sample_rate_conf.tx_bck_div_num = 2;   

      // Output Frequency is now
      //160 / 5 / (2*2) = 8Mhz


    // Calculate clock divider for Original ESP32
    #else  

    /*
      The clock configuration of the LCD master transmitting mode is identical to I2S’ clock configuration. 
      In the LCD mode, the frequency of WS is half of f-bck
    */

    // ESP32 and ESP32-S2 TRM clearly say that "Note that I2S_TX_BCK_DIV_NUM[5:0] must not be configured as 1." 
    dev->sample_rate_conf.tx_bck_div_num = 2;   
    dev->sample_rate_conf.rx_bck_div_num = 2;

  	dev->clkm_conf.clka_en    = 0;           // Use the 80mhz system clock (PLL_D2_CLK) when '0'
		dev->clkm_conf.clkm_div_a = 1;      // Clock denominator 
		dev->clkm_conf.clkm_div_b = 0;      // Clock numerator

    unsigned int _div_num = (freq > 8000000) ? 2:4; // 20 mhz or 10mhz
		ESP_LOGD("ESP32", "i2s pll_d2_clock clkm_div_num is: %u", _div_num);    		

    // Frequency will be (80Mhz / clkm_div_num / tx_bck_div_num (2))
		dev->clkm_conf.clkm_div_num = _div_num;  

   // Note tx_bck_div_num value of 2 will further divide clock rate
		
    #endif
   
    ////////////////////////////// END CLOCK CONFIGURATION /////////////////////////////////

    // I2S conf2 reg
    dev->conf2.val = 0;
    dev->conf2.lcd_en = 1;
    dev->conf2.lcd_tx_wrx2_en=0; 
    dev->conf2.lcd_tx_sdx2_en=0;    

    // I2S conf reg
    dev->conf.val = 0;   
    
  #if defined (CONFIG_IDF_TARGET_ESP32S2)  
    dev->conf.tx_dma_equal=1;  // esp32-s2 only
    dev->conf.pre_req_en=1;    // esp32-s2 only - enable I2S to prepare data earlier? wtf?
  #endif

    // Now start setting up DMA FIFO
    dev->fifo_conf.val = 0;  
    dev->fifo_conf.rx_data_num = 32; // Thresholds. 
    dev->fifo_conf.tx_data_num = 32;  
    dev->fifo_conf.dscr_en     = 1;  

  #if !defined (CONFIG_IDF_TARGET_ESP32S2)  

    // Enable "One datum will be written twice in LCD mode" - for some reason, 
    // if we don't do this in 8-bit mode, data is updated on half-clocks not clocks
    if(_cfg.parallel_width == 8)
      dev->conf2.lcd_tx_wrx2_en=1;  

    // Not really described for non-pcm modes, although datasheet states it should be set correctly even for LCD mode
    // First stage config. Configures how data is loaded into fifo
    if(_cfg.parallel_width == 24) {
      // Mode 0, single 32-bit channel, linear 32 bit load to fifo
      dev->fifo_conf.tx_fifo_mod = 3;
    } else {
      // Mode 1, single 16-bit channel, load 16 bit sample(*) into fifo and pad to 32 bit with zeros
      // *Actually a 32 bit read where two samples are read at once. Length of fifo must thus still be word-aligned
      dev->fifo_conf.tx_fifo_mod = 1;
    }
  
    // Dictated by ESP32 datasheet
    dev->fifo_conf.rx_fifo_mod_force_en = 1;
    dev->fifo_conf.tx_fifo_mod_force_en = 1;
    
    // Second stage config
    dev->conf_chan.val = 0;
    
    // 16-bit single channel data
    dev->conf_chan.tx_chan_mod = 1;
    dev->conf_chan.rx_chan_mod = 1;
      
  #endif  

    // Reset FIFO
    dev->conf.rx_fifo_reset = 1;
    
  #if defined (CONFIG_IDF_TARGET_ESP32S2)
    while(dev->conf.rx_fifo_reset_st); // esp32-s2 only
  #endif

    dev->conf.rx_fifo_reset = 0;
    dev->conf.tx_fifo_reset = 1;

  #if defined (CONFIG_IDF_TARGET_ESP32S2)
    while(dev->conf.tx_fifo_reset_st); // esp32-s2 only
  #endif
    dev->conf.tx_fifo_reset = 0;


    // Reset DMA
    dev->lc_conf.in_rst = 1;
    dev->lc_conf.in_rst = 0;
    dev->lc_conf.out_rst = 1;
    dev->lc_conf.out_rst = 0;
    
    dev->lc_conf.ahbm_rst = 1;
    dev->lc_conf.ahbm_rst = 0;

    dev->in_link.val = 0;
    dev->out_link.val = 0;    


    // Device reset
    dev->conf.rx_reset=1;
    dev->conf.tx_reset=1;
    dev->conf.rx_reset=0;
    dev->conf.tx_reset=0;  

    dev->conf1.val = 0;
    dev->conf1.tx_stop_en = 0; 
    dev->timing.val = 0;


    // If we have double buffering, then allocate an interrupt service routine function
    // that can be used for I2S0/I2S1 created interrupts.

    // Setup I2S Interrupt
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(ESP32_I2S_DEVICE), I2S_OUT_EOF_INT_ENA_V, 1, I2S_OUT_EOF_INT_ENA_S);

    // Allocate a level 1 intterupt: lowest priority, as ISR isn't urgent and may take a long time to complete
    esp_intr_alloc(irq_source, (int)(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1), i2s_isr, NULL, NULL);

 
  #if defined (CONFIG_IDF_TARGET_ESP32S2)
      ESP_LOGD("ESP32-S2", "init() GPIO and clock configuration set for ESP32-S2");    
  #else
      ESP_LOGD("ESP32-ORIG", "init() GPIO and clock configuration set for ESP32");    
  #endif

      return true;
  }


  void Bus_Parallel16::release(void)
  {
    if (_dmadesc_a)
    {
      heap_caps_free(_dmadesc_a);
      _dmadesc_a = nullptr;
      _dmadesc_count = 0;
    }

    if (_dmadesc_b)
    {
      heap_caps_free(_dmadesc_b);
      _dmadesc_b = nullptr;
      _dmadesc_count = 0;      
    }
  }

  void Bus_Parallel16::enable_double_dma_desc(void)
  {
    _double_dma_buffer = true;
  }

  // Need this to work for double buffers etc.
  bool Bus_Parallel16::allocate_dma_desc_memory(size_t len)
  {
    if (_dmadesc_a) heap_caps_free(_dmadesc_a); // free all dma descrptios previously
    
    _dmadesc_count = len; 
    _dmadesc_last  = len-1; 

    ESP_LOGI("ESP32/S2", "Allocating memory for %d DMA descriptors.", (int)len);    

    _dmadesc_a= (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);

    if (_dmadesc_a == nullptr)
    {
      ESP_LOGE("ESP32/S2", "ERROR: Couldn't malloc _dmadesc_a. Not enough memory.");
      return false;
    }


    if (_double_dma_buffer)
    {
      if (_dmadesc_b) heap_caps_free(_dmadesc_b); // free all dma descrptios previously

      ESP_LOGD("ESP32/S2", "Allocating the second buffer (double buffer enabled).");              

      _dmadesc_b = (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * len, MALLOC_CAP_DMA);

      if (_dmadesc_b == nullptr)
      {
        ESP_LOGE("ESP32/S2", "ERROR: Couldn't malloc _dmadesc_b. Not enough memory.");
        _double_dma_buffer = false;
        return false;
      }
    }
    
    _dmadesc_a_idx  = 0;
    _dmadesc_b_idx  = 0;

    ESP_LOGD("ESP32/S2", "Allocating %d bytes of memory for DMA descriptors.", (int)sizeof(HUB75_DMA_DESCRIPTOR_T) * len);       

    // New - Temporary blank descriptor for transitions between DMA buffer
    _dmadesc_blank = (HUB75_DMA_DESCRIPTOR_T*)heap_caps_malloc(sizeof(HUB75_DMA_DESCRIPTOR_T) * 1, MALLOC_CAP_DMA);
    _dmadesc_blank->size     = 1024*2;
    _dmadesc_blank->length   = 1024*2;
    _dmadesc_blank->buf      = (uint8_t*) _blank_data; 
    _dmadesc_blank->eof      = 1;         
    _dmadesc_blank->sosf     = 0;         
    _dmadesc_blank->owner    = 1;         
    _dmadesc_blank->qe.stqe_next = (lldesc_t*) _dmadesc_blank;         
    _dmadesc_blank->offset   = 0;         

    return true;

  }

  void Bus_Parallel16::create_dma_desc_link(void *data, size_t size, bool dmadesc_b)
  {
    static constexpr size_t MAX_DMA_LEN = (4096-4);

    if (size > MAX_DMA_LEN)
    {
      size = MAX_DMA_LEN;
      ESP_LOGW("ESP32/S2", "Creating DMA descriptor which links to payload with size greater than MAX_DMA_LEN!");            
    }

    if ( !dmadesc_b )
    {
      if ( (_dmadesc_a_idx+1) > _dmadesc_count) {
        ESP_LOGE("ESP32/S2", "Attempted to create more DMA descriptors than allocated memory for. Expecting a maximum of %u DMA descriptors", (unsigned int)_dmadesc_count);          
        return;
      }
    }

    volatile lldesc_t *dmadesc;
    volatile lldesc_t *next;
    bool eof = false;
    
    if ( (dmadesc_b == true) ) // for primary buffer
    {
        dmadesc      = &_dmadesc_b[_dmadesc_b_idx];

        next = (_dmadesc_b_idx < (_dmadesc_last) ) ? &_dmadesc_b[_dmadesc_b_idx+1]:_dmadesc_b;       
        eof  = (_dmadesc_b_idx == (_dmadesc_last));
    }
    else
    {
        dmadesc      = &_dmadesc_a[_dmadesc_a_idx];

        // https://stackoverflow.com/questions/47170740/c-negative-array-index
        next = (_dmadesc_a_idx < (_dmadesc_last) ) ? _dmadesc_a + _dmadesc_a_idx+1:_dmadesc_a;       
        eof  = (_dmadesc_a_idx == (_dmadesc_last));
    }

    if ( _dmadesc_a_idx == (_dmadesc_last) ) {
      ESP_LOGW("ESP32/S2", "Creating final DMA descriptor and linking back to 0.");             
    } 

    dmadesc->size     = size;
    dmadesc->length   = size;
    dmadesc->buf      = (uint8_t*) data; 
    dmadesc->eof      = eof;         
    dmadesc->sosf     = 0;         
    dmadesc->owner    = 1;         
    dmadesc->qe.stqe_next = (lldesc_t*) next;         
    dmadesc->offset   = 0;       

    if ( (dmadesc_b == true) ) { // for primary buffer
      _dmadesc_b_idx++; 
    } else {
      _dmadesc_a_idx++; 
    }
  
  } // end create_dma_desc_link

  void Bus_Parallel16::dma_transfer_start()
  {
    auto dev = _dev;
   
    // Configure DMA burst mode
    dev->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;

    // Set address of DMA descriptor, start with buffer 0 / 'a'
    dev->out_link.addr = (uint32_t) _dmadesc_a;
  
  // Start DMA operation
    dev->out_link.stop  = 0; 
    dev->out_link.start = 1;
    
    dev->conf.tx_start  = 1;

   
  } // end 
  

  void Bus_Parallel16::dma_transfer_stop()
  {
     auto dev = _dev;
     
    // Stop all ongoing DMA operations
    dev->out_link.stop = 1;
    dev->out_link.start = 0;
    dev->conf.tx_start = 0;
        
  } // end   


  void Bus_Parallel16::flip_dma_output_buffer(int buffer_id) // pass by reference so we can change in main matrixpanel class
  {
	  
      // Setup interrupt handler which is focussed only on the (page 322 of Tech. Ref. Manual)
      // "I2S_OUT_EOF_INT: Triggered when rxlink has finished sending a packet" (when dma linked list with eof = 1 is hit)
  	  
      if ( buffer_id == 1) { 

	      _dmadesc_a[_dmadesc_last].qe.stqe_next = &_dmadesc_b[0]; // Start sending out _dmadesc_b (or buffer 1)

        //fix _dmadesc_ loop issue #407
        //need to connect the up comming _dmadesc_ not the old one
        _dmadesc_b[_dmadesc_last].qe.stqe_next = &_dmadesc_b[0];
		      
      } else { 
	      
        _dmadesc_b[_dmadesc_last].qe.stqe_next = &_dmadesc_a[0]; 
        _dmadesc_a[_dmadesc_last].qe.stqe_next = &_dmadesc_a[0];
		
      }

      previousBufferFree = false;  
      //while (i2s_parallel_is_previous_buffer_free() == false) {}      
      while (!previousBufferFree);

           


  } // end flip



#endif
