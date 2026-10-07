#pragma once

#include <sdkconfig.h>

// Guard enable for both ESP32-P4 and ESP32-S31 SoCs using PARLIO + GDMA
#if defined(CONFIG_IDF_TARGET_ESP32P4)  || \
    defined(CONFIG_IDF_TARGET_ESP32S31) || \
    defined(SOC_PARLIO_SUPPORTED)

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>

#include "driver/parlio_tx.h"
#include "esp_private/gdma.h"
#include "gdma_link.h"
#include "parlio_priv.h"

#define DMA_MAX (4096 - 4)

// Descriptor handle alias for PARLIO-supported targets
#define HUB75_DMA_DESCRIPTOR_T gdma_link_list_handle_t

class Bus_Parallel16 
{
public:
    Bus_Parallel16();
    ~Bus_Parallel16();

    struct config_t
    {
        uint32_t bus_freq = 10000000;
        int8_t pin_wr = -1;      // Output Clock Pin
        int8_t pin_rd = -1;      // Unused in PARLIO
        int8_t pin_rs = -1;      // Unused in PARLIO
        bool   invert_pclk = false;
        int8_t parallel_width = 16;
        union
        {
            int8_t pin_data[16];
            struct
            {
                int8_t pin_d0;  int8_t pin_d1;  int8_t pin_d2;  int8_t pin_d3;
                int8_t pin_d4;  int8_t pin_d5;  int8_t pin_d6;  int8_t pin_d7;
                int8_t pin_d8;  int8_t pin_d9;  int8_t pin_d10; int8_t pin_d11;
                int8_t pin_d12; int8_t pin_d13; int8_t pin_d14; int8_t pin_d15;
            };
        };
    };

    const config_t& config(void) const { return _cfg; }
    void config(const config_t& config);

    bool init(void);
    void release(void);

    void enable_double_dma_desc();
    bool allocate_dma_desc_memory(size_t len);

    void create_dma_desc_link(void *memory, size_t size, bool dmadesc_b = false);

    void dma_transfer_start();
    void dma_transfer_stop();

    void flip_dma_output_buffer(int buffer_id);

private:
    void dma_bus_deinit(void);

    config_t _cfg;

    parlio_tx_unit_handle_t _parlio_unit;
    gdma_channel_handle_t   _gdma_channel;

    gdma_link_list_handle_t _gdma_link_list_a;
    gdma_link_list_handle_t _gdma_link_list_b;

    bool     _double_dma_buffer = false;
    uint32_t _dmadesc_count     = 0;
    uint32_t _dmadesc_last      = 0;

    uint32_t _dmadesc_a_idx     = 0;
    uint32_t _dmadesc_b_idx     = 0;

    bool     _is_transmitting   = false;
    int      _active_buffer_id  = 0;
};

#endif // CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31