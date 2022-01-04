#pragma once
/*
 * ESP32_I2S_PARALLEL_DMA
 */
 
#pragma once

#if defined(ESP32) || defined(IDF_VER)

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sys/types.h>

#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>
#include <esp_err.h>
//#include <esp32/rom/lldesc.h>
//#include <esp32/rom/gpio.h>
#include <rom/lldesc.h>
#include <rom/gpio.h>


// Get MCU Type and Max CLK Hz for MCU
#include <esp32_i2s_parallel_mcu_def.h>

typedef enum {
  I2S_PARALLEL_WIDTH_8,
  I2S_PARALLEL_WIDTH_16,
  I2S_PARALLEL_WIDTH_24,
  I2S_PARALLEL_WIDTH_MAX
} i2s_parallel_cfg_bits_t;

typedef struct {
    int gpio_bus[24];   // The parallel GPIOs to use, set gpio to -1 to disable
    int gpio_clk;
    int sample_rate;    // 'clockspeed'
    int sample_width;
    int desccount_a;
    lldesc_t * lldesc_a;
    int desccount_b;      // only used with double buffering
    lldesc_t * lldesc_b;  // only used with double buffering
    bool clkphase;        // Clock signal phase
    bool int_ena_out_eof; // Do we raise an interrupt every time the DMA output loops? Don't do this unless we're doing double buffering!
} i2s_parallel_config_t;

static inline int i2s_parallel_get_memory_width(i2s_port_t port, i2s_parallel_cfg_bits_t width) {
  switch(width) {
    case I2S_PARALLEL_WIDTH_8:

#ifdef ESP32_ORIG   
      // Only I2S1 on the legacy ESP32 WROOM MCU supports space saving single byte 8 bit parallel access
      if(port == I2S_NUM_1) {
        return 1;
      } else {
        return 2;
      }
#else 
        return 1;
#endif

    case I2S_PARALLEL_WIDTH_16:
      return 2;
    case I2S_PARALLEL_WIDTH_24:
      return 4;
    default:
      return -ESP_ERR_INVALID_ARG;
  }
}

// DMA Linked List Creation
void link_dma_desc(volatile lldesc_t *dmadesc, volatile lldesc_t *prevdmadesc, void *memory, size_t size);

// I2S DMA Peripheral Setup Functions
esp_err_t   i2s_parallel_driver_install(i2s_port_t port, i2s_parallel_config_t* conf);
esp_err_t   i2s_parallel_send_dma(i2s_port_t port, lldesc_t* dma_descriptor);
esp_err_t   i2s_parallel_stop_dma(i2s_port_t port);
//i2s_dev_t*    i2s_parallel_get_dev(i2s_port_t port);

// For frame buffer flipping / double buffering
typedef struct {
    volatile lldesc_t *dmadesc_a, *dmadesc_b;
    int desccount_a, desccount_b;
    i2s_port_t i2s_interrupt_port_arg;
} i2s_parallel_state_t;

void i2s_parallel_flip_to_buffer(i2s_port_t port, int bufid);
bool i2s_parallel_is_previous_buffer_free();
void i2s_parallel_set_previous_buffer_not_free();

// Callback function for when whole length of DMA chain has been sent out.
typedef void (*callback)(void);
void setShiftCompleteCallback(callback f);



#ifdef __cplusplus
}
#endif

#endif

