/*----------------------------------------------------------------------------/
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
#pragma once

#if defined (ESP_PLATFORM)

 #include <sdkconfig.h>

 #if defined (CONFIG_IDF_TARGET_ESP32S2)

  //#pragma message "Compiling for ESP32-S2"
  #include "esp32/esp32_i2s_parallel_dma.hpp"  
  #include "esp32s2/esp32s2-default-pins.hpp"  


 #elif defined (CONFIG_IDF_TARGET_ESP32S3)
  
  //#pragma message "Compiling for ESP32-S3"
  #include "esp32s3/gdma_lcd_parallel16.hpp"
  #include "esp32s3/esp32s3-default-pins.hpp"    
  
  #if defined(SPIRAM_FRAMEBUFFER)
	#pragma message "Use SPIRAM_DMA_BUFFER instead."
  #endif

  #if defined(SPIRAM_DMA_BUFFER) && defined (CONFIG_IDF_TARGET_ESP32S3)       
   #pragma message "Enabling use of PSRAM/SPIRAM based DMA Buffer"
   
   // Disable fast functions because I don't understand the interaction with DMA PSRAM and the CPU->DMA->SPIRAM Cache implications..
   #define NO_FAST_FUNCTIONS 1

  #endif

 #elif defined(CONFIG_IDF_TARGET_ESP32C6)

  #pragma message "ESP32C6 is not supported, as this silicon does not support continuous DMA transfer."
  
  /* 
   * Refer to: https://github.com/espressif/esp-idf/tree/master/examples/peripherals/parlio/parlio_tx/simple_rgb_led_matrix
   * 	"Because of the hardware limitation in ESP32-C6 and ESP32-H2, the transaction length can't be controlled by DMA, thus 
   * 	we can't flush the LED screen continuously within a hardware loop."
   *
   */
  
  //#include "esp32c6/dma_parallel_io.hpp"
  //#include "esp32c6/esp32c6-default-pins.hpp"

 #elif defined(CONFIG_IDF_TARGET_ESP32P4)

   #pragma message "You are ahead of your time. ESP32P4 support is planned"

 #elif defined (CONFIG_IDF_TARGET_ESP32) || defined(ESP32)

  // Assume an ESP32 (the original 2015 version)
  // Same include as ESP32S3  
  //#pragma message "Compiling for original ESP32 (released 2016)"  
  
  #define ESP32_THE_ORIG 1	
  //#include "esp32/esp32_i2s_parallel_dma.hpp"
  //#include "esp32/esp32_i2s_parallel_dma.h"
  #include "esp32/esp32_i2s_parallel_dma.hpp"  
  #include "esp32/esp32-default-pins.hpp"

#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32H2)

	#error "ESP32 C2 C3 and H2 devices are not supported by this library."

 
 #else
    #error "Unknown platform."
  
 #endif


#endif

