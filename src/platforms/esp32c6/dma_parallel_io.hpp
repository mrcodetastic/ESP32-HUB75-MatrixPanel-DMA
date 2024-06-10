/*
    Simple example of using the ESP32-C6's parallel IO peripheral for general-purpose 
    parallel data output with DMA.

    Credits to ESPRESSIF them selfe, they made the first example:

    https://github.com/espressif/esp-idf/tree/release/v5.1/examples/peripherals/parlio/simple_rgb_led_matrix

    And Credits to the guy who implemented the S3 version of this library. 
    There is a lot of resusable code


*/


#pragma once

#include <sdkconfig.h>

#ifdef CONFIG_IDF_TARGET_ESP32C6

#include "driver/parlio_tx.h"

#include <esp_private/gdma.h>
#include <hal/dma_types.h>
#include <hal/gpio_hal.h>


#define DMA_MAX (4096-4)
#define HUB75_DMA_DESCRIPTOR_T dma_descriptor_t


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
      //int8_t pin_rd = -1;
      //int8_t pin_rs = -1;  // D/C
      bool   invert_pclk = false;
      //bool   psram_clk_override = false;
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

    gdma_channel_handle_t dma_chan; 

    uint32_t _dmadesc_count  = 0;   // number of dma decriptors
	
    uint32_t _dmadesc_a_idx  = 0;
    uint32_t _dmadesc_b_idx  = 0;

    HUB75_DMA_DESCRIPTOR_T* _dmadesc_a = nullptr;
    HUB75_DMA_DESCRIPTOR_T* _dmadesc_b = nullptr;    

    bool    _double_dma_buffer = false;
    //bool    _dmadesc_a_active   = true;    

  };



#endif