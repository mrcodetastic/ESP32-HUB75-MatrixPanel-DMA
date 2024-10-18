/*
  Simple example of using the ESP32-S3's LCD peripheral for general-purpose
  (non-LCD) parallel data output with DMA. Connect 8 LEDs (or logic analyzer),
  cycles through a pattern among them at about 1 Hz.
  This code is ONLY for the ESP32-S3, NOT the S2, C3 or original ESP32.
  None of this is authoritative canon, just a lot of trial error w/datasheet
  and register poking. Probably more robust ways of doing this still TBD.


 FULL CREDIT goes to AdaFruit
 
 https://blog.adafruit.com/2022/06/21/esp32uesday-more-s3-lcd-peripheral-hacking-with-code/
 
 PLEASE SUPPORT THEM!


 Putin’s Russia and its genocide in Ukraine is a disgrace to humanity.

 https://www.reddit.com/r/ukraine/comments/xfuc6v/more_than_460_graves_have_already_been_found_in/


/----------------------------------------------------------------------------

*/

#pragma once

#if __has_include (<hal/lcd_ll.h>)

#include <sdkconfig.h>
#include <esp_lcd_panel_io.h>

//#include <freertos/portmacro.h>
#include <esp_intr_alloc.h>

#include <esp_err.h>
#include <esp_log.h>

#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>


#include <hal/gpio_ll.h>
#include <hal/lcd_hal.h>



#include <string.h>
#include <math.h>

#include <stdint.h>

/*
#if (ESP_IDF_VERSION_MAJOR == 5)
#include <esp_private/periph_ctrl.h>
#else
#include <driver/periph_ctrl.h>
#endif
*/

#include <esp_private/gdma.h>
#include <esp_rom_gpio.h>
#include <hal/dma_types.h>
#include <hal/gpio_hal.h>

#include <hal/lcd_ll.h>
#include <soc/lcd_cam_reg.h>
#include <soc/lcd_cam_struct.h>

#include <esp_heap_caps.h>
#include <esp_heap_caps_init.h>


#if __has_include (<esp_private/periph_ctrl.h>)
 #include <esp_private/periph_ctrl.h>
#else
 #include <driver/periph_ctrl.h>
#endif

#if __has_include(<esp_arduino_version.h>)
 #include <esp_arduino_version.h>
#endif

#define DMA_MAX (4096-4)

// The type used for this SoC
#define HUB75_DMA_DESCRIPTOR_T dma_descriptor_t


//----------------------------------------------------------------------------

  class Bus_Parallel16 
  {
  public:
    Bus_Parallel16()
    {

    }

    struct config_t
    {
      // LCD_CAM peripheral number. No need to change (only 0 for ESP32-S3.)
      //int port = 0;

      // max 40MHz (when in 16 bit / 2 byte mode)
      uint32_t bus_freq = 10000000;
      int8_t pin_wr = -1;
      int8_t pin_rd = -1;
      int8_t pin_rs = -1;  // D/C
      bool   invert_pclk = false;
      bool   psram_clk_override = false;
      union
      {
        int8_t pin_data[16];
        struct
        {
          int8_t pin_d0;
          int8_t pin_d1;
          int8_t pin_d2;
          int8_t pin_d3;
          int8_t pin_d4;
          int8_t pin_d5;
          int8_t pin_d6;
          int8_t pin_d7;
          int8_t pin_d8;
          int8_t pin_d9;
          int8_t pin_d10;
          int8_t pin_d11;
          int8_t pin_d12;
          int8_t pin_d13;
          int8_t pin_d14;
          int8_t pin_d15;
        };
      };
    };

    const config_t& config(void) const { return _cfg; }
    void  config(const config_t& config);
   
    bool init(void) ;

    void release(void) ;

    void enable_double_dma_desc();
    bool allocate_dma_desc_memory(size_t len);

    void create_dma_desc_link(void *memory, size_t size, bool dmadesc_b = false);

    void dma_transfer_start();
    void dma_transfer_stop();

     void flip_dma_output_buffer(int back_buffer_id);

  private:

    config_t _cfg;

    volatile lcd_cam_dev_t* _dev;   
    gdma_channel_handle_t dma_chan; 

    uint32_t _dmadesc_count  = 0;   // number of dma decriptors
	
    uint32_t _dmadesc_a_idx  = 0;
    uint32_t _dmadesc_b_idx  = 0;

    HUB75_DMA_DESCRIPTOR_T* _dmadesc_a = nullptr;
    HUB75_DMA_DESCRIPTOR_T* _dmadesc_b = nullptr;    

    bool    _double_dma_buffer = false;
    //bool    _dmadesc_a_active   = true;    

    esp_lcd_i80_bus_handle_t _i80_bus = nullptr;


  };


#endif