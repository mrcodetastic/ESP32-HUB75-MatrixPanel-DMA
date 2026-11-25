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

  /*
   * Experimental ESP32-C6 support using triggered PARLIO TX + AHB GDMA.
   * ESP32-C6 PARLIO cannot loop infinitely via DMA EOF and caps each
   * transaction at 65,535 bytes; Bus_Parallel16 auto-splits frames into
   * <= 65,532-byte segments while MatrixPanel_I2S_DMA paces refreshes via
   * esp_timer (single-buffer only).
   */
  #pragma message "EXPERIMENTAL: Compiling for ESP32-C6 with triggered PARLIO support (single-buffer only)."
  #include "esp32c6/dma_parallel_io.hpp"
  #include "esp32c6/esp32c6-default-pins.hpp"

  #if defined(SPIRAM_DMA_BUFFER)
   #error "SPIRAM_DMA_BUFFER is not supported on ESP32-C6 (no external PSRAM)."
  #endif

 #elif defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)

  // ESP32-S31 has the same PARLIO block as the P4 and is assumed to share its
  // pin defaults for now (pending hardware bring-up). The PARLIO driver is
  // already guarded for CONFIG_IDF_TARGET_ESP32S31.
  #pragma message "Compiling for ESP32-P4/S31 with PARLIO support"
  #include "esp32p4/esp32_parlio_parallel_dma.hpp"
  #include "esp32p4/esp32p4-default-pins.hpp"     

 #elif defined(CONFIG_IDF_TARGET_ESP32) || (defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CONFIG_IDF_TARGET_ESP32C6))

  // Assume an ESP32 (the original 2015 version)
  // Same include as ESP32S3  
  //#pragma message "Compiling for original ESP32 (released 2016)"  
  
  #define ESP32_THE_ORIG 1	
  //#include "esp32/esp32_i2s_parallel_dma.hpp"
  //#include "esp32/esp32_i2s_parallel_dma.h"
  #include "esp32/esp32_i2s_parallel_dma.hpp"  
  #include "esp32/esp32-default-pins.hpp"

#elif defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32H2)

	#error "ESP32 C2, C3, C5 and H2 devices are not supported by this library."

 
 #else
    #error "Unknown platform."
  
 #endif


#endif

