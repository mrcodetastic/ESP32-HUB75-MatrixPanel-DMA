#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#if defined(SPIRAM_DMA_BUFFER)
// Sprite_TM memory caching utilities: https://www.esp32.com/viewtopic.php?f=2&t=30584
#include "rom/cache.h"
#endif


/* We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
 * 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
 * Irrelevant for ESP32-S2 the way the FIFO ordering works is different - refer to page 679 of S2 technical reference manual
 */
#if defined(ESP32_THE_ORIG)
#define ESP32_TX_FIFO_POSITION_ADJUST(x_coord) (((x_coord)&1U) ? (x_coord - 1) : (x_coord + 1))
#else
#define ESP32_TX_FIFO_POSITION_ADJUST(x_coord) x_coord
#endif


  /* LED Brightness Compensation */                                                               
#ifndef NO_CIE1931                                                                                
  /* CIE 1931 correction with bit-depth-optimized LUTs*/                                            
  #ifdef LUT_NATIVE_BIT_DEPTH


    // Optimized path: LUT maps 8-bit input (0-255) directly to target bit depth output
    #define DO_BRIGHTNESS_COMPENSATION()                                                              \
      auto red_val   = lumConvTab[red];                                                               \
      auto green_val = lumConvTab[green];                                                             \
      auto blue_val  = lumConvTab[blue];                                                              


  #else
    // Fallback for non-standard bit depths: 12-bit LUT with shift+round to target depth

    #define DO_BRIGHTNESS_COMPENSATION()                                                              \
      uint16_t red16   = lumConvTab[red];                                                             \
      uint16_t green16 = lumConvTab[green];                                                           \
      uint16_t blue16  = lumConvTab[blue];                                                            \
                                                                                                      \
      uint8_t shift_amount = 12 - m_cfg.getPixelColorDepthBits();                                     \
      uint16_t rounding = 1 << (shift_amount - 1);                                                    \
      uint16_t max_val = (1 << m_cfg.getPixelColorDepthBits()) -1;                                    \
      auto red_val   = (red16 + rounding) >> shift_amount;                                            \
      auto green_val = (green16 + rounding) >> shift_amount;                                          \
      auto blue_val  = (blue16 + rounding) >> shift_amount;                                           \
                                                                                                      \
      red_val = red_val > max_val ? max_val : red_val;                                                \
      green_val = green_val > max_val ? max_val : green_val;                                          \
      blue_val = blue_val > max_val ? max_val : blue_val;  
  #endif
#else
  // NO_CIE1931: linear scaling with rounding
  #define DO_BRIGHTNESS_COMPENSATION()                                                               \
    uint16_t red16   = red * 256u;                                                                   \
    uint16_t green16 = green * 256u;                                                                 \
    uint16_t blue16  = blue * 256u;                                                                  \
                                                                                                     \
    uint8_t shift_amount = 16 - m_cfg.getPixelColorDepthBits();                                      \
    uint16_t rounding = (1 << (shift_amount - 1))-1;                                                 \
    uint16_t max_val = (1 << m_cfg.getPixelColorDepthBits()) -1;                                     \
    uint16_t red_val   = (red16 + rounding) >> shift_amount;                                         \
    uint16_t green_val = (green16 + rounding) >> shift_amount;                                       \
    uint16_t blue_val  = (blue16 + rounding) >> shift_amount;                                        \
    red_val = red_val > max_val ? max_val : red_val;                                                 \
    green_val = green_val > max_val ? max_val : green_val;                                           \
    blue_val = blue_val > max_val ? max_val : blue_val;                                              
#endif

// -----------------------------------------------------------------------------
// setupDMA() Subroutines
// -----------------------------------------------------------------------------

