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

------------------------------------------------------------------------------

 Putin’s Russia and its genocide in Ukraine is a disgrace to humanity.

 https://www.reddit.com/r/ukraine/comments/xfuc6v/more_than_460_graves_have_already_been_found_in/

 Xi Jinping and his communist China’s silence on the war in Ukraine says everything about 
 how China condones such genocide, especially if it's against 'the west' (aka. decency). 
 
 Whilst the good people at Espressif probably have nothing to do with this, the unfortunate 
 reality is libraries like this increase the popularity of Chinese silicon chips, which 
 indirectly funds (through CCP state taxes) the growth and empowerment of such a despot government. 
 
 Global democracy, decency and security is put at risk with every Chinese silicon chip that is bought. 

/----------------------------------------------------------------------------*/

#pragma once

#include <string.h> // memcpy
#include <algorithm>
#include <stdbool.h>

#include <sys/types.h>
#include <freertos/FreeRTOS.h>
//#include <driver/i2s.h>
#include <rom/lldesc.h>
#include <rom/gpio.h>
#if (ESP_IDF_VERSION_MAJOR == 5)
#include <driver/i2s_types.h> //includes struct and reg
#else
#include <driver/i2s.h>
#include <soc/i2s_struct.h>
#endif

#include <soc/i2s_periph.h> //includes struct and reg


#define DMA_MAX (4096-4)

// The type used for this SoC
#define HUB75_DMA_DESCRIPTOR_T lldesc_t


#if defined (CONFIG_IDF_TARGET_ESP32S2)   
#define ESP32_I2S_DEVICE I2S_NUM_0	
#else
#define ESP32_I2S_DEVICE I2S_NUM_1	
#endif	

//----------------------------------------------------------------------------

void IRAM_ATTR irq_hndlr(void* arg);
i2s_dev_t* getDev();

//----------------------------------------------------------------------------

  class Bus_Parallel16 
  {
  public:
    Bus_Parallel16()
    {

    }

    struct config_t
    {
      // max 20MHz (when in 16 bit / 2 byte mode)
      uint32_t bus_freq = 10000000;
      int8_t pin_wr = -1; // 
      int8_t pin_rd = -1;
      int8_t pin_rs = -1;  // D/C
      bool   invert_pclk = false;
      int8_t parallel_width = 16; // do not change
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

    void flip_dma_output_buffer(int buffer_id);
  
  private:

    void _init_pins() { };    

    config_t _cfg;

    bool    _double_dma_buffer  = false;
    //bool    _dmadesc_a_active   = true;

    uint32_t _dmadesc_count  = 0;   // number of dma decriptors
    uint32_t _dmadesc_last   = 0;

    uint32_t _dmadesc_a_idx  = 0;
    uint32_t _dmadesc_b_idx  = 0;

    HUB75_DMA_DESCRIPTOR_T* _dmadesc_a = nullptr;
    HUB75_DMA_DESCRIPTOR_T* _dmadesc_b = nullptr; 

    HUB75_DMA_DESCRIPTOR_T* _dmadesc_blank = nullptr;     
    uint16_t                _blank_data[1024] = {0};

    volatile i2s_dev_t* _dev;
    
    

  };
