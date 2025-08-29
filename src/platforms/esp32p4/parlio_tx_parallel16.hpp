/*
 * ESP32-P4 Parallel TX Driver using PARLIO + Direct GDMA Link API
 * 
 * Based on the ESP32-S3 gdma_lcd_parallel16 implementation
 * Adapted for ESP32-P4 PARLIO (Parallel IO) interface
 * 
 * KEY DIFFERENCES FROM ESP32-S3:
 * - Uses PARLIO's internal GDMA link lists for BCM compatibility
 * - Exposes direct DMA descriptor control through GDMA Link API
 * - Maintains precise descriptor chaining required for HUB75 BCM
 * - PARLIO handles clock/GPIO setup, we manage DMA descriptors manually
 * 
 * This provides full compatibility with the ESP32-S3 HUB75 BCM implementation
 * while leveraging ESP32-P4's PARLIO peripheral for GPIO/clock management.
 */

#pragma once

#if SOC_PARLIO_SUPPORTED

#include <sdkconfig.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <hal/gpio_ll.h>
#include <string.h>
#include <stdint.h>
#include <esp_heap_caps.h>
#include <esp_heap_caps_init.h>

#if __has_include(<esp_private/periph_ctrl.h>)
 #include <esp_private/periph_ctrl.h>
#else
 #include <driver/periph_ctrl.h>
#endif

#if __has_include(<esp_arduino_version.h>)
 #include <esp_arduino_version.h>
#endif

#include "driver/parlio_tx.h"
#include "parlio_priv.h"        // Local copy of parlio private headers
#include "esp_private/gdma.h"
#include "gdma_link.h"          // Local copy of GDMA link API

// Define alignment if not available from parlio_priv.h
#ifndef PARLIO_DMA_DESC_ALIGNMENT
#define PARLIO_DMA_DESC_ALIGNMENT 8
#endif

#define DMA_MAX (4096-4)

// Use the same descriptor buffer size limit as defined in parlio_priv.h
#ifndef PARLIO_DMA_DESCRIPTOR_BUFFER_MAX_SIZE
#define PARLIO_DMA_DESCRIPTOR_BUFFER_MAX_SIZE 4095
#endif

// For HUB75 BCM compatibility - we need direct descriptor access
#define HUB75_DMA_DESCRIPTOR_T gdma_link_list_handle_t

//----------------------------------------------------------------------------

class Bus_Parallel16 
{
public:
    Bus_Parallel16()
    {
        _parlio_unit = nullptr;
        _gdma_link_list_a = nullptr;
        _gdma_link_list_b = nullptr;
        _gdma_channel = nullptr;
        _double_dma_buffer = false;
        _dmadesc_count = 0;
        _dmadesc_a_idx = 0;
        _dmadesc_b_idx = 0;
        _max_descriptor_count = 0;
        _is_transmitting = false;
        _current_buffer_id = 0;
    }

    struct config_t
    {
        // max frequency depends on PARLIO capabilities
        uint32_t bus_freq = 10000000;
        int8_t pin_wr = -1;      // clock pin
        int8_t pin_rd = -1;      // not used in PARLIO
        int8_t pin_rs = -1;      // not used in PARLIO  
        bool   invert_pclk = false;
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
   
    bool init(void);
    void release(void);
    void enable_double_dma_desc();
    bool allocate_dma_desc_memory(uint16_t num_descriptors);
    void create_dma_desc_link(void *memory, size_t size, bool dmadesc_b = false);
    void dma_transfer_start();
    void dma_transfer_stop();
    void flip_dma_output_buffer(int back_buffer_id);
    void dma_bus_deinit(void);  // Clean up GDMA resources

private:
    config_t _cfg;
    parlio_tx_unit_handle_t _parlio_unit;
    
    // BCM-compatible DMA descriptor management using GDMA Link API
    gdma_link_list_handle_t _gdma_link_list_a;
    gdma_link_list_handle_t _gdma_link_list_b;
    gdma_channel_handle_t _gdma_channel;
    
    // Descriptor tracking for HUB75 BCM compatibility
    uint32_t _dmadesc_count;
    uint32_t _dmadesc_a_idx;
    uint32_t _dmadesc_b_idx;
    uint32_t _max_descriptor_count;
    bool _double_dma_buffer;
    
    // Transmit configuration
    parlio_transmit_config_t _transmit_config;
    
    // Internal state
    bool _is_transmitting;
    int _current_buffer_id;
};

#endif // SOC_PARLIO_SUPPORTED