bool MatrixPanel_I2S_DMA::allocateFrameBuffers(int fbs_required) {
    ESP_LOGI("I2S-DMA", "Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("I2S-DMA", "Free SPIRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    size_t allocated_fb_memory = 0;

    for (int fb_idx = 0; fb_idx < fbs_required; fb_idx++) {
        frame_buffer[fb_idx].rowBits.reserve(ROWS_PER_FRAME);

        for (int malloc_num = 0; malloc_num < ROWS_PER_FRAME; malloc_num++) {
            std::unique_ptr<rowBitStruct> ptr(new rowBitStruct(PIXELS_PER_ROW, m_cfg.getPixelColorDepthBits()));

            if (ptr->data == nullptr) {
                ESP_LOGE("I2S-DMA", "CRITICAL ERROR: Not enough memory for requested colour depth of %d bits!", m_cfg.getPixelColorDepthBits());
                return false;
            }

            allocated_fb_memory += ptr->getColorDepthSize(false);
            frame_buffer[fb_idx].rowBits.push_back(std::move(ptr));
            ++frame_buffer[fb_idx].rows;
        }
    }
    ESP_LOGI("I2S-DMA", "Allocating %d bytes memory for DMA BCM framebuffer(s).", allocated_fb_memory);
    return true;
}

void MatrixPanel_I2S_DMA::calculateRefreshRateAndTransitionBit() {
#if !defined(FORCE_COLOR_DEPTH)
    ESP_LOGI("I2S-DMA", "Minimum visual refresh rate requested: %d Hz", m_cfg.min_refresh_rate);

    while (true) {
        int psPerClock = 1000000000000UL / static_cast<uint32_t>(m_cfg.i2sspeed);
        int nsPerLatch = ((PIXELS_PER_ROW + CLKS_DURING_LATCH) * psPerClock) / 1000;
        int nsPerRow = m_cfg.getPixelColorDepthBits() * nsPerLatch;

        for (int i = lsbMsbTransitionBit + 1; i < m_cfg.getPixelColorDepthBits(); i++) {
            nsPerRow += (1 << (i - lsbMsbTransitionBit - 1)) * nsPerLatch;
        }

        int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
        int actualRefreshRate = 1000000000UL / nsPerFrame;
        calculated_refresh_rate = actualRefreshRate;

        if (actualRefreshRate >= m_cfg.min_refresh_rate) break;

        if (lsbMsbTransitionBit < m_cfg.getPixelColorDepthBits() - 1) {
            lsbMsbTransitionBit++;
        } else {
            break;
        }
    }

    if (lsbMsbTransitionBit > 0) {
        ESP_LOGW("I2S-DMA", "lsbMsbTransitionBit of %d used to achieve refresh rate of %d Hz.", lsbMsbTransitionBit, m_cfg.min_refresh_rate);
    }
#endif
}

int MatrixPanel_I2S_DMA::calculateDmaDescriptorCount(int &dma_descs_per_row_all_cdepths, 
                                                      size_t &last_dma_desc_bytes_all_cdepths, 
                                                      int &dma_descs_per_row_1cdepth, 
                                                      size_t &last_dma_desc_bytes_1cdepth) const 
{
    // Single bitplane payload size metrics
    dma_descs_per_row_1cdepth = (frame_buffer[0].rowBits[0]->getColorDepthSize(true) + DMA_MAX - 1) / DMA_MAX;
    last_dma_desc_bytes_1cdepth = (frame_buffer[0].rowBits[0]->getColorDepthSize(true) % DMA_MAX);

    // Full frame (all color depths LSB to MSB in 1 sweep) payload size metrics
    dma_descs_per_row_all_cdepths = (frame_buffer[0].rowBits[0]->getColorDepthSize(false) + DMA_MAX - 1) / DMA_MAX;
    last_dma_desc_bytes_all_cdepths = (frame_buffer[0].rowBits[0]->getColorDepthSize(false) % DMA_MAX);

    // 1. Base pass: 1 full sweep of all bitplanes (LSB -> MSB)
    int dma_descriptors_per_row = dma_descs_per_row_all_cdepths;

    // 2. Additional BTD passes for bits above lsbMsbTransitionBit:
    // Each pass sweeps from bit 'i' up through MSB (remaining bitplanes = color_depth - i)
    for (int i = lsbMsbTransitionBit + 1; i < m_cfg.getPixelColorDepthBits(); i++) {
        uint8_t remaining_bitplanes = m_cfg.getPixelColorDepthBits() - i;
        int dma_descs_for_remaining = (frame_buffer[0].rowBits[0]->width * remaining_bitplanes * sizeof(ESP32_I2S_DMA_STORAGE_TYPE) + DMA_MAX - 1) / DMA_MAX;
        
        int passes = (1 << (i - lsbMsbTransitionBit - 1));
        dma_descriptors_per_row += passes * dma_descs_for_remaining;
    }

    return dma_descriptors_per_row * ROWS_PER_FRAME;
}

/**
 * @brief Builds and links DMA descriptor linked lists across row color bitplanes.
 * 
 * --- MEMORY & DESCRIPTOR OPTIMIZATION STRATEGY ---
 * Instead of creating separate DMA descriptors for every individual bitplane layer, 
 * this implementation uses a single-pass sweep to MSB (Most Significant Bit) approach.
 * 
 * 1. Base Pass (Pass 0):
 *    We issue ONE DMA transfer that reads contiguously from bitplane 0 (LSB) all the 
 *    way through to the highest bitplane (MSB). This single sweep displays every color 
 *    bit once, handling the baseline illumination for all bits <= lsbMsbTransitionBit.
 * 
 * 2. Binary Time Division (BTD) Passes:
 *    For bitplanes above lsbMsbTransitionBit, higher weight bits need to be displayed 
 *    longer (2^N time weighting). Rather than isolating bitplane 'i' alone in a 
 *    dedicated payload, each subsequent pass starts at bitplane 'i' and sweeps 
 *    CONTIGUOUSLY through to the MSB (remaining bitplanes = color_depth - i).
 * 
 *    WHY THIS SAVES DESCRIPTOR RAM:
 *    Because each extra pass automatically sweeps through all higher bitplanes up 
 *    to MSB, those higher bitplanes accumulate display time naturally during lower 
 *    bit passes. This allows us to reduce the required loop iterations for bit 'i' 
 *    to 2^(i - lsbMsbTransitionBit - 1) instead of 2^i. Sweeping contiguous memory 
 *    blocks in larger chunks significantly reduces the total count of DMA descriptors 
 *    allocated in internal RAM.
 */
void MatrixPanel_I2S_DMA::buildDmaDescriptorLinks(int fbs_required, 
                                                   int dma_descs_per_row_all_cdepths, 
                                                   size_t last_dma_desc_bytes_all_cdepths, 
                                                   int dma_descs_per_row_1cdepth, 
                                                   size_t last_dma_desc_bytes_1cdepth) 
{
    for (int fb_idx = 0; fb_idx < fbs_required; fb_idx++) {  
        int _dmadescriptor_count = 0;

        for (int row = 0; row < ROWS_PER_FRAME; row++) {
            
            // -----------------------------------------------------------------
            // Pass 0: Base Sweep (LSB -> MSB in one contiguous DMA payload)
            // Displays all color depth layers once to establish base bit timing.
            // -----------------------------------------------------------------
            for (int dma_desc_all = 0; dma_desc_all < dma_descs_per_row_all_cdepths; dma_desc_all++) {
                size_t payload_bytes = (dma_desc_all == (dma_descs_per_row_all_cdepths - 1)) 
                                        ? last_dma_desc_bytes_all_cdepths 
                                        : DMA_MAX;
                
                // Point descriptor directly to bitplane 0 head pointer
                dma_bus.create_dma_desc_link(
                    frame_buffer[fb_idx].rowBits[row]->getDataPtr(0) + (dma_desc_all * (DMA_MAX / sizeof(ESP32_I2S_DMA_STORAGE_TYPE))), 
                    payload_bytes, 
                    (fb_idx == 1)
                );
                _dmadescriptor_count++;
            }

            // -----------------------------------------------------------------
            // BTD Passes: Additional sweeps for bitplanes > lsbMsbTransitionBit
            // -----------------------------------------------------------------
            for (int i = lsbMsbTransitionBit + 1; i < m_cfg.getPixelColorDepthBits(); i++) {
                
                // Calculate total contiguous byte payload from bitplane 'i' up through MSB
                uint8_t remaining_bitplanes = m_cfg.getPixelColorDepthBits() - i;
                size_t sweep_payload_bytes = frame_buffer[fb_idx].rowBits[row]->width 
                                            * remaining_bitplanes 
                                            * sizeof(ESP32_I2S_DMA_STORAGE_TYPE);
                
                // Determine how many DMA_MAX chunk descriptors are needed for this sweep segment
                int dma_descs_for_sweep = (sweep_payload_bytes + DMA_MAX - 1) / DMA_MAX;
                size_t last_desc_bytes_for_sweep = (sweep_payload_bytes % DMA_MAX);

                // By sweeping through to MSB every time, we halve the number of sweeps 
                // required for bit level 'i', conserving DMA linked list descriptor memory.
                int passes = (1 << (i - lsbMsbTransitionBit - 1));
                for (int k = 0; k < passes; k++) {
                    for (int desc_idx = 0; desc_idx < dma_descs_for_sweep; desc_idx++) {
                        size_t payload_bytes = (desc_idx == (dma_descs_for_sweep - 1) && last_desc_bytes_for_sweep > 0) 
                                                ? last_desc_bytes_for_sweep 
                                                : DMA_MAX;
                        
                        // Point descriptor offset starting at bitplane 'i'
                        dma_bus.create_dma_desc_link(
                            frame_buffer[fb_idx].rowBits[row]->getDataPtr(i) + (desc_idx * (DMA_MAX / sizeof(ESP32_I2S_DMA_STORAGE_TYPE))), 
                            payload_bytes, 
                            (fb_idx == 1)
                        );
                        _dmadescriptor_count++;
                    }
                } 
            } 
        } 
        ESP_LOGI("I2S-DMA", "Created %d DMA descriptors for buffer %d.", _dmadescriptor_count, fb_idx);	
    } 
}

void MatrixPanel_I2S_DMA::configureBusParallel16() {
    auto bus_cfg = dma_bus.config(); 
    bus_cfg.bus_freq    = static_cast<uint32_t>(m_cfg.i2sspeed);
    bus_cfg.pin_wr      = m_cfg.gpio.clk;
    bus_cfg.invert_pclk = m_cfg.clkphase;

    bus_cfg.pin_d0 = m_cfg.gpio.r1;
    bus_cfg.pin_d1 = m_cfg.gpio.g1;
    bus_cfg.pin_d2 = m_cfg.gpio.b1;
    bus_cfg.pin_d3 = m_cfg.gpio.r2;
    bus_cfg.pin_d4 = m_cfg.gpio.g2;
    bus_cfg.pin_d5 = m_cfg.gpio.b2;
    bus_cfg.pin_d6 = m_cfg.gpio.lat;
    bus_cfg.pin_d7 = m_cfg.gpio.oe;
    bus_cfg.pin_d8 = m_cfg.gpio.a;
    bus_cfg.pin_d9 = m_cfg.gpio.b;
    bus_cfg.pin_d10 = m_cfg.gpio.c;
    bus_cfg.pin_d11 = m_cfg.gpio.d;
    bus_cfg.pin_d12 = m_cfg.gpio.e;
    bus_cfg.pin_d13 = -1;
    bus_cfg.pin_d14 = -1;
    bus_cfg.pin_d15 = -1;

    dma_bus.config(bus_cfg);
    ESP_LOGI("I2S-DMA", "DMA setup completed");
}

bool MatrixPanel_I2S_DMA::setupDMA(const HUB75_I2S_CFG &_cfg) {
    int fbs_required = (m_cfg.double_buff) ? 2 : 1;

    if (!allocateFrameBuffers(fbs_required)) return false;

    calculateRefreshRateAndTransitionBit();

    int dma_descs_per_row_all_cdepths = 0;
    size_t last_dma_desc_bytes_all_cdepths = 0;
    int dma_descs_per_row_1cdepth = 0;
    size_t last_dma_desc_bytes_1cdepth = 0;

    int dma_descriptions_required = calculateDmaDescriptorCount(
        dma_descs_per_row_all_cdepths, last_dma_desc_bytes_all_cdepths, 
        dma_descs_per_row_1cdepth, last_dma_desc_bytes_1cdepth);

    if (m_cfg.double_buff) {
        dma_bus.enable_double_dma_desc();
    }

    if (!dma_bus.allocate_dma_desc_memory(dma_descriptions_required)) {
        return false;
    }

    buildDmaDescriptorLinks(fbs_required, dma_descs_per_row_all_cdepths, 
                             last_dma_desc_bytes_all_cdepths, 
                             dma_descs_per_row_1cdepth, 
                             last_dma_desc_bytes_1cdepth);

    configureBusParallel16();

    fb = &frame_buffer[0];
    initialized = true;
    return true;
}

// -----------------------------------------------------------------------------
// Core Pixel Updating Routines
// -----------------------------------------------------------------------------

void IRAM_ATTR MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint16_t x_coord, uint16_t y_coord, uint8_t red, uint8_t green, uint8_t blue) {
    if (!initialized || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height) return;

    DO_BRIGHTNESS_COMPENSATION()

    x_coord = ESP32_TX_FIFO_POSITION_ADJUST(x_coord);

    uint16_t _colourbitclear = BITMASK_RGB1_CLEAR;
    uint16_t _colourbitoffset = 0;

    if (y_coord >= ROWS_PER_FRAME) {
        _colourbitoffset = BITS_RGB2_OFFSET;
        _colourbitclear = BITMASK_RGB2_CLEAR;
        y_coord -= ROWS_PER_FRAME;
    }


  // Pre-fetch base pointer and stride width ONCE
  auto& rowStruct = fb->rowBits[y_coord];
  ESP32_I2S_DMA_STORAGE_TYPE* base_ptr = rowStruct->data.get();
  const uint16_t stride = rowStruct->width;

  uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
  do {
    --colour_depth_idx;

    // Fast shift extraction
    uint16_t RGB_output_bits = (((blue_val  >> colour_depth_idx) & 1) << 2) |
                               (((green_val >> colour_depth_idx) & 1) << 1) |
                                ((red_val   >> colour_depth_idx) & 1);
    
    RGB_output_bits <<= _colourbitoffset;

    // Calculate depth plane pointer via simple pointer arithmetic
    ESP32_I2S_DMA_STORAGE_TYPE *p = base_ptr + (colour_depth_idx * stride);

    p[x_coord] = (p[x_coord] & _colourbitclear) | RGB_output_bits;

#if defined(SPIRAM_DMA_BUFFER)
    Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE));
#endif
  } while (colour_depth_idx);
  
  
/*
    uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
    do {
        --colour_depth_idx;

        uint16_t mask = (1 << colour_depth_idx);
        uint16_t RGB_output_bits = 0;

        RGB_output_bits |= (bool)(blue_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(green_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(red_val & mask);
        RGB_output_bits <<= _colourbitoffset;

        ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, colour_depth_idx);

        p[x_coord] &= _colourbitclear;
        p[x_coord] |= RGB_output_bits;

#if defined(SPIRAM_DMA_BUFFER)
        Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE));
#endif
    } while (colour_depth_idx);
*/


}

void MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue) {
    if (!initialized) return;

    DO_BRIGHTNESS_COMPENSATION()

    for (uint8_t colour_depth_idx = 0; colour_depth_idx < m_cfg.getPixelColorDepthBits(); colour_depth_idx++) {
        uint16_t RGB_output_bits = 0;
        uint16_t mask = (1 << colour_depth_idx);

        RGB_output_bits |= (bool)(blue_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(green_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(red_val & mask);

        RGB_output_bits |= (RGB_output_bits << BITS_RGB2_OFFSET);

        int matrix_frame_parallel_row = fb->rowBits.size();
        do {
            --matrix_frame_parallel_row;

            ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(matrix_frame_parallel_row, colour_depth_idx);

            int x_coord = fb->rowBits[matrix_frame_parallel_row]->width;
            do {
                --x_coord;
                p[x_coord] &= BITMASK_RGB12_CLEAR;
                p[x_coord] |= RGB_output_bits;

#if defined(SPIRAM_DMA_BUFFER)
                Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE));
#endif
            } while (x_coord);
        } while (matrix_frame_parallel_row);
    }
}

// -----------------------------------------------------------------------------
// clearFrameBuffer() Subroutines
// -----------------------------------------------------------------------------

ESP32_I2S_DMA_STORAGE_TYPE MatrixPanel_I2S_DMA::calculateRowAddressMask(uint8_t row_idx, ESP32_I2S_DMA_STORAGE_TYPE abcde_mask) const {
    ESP32_I2S_DMA_STORAGE_TYPE abcde = static_cast<ESP32_I2S_DMA_STORAGE_TYPE>(row_idx);
    if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::TYPE_DIRECT) { 
        abcde = abcde_mask & (~(1 << abcde)); 
    }
    return abcde << BITS_ADDR_OFFSET;
}

void MatrixPanel_I2S_DMA::applyShiftRegisterLineDecoderFixes(frameStruct* target_fb, uint8_t row_idx) {
    ESP32_I2S_DMA_STORAGE_TYPE *row = target_fb->rowBits[row_idx]->getDataPtr(0);

    if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::SM5266P) {
        int x_pixel = target_fb->rowBits[row_idx]->width - 16;
        uint16_t serialCount = 8;
        do {
            serialCount--;
            uint16_t latch = row[x_pixel] | (((((ESP32_I2S_DMA_STORAGE_TYPE)row_idx) % 8) == serialCount) << 1) << BITS_ADDR_OFFSET;
            row[x_pixel++] = latch | (0x05 << BITS_ADDR_OFFSET);
            row[x_pixel++] = latch | (0x04 << BITS_ADDR_OFFSET);
        } while (serialCount);
    }

    if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::SM5368) {
        int x_pixel = target_fb->rowBits[row_idx]->width - 1;
        uint16_t c = (row_idx == 0) ? BIT_C : 0x0000;
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel - 1)] |= c | BIT_B;
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)]     |= c | BIT_A | BIT_B;
    }
}

void MatrixPanel_I2S_DMA::applyRowControlPulses(frameStruct* target_fb, uint8_t row_idx) {
    uint8_t colouridx = target_fb->rowBits[row_idx]->colour_depth;
    do {
        --colouridx;
        ESP32_I2S_DMA_STORAGE_TYPE *row = target_fb->rowBits[row_idx]->getDataPtr(colouridx);

        if (m_cfg.driver == HUB75_I2S_CFG::shift_driver::DP3246) {
            row[ESP32_TX_FIFO_POSITION_ADJUST(target_fb->rowBits[row_idx]->width - 3)] |= BIT_LAT;
            row[ESP32_TX_FIFO_POSITION_ADJUST(target_fb->rowBits[row_idx]->width - 2)] |= BIT_LAT;
        }
        
        row[ESP32_TX_FIFO_POSITION_ADJUST(target_fb->rowBits[row_idx]->width - 1)] |= BIT_LAT;

        uint8_t _blank = m_cfg.latch_blanking;
        do {
            --_blank;
            row[ESP32_TX_FIFO_POSITION_ADJUST(0 + _blank)] |= BIT_OE;
            row[ESP32_TX_FIFO_POSITION_ADJUST(target_fb->rowBits[row_idx]->width - 1)] |= BIT_OE;
            row[ESP32_TX_FIFO_POSITION_ADJUST(target_fb->rowBits[row_idx]->width - _blank - 1)] |= BIT_OE;
        } while (_blank);
    } while (colouridx);
}

void MatrixPanel_I2S_DMA::clearFrameBuffer(bool _buff_id) {
    if (!initialized) return;

    frameStruct *target_fb = &frame_buffer[_buff_id];
    int row_idx = target_fb->rowBits.size();
    ESP32_I2S_DMA_STORAGE_TYPE abcde_mask = (row_idx == 2) ? 0x3 : 0xf;

    do {
        --row_idx;

        ESP32_I2S_DMA_STORAGE_TYPE *row = target_fb->rowBits[row_idx]->getDataPtr(0);
        ESP32_I2S_DMA_STORAGE_TYPE abcde = calculateRowAddressMask(row_idx, abcde_mask);

        int x_pixel = target_fb->rowBits[row_idx]->width * target_fb->rowBits[row_idx]->colour_depth;

        do {
            --x_pixel;
            if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::SM5266P) {
                row[x_pixel] = abcde & (0x18 << BITS_ADDR_OFFSET);
            } else if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::SM5368) {
                row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = 0x0000;
            } else {
                row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = abcde;
            }
        } while (x_pixel != target_fb->rowBits[row_idx]->width);

        uint8_t prev_row = (row_idx == 0) ? (ROWS_PER_FRAME - 1) : (row_idx - 1);
        abcde = calculateRowAddressMask(prev_row, abcde_mask);

        do {
            --x_pixel;
            if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::SM5266P) {
                row[x_pixel] = abcde & (0x18 << BITS_ADDR_OFFSET);
            } else if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::SM5368) {
                row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = 0x0000;
            } else {
                row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = abcde;
            }
        } while (x_pixel);

        applyShiftRegisterLineDecoderFixes(target_fb, row_idx);
        applyRowControlPulses(target_fb, row_idx);

#if defined(SPIRAM_DMA_BUFFER)
        Cache_WriteBack_Addr((uint32_t)row, target_fb->rowBits[row_idx]->getColorDepthSize(false));
#endif
    } while (row_idx);
}

/**
 * @brief Adjusts global brightness by setting Output Enable (OE) pulse widths.
 * 
 * Sets OE active (LOW) centered within each row's clock window.
 * The active pulse width scales linearly with brightness (0-255) across all 
 * bitplane buffers, preserving correct BCM color ratios without color shift.
 */
void MatrixPanel_I2S_DMA::setBrightnessOE(uint8_t brt, const int _buff_id)
{
  if (!initialized) return;

  frameStruct *target_fb = &frame_buffer[_buff_id];

  const uint8_t  _blank = m_cfg.latch_blanking;
  const uint8_t  _depth = target_fb->rowBits[0]->colour_depth;
  const uint16_t _width = target_fb->rowBits[0]->width;

  // Maximum usable pixel window for active display (excluding latch blanking margins)
  const int max_active_pixels = _width - (2 * _blank);
  if (max_active_pixels <= 0) return;

  // Calculate active OE window width (in pixel clocks) based on brightness
  int active_pixels = (max_active_pixels * brt) / 255;

  // Keep at least 1 pixel clock active for non-zero brightness to prevent dark drop-out
  if (brt > 0 && active_pixels == 0) {
    active_pixels = 1;
  }

  // Center the active (OE = LOW) window within the row duration
  const int x_coord_min = (_width - active_pixels) / 2;
  const int x_coord_max = x_coord_min + active_pixels;

  int row_idx = target_fb->rowBits.size();
  do {
    --row_idx;

    uint8_t colouridx = _depth;
    do {
      --colouridx;

      ESP32_I2S_DMA_STORAGE_TYPE *row = target_fb->rowBits[row_idx]->getDataPtr(colouridx);

      int x_coord = _width;
      do {
        --x_coord;

        // OE is active LOW: Enable output inside the calculated center window
        if (x_coord >= x_coord_min && x_coord < x_coord_max) {
          row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] &= BITMASK_OE_CLEAR;
        } else {
          row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE;
        }
      } while (x_coord);

    } while (colouridx);

#if defined(SPIRAM_DMA_BUFFER)
    ESP32_I2S_DMA_STORAGE_TYPE *row_ptr = target_fb->rowBits[row_idx]->getDataPtr(0);
    Cache_WriteBack_Addr((uint32_t)row_ptr, target_fb->rowBits[row_idx]->getColorDepthSize(false));
#endif
  } while (row_idx);
}

bool MatrixPanel_I2S_DMA::begin(int r1, int g1, int b1, int r2, int g2, int b2, int a, int b, int c, int d, int e, int lat, int oe, int clk) {
    if (initialized) return true;

    m_cfg.gpio.r1 = (gpio_num_t)r1; m_cfg.gpio.g1 = (gpio_num_t)g1; m_cfg.gpio.b1 = (gpio_num_t)b1;
    m_cfg.gpio.r2 = (gpio_num_t)r2; m_cfg.gpio.g2 = (gpio_num_t)g2; m_cfg.gpio.b2 = (gpio_num_t)b2;
    m_cfg.gpio.a  = (gpio_num_t)a;  m_cfg.gpio.b  = (gpio_num_t)b;  m_cfg.gpio.c  = (gpio_num_t)c;
    m_cfg.gpio.d  = (gpio_num_t)d;  m_cfg.gpio.e  = (gpio_num_t)e;
    m_cfg.gpio.lat = (gpio_num_t)lat; m_cfg.gpio.oe = (gpio_num_t)oe; m_cfg.gpio.clk = (gpio_num_t)clk;

    return begin();
}

bool MatrixPanel_I2S_DMA::begin(const HUB75_I2S_CFG &cfg) {
    if (initialized) return true;
    if (!setCfg(cfg)) return false;
    return begin();
}

bool MatrixPanel_I2S_DMA::validateConfiguration() const {
    if (m_cfg.mx_height % 2 != 0) {
        ESP_LOGE("begin()", "Error: m_cfg.mx_height must be an even number!");
        return false;
    }

    if (m_cfg.line_decoder == HUB75_I2S_CFG::line_driver::TYPE_DIRECT && (m_cfg.mx_height != 4 && m_cfg.mx_height != 8)) {
        ESP_LOGE("begin()", "Error: panel must be 2S or 4S to use TYPE_DIRECT line decoder!");
        return false;
    }

    return true;
}

bool MatrixPanel_I2S_DMA::begin() {
    if (initialized) return true;
    if (!config_set || !validateConfiguration()) return false;

    if (m_cfg.driver != HUB75_I2S_CFG::shift_driver::SHIFTREG) {
        shiftDriver(m_cfg);
    }

#if defined(SPIRAM_DMA_BUFFER)
    m_cfg.i2sspeed = HUB75_I2S_CFG::clk_speed::HZ_8M;
#endif

    if (!setupDMA(m_cfg)) return false;

    resetbuffers();
    flipDMABuffer();

    dma_bus.init();
    dma_bus.dma_transfer_start();

    return initialized;
}

uint8_t MatrixPanel_I2S_DMA::setLatBlanking(uint8_t pulses) {
    m_cfg.latch_blanking = std::min(std::max(pulses, static_cast<uint8_t>(1)), MAX_LAT_BLANKING);
    return m_cfg.latch_blanking;
}

#ifndef NO_FAST_FUNCTIONS
void MatrixPanel_I2S_DMA::hlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue) {
    if (!initialized || (x_coord + l) < 1 || y_coord < 0 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
        return;

    l = x_coord < 0 ? l + x_coord : l;
    x_coord = std::max<int16_t>(0, x_coord);
    l = ((x_coord + l) >= PIXELS_PER_ROW) ? (PIXELS_PER_ROW - x_coord) : l;

    DO_BRIGHTNESS_COMPENSATION()

    uint16_t _colourbitclear = BITMASK_RGB1_CLEAR;
    uint16_t _colourbitoffset = 0;

    if (y_coord >= ROWS_PER_FRAME) {
        _colourbitoffset = BITS_RGB2_OFFSET;
        _colourbitclear = BITMASK_RGB2_CLEAR;
        y_coord -= ROWS_PER_FRAME;
    }

    uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
    do {
        --colour_depth_idx;

        uint16_t mask = (1 << colour_depth_idx);
        uint16_t RGB_output_bits = 0;

        RGB_output_bits |= (bool)(blue_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(green_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(red_val & mask);
        RGB_output_bits <<= _colourbitoffset;

        ESP32_I2S_DMA_STORAGE_TYPE *p = fb->rowBits[y_coord]->getDataPtr(colour_depth_idx);

        int16_t _l = l;
        do {
            int16_t _x = x_coord + --_l;
            uint16_t &v = p[ESP32_TX_FIFO_POSITION_ADJUST(_x)];
            v &= _colourbitclear;
            v |= RGB_output_bits;
        } while (_l);
    } while (colour_depth_idx);
}

void MatrixPanel_I2S_DMA::vlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue) {
    if (!initialized || x_coord < 0 || (y_coord + l) < 1 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
        return;

    l = y_coord < 0 ? l + y_coord : l;
    y_coord = std::max<int16_t>(0, y_coord);
    l = ((y_coord + l) >= m_cfg.mx_height) ? (m_cfg.mx_height - y_coord) : l;

    DO_BRIGHTNESS_COMPENSATION()

    x_coord = ESP32_TX_FIFO_POSITION_ADJUST(x_coord);

    uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
    do {
        --colour_depth_idx;

        uint16_t mask = (1 << colour_depth_idx);
        uint16_t RGB_output_bits = 0;

        RGB_output_bits |= (bool)(blue_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(green_val & mask);
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(red_val & mask);

        int16_t _l = 0;
        int16_t _y = y_coord;
        uint16_t _colourbitclear = BITMASK_RGB1_CLEAR;

        do {
            uint16_t current_rgb_bits = RGB_output_bits;
            int16_t calc_y = _y;

            if (calc_y >= ROWS_PER_FRAME) {
                calc_y -= ROWS_PER_FRAME;
                _colourbitclear = BITMASK_RGB2_CLEAR;
                current_rgb_bits <<= BITS_RGB2_OFFSET;
            } else {
                _colourbitclear = BITMASK_RGB1_CLEAR;
            }

            ESP32_I2S_DMA_STORAGE_TYPE *p = fb->rowBits[calc_y]->getDataPtr(colour_depth_idx);

            p[x_coord] &= _colourbitclear;
            p[x_coord] |= current_rgb_bits;
            ++_y;
        } while (++_l != l);
    } while (colour_depth_idx);
}

void MatrixPanel_I2S_DMA::fillRectDMA(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
    if (h > 2 * w) {
        do {
            --w;
            vlineDMA(x + w, y, h, r, g, b);
        } while (w);
    } else {
        do {
            --h;
            hlineDMA(x, y + h, w, r, g, b);
        } while (h);
    }
}
#endif