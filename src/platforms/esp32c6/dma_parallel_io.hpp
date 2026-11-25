/*
    Experimental ESP32-C6 Parallel IO (PARLIO) + AHB GDMA driver for HUB75.

    ESP32-C6 PARLIO TX does not support DMA-EOF continuous hardware looping and
    caps each triggered transaction at 65,535 bytes (tx_bytelen). This driver
    auto-splits each framebuffer's DMA descriptor chain into <= 65,532-byte
    non-circular segments, chaining segments within a frame via the
    PARLIO_LL_EVENT_TX_EOF interrupt while keeping OE blanked (HIGH) when idle.
    Frame-level refresh pacing is driven externally by MatrixPanel_I2S_DMA.
*/

#pragma once

#include <sdkconfig.h>

#ifdef CONFIG_IDF_TARGET_ESP32C6

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <esp_attr.h>
#include <esp_err.h>
#include <esp_intr_alloc.h>
#include <hal/dma_types.h>
#include <esp_private/gdma.h>

#define DMA_MAX (4096 - 4)
#define HUB75_DMA_DESCRIPTOR_T dma_descriptor_t

class Bus_Parallel16
{
public:
    Bus_Parallel16();
    ~Bus_Parallel16();

    struct config_t
    {
        uint32_t bus_freq = 10000000;
        int8_t pin_wr = -1;
        int8_t pin_rd = -1;
        int8_t pin_rs = -1;
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

    void set_dma_output_buffer(int buffer_id);

private:
    struct segment_t
    {
        uint32_t start_idx;
        uint32_t byte_len;
    };

    void free_dma_desc_memory(void);
    void start_segment_hw(const segment_t &seg);
    static void IRAM_ATTR parlio_tx_isr(void *arg);

    config_t _cfg;

    gdma_channel_handle_t _dma_chan = nullptr;
    intr_handle_t _parlio_intr = nullptr;

    uint32_t _dmadesc_count = 0;
    uint32_t _dmadesc_a_idx = 0;
    HUB75_DMA_DESCRIPTOR_T* _dmadesc_a = nullptr;

    std::vector<segment_t> _segments;
    const segment_t* _segments_ptr = nullptr;
    size_t _seg_count = 0;
    size_t _seg_idx = 0;

    bool _initialized = false;
    bool _frame_active = false;
    bool _retrigger_pending = false;
    int64_t _frame_start_us = 0;

    portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;
};

#endif // CONFIG_IDF_TARGET_ESP32C6