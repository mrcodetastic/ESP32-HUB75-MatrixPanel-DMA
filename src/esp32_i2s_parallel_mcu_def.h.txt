#pragma once

/* Abstract the Espressif IDF ESP32 MCU variant compile-time defines
 * into another list for the purposes of this library.
 *
 * i.e. I couldn't be bothered having to update the library when they
 * release the ESP32S4,5,6,7, n+1 etc. if they are all fundamentally
 * the same architecture.
 */
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3

	#define ESP32_SXXX 1
	#define ESP32_I2S_DEVICE            I2S_NUM_0	
		
	#define I2S_PARALLEL_CLOCK_HZ 160000000L
	#define DMA_MAX (4096-4)

#elif CONFIG_IDF_TARGET_ESP32 || defined(ESP32)

	// 2016 model that started it all, and this library. The best.
	#define ESP32_ORIG 1	
	#define ESP32_I2S_DEVICE            I2S_NUM_0	
	
	#define I2S_PARALLEL_CLOCK_HZ 80000000L
	#define DMA_MAX (4096-4)
	
#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2

	#error "ESPC-series RISC-V MCU's do not support parallel DMA and not supported by this library!"
	#define ESP32_CXXX 1

#else
	#error "ERROR: No ESP32 or ESP32 Espressif IDF detected at compile time."

#endif