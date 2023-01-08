#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#if defined(SPIRAM_DMA_BUFFER)
  // Sprite_TM saves the day again...
  // https://www.esp32.com/viewtopic.php?f=2&t=30584
  #include "rom/cache.h"
#endif 

/* This replicates same function in rowBitStruct, but due to induced inlining it might be MUCH faster 
 * when used in tight loops while method from struct could be flushed out of instruction cache between 
 * loop cycles do NOT forget about buff_id param if using this.
 */
#define getRowDataPtr(row, _dpth, buff_id) &(dma_buff.rowBits[row]->data[_dpth * dma_buff.rowBits[row]->width + buff_id*(dma_buff.rowBits[row]->width * dma_buff.rowBits[row]->colour_depth)])

/* We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
 * 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
 * Irrelevant for ESP32-S2 the way the FIFO ordering works is different - refer to page 679 of S2 technical reference manual
 */
#if defined (ESP32_THE_ORIG)
    #define ESP32_TX_FIFO_POSITION_ADJUST(x_coord)  (((x_coord) & 1U) ? (x_coord-1):(x_coord+1))
#else 
    #define ESP32_TX_FIFO_POSITION_ADJUST(x_coord)  x_coord
#endif 

/* This library is designed to take an 8 bit / 1 byte value (0-255) for each R G B colour sub-pixel. 
 * The PIXEL_COLOUR_DEPTH_BITS should always be '8' as a result.
 * However, if the library is to be used with lower colour depth (i.e. 6 bit colour), then we need to ensure the 8-bit value passed to the colour masking
 * is adjusted accordingly to ensure the LSB's are shifted left to MSB, by the difference. Otherwise the colours will be all screwed up.
 */
#if PIXEL_COLOUR_DEPTH_BITS > 8
    #error "Color depth bits cannot be greater than 8."
#elif PIXEL_COLOUR_DEPTH_BITS < 2 
    #error "Colour depth bits cannot be less than 2."
#endif

#if PIXEL_COLOUR_DEPTH_BITS != 8
  #define MASK_OFFSET (8 - PIXEL_COLOUR_DEPTH_BITS)
  #define PIXEL_COLOUR_MASK_BIT(colour_depth_index)   (1 << (colour_depth_index + MASK_OFFSET))
  //static constexpr uint8_t const MASK_OFFSET = 8-PIXEL_COLOUR_DEPTH_BITS;
#else
  #define PIXEL_COLOUR_MASK_BIT(colour_depth_index)   (1 << (colour_depth_index))
#endif

/*
    #if PIXEL_COLOUR_DEPTH_BITS < 8
        uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit colour (8 bits per RGB subpixel)
    #else
        uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
    #endif 
*/


bool MatrixPanel_I2S_DMA::allocateDMAmemory()
{

  ESP_LOGI("I2S-DMA", "Free heap: %d",   heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  ESP_LOGI("I2S-DMA", "Free SPIRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));  


  // Alright, theoretically we should be OK, so let us do this, so
  // lets allocate a chunk of memory for each row (a row could span multiple panels if chaining is in place)
  dma_buff.rowBits.reserve(ROWS_PER_FRAME);

    // iterate through number of rows, allocate memory for each
    size_t allocated_fb_memory = 0;
    for (int malloc_num =0; malloc_num < ROWS_PER_FRAME; ++malloc_num)
    {
        auto ptr = std::make_shared<rowBitStruct>(PIXELS_PER_ROW, PIXEL_COLOUR_DEPTH_BITS, m_cfg.double_buff);

        if (ptr->data == nullptr)
        {
            ESP_LOGE("I2S-DMA", "CRITICAL ERROR: Not enough memory for requested colour depth! Please reduce PIXEL_COLOUR_DEPTH_BITS value.\r\n");
            ESP_LOGE("I2S-DMA", "Could not allocate rowBitStruct %d!.\r\n", malloc_num);

            return false;
            // TODO: should we release all previous rowBitStructs here???
        }

        allocated_fb_memory += ptr->size();
        dma_buff.rowBits.emplace_back(ptr);     // save new rowBitStruct into rows vector
        ++dma_buff.rows;
    }
    ESP_LOGI("I2S-DMA", "Allocating %d bytes memory for DMA BCM framebuffer(s).", allocated_fb_memory);        
   
    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory that will meet or exceed the configured refresh rate
#if !defined(FORCE_COLOUR_DEPTH)    

    while(1) {           
        int psPerClock = 1000000000000UL/m_cfg.i2sspeed;
        int nsPerLatch = ((PIXELS_PER_ROW + CLKS_DURING_LATCH) * psPerClock) / 1000;

        // add time to shift out LSBs + LSB-MSB transition bit - this ignores fractions...
        int nsPerRow = PIXEL_COLOUR_DEPTH_BITS * nsPerLatch;

        // add time to shift out MSBs
        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOUR_DEPTH_BITS; i++)
            nsPerRow += (1<<(i - lsbMsbTransitionBit - 1)) * (PIXEL_COLOUR_DEPTH_BITS - i) * nsPerLatch;

        int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
        int actualRefreshRate = 1000000000UL/(nsPerFrame);
        calculated_refresh_rate = actualRefreshRate;

        ESP_LOGW("I2S-DMA", "lsbMsbTransitionBit of %d gives %d Hz refresh rate.", lsbMsbTransitionBit, actualRefreshRate);

        if (actualRefreshRate > m_cfg.min_refresh_rate) 
          break;
                  
        if(lsbMsbTransitionBit < PIXEL_COLOUR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }
#endif
 

  /***
   * Step 2a: lsbMsbTransition bit is now finalised - recalculate the DMA descriptor count required, which is used for
   *          memory allocation of the DMA linked list memory structure.
   */          
    int numDMAdescriptorsPerRow = 1;
    for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOUR_DEPTH_BITS; i++) {
        numDMAdescriptorsPerRow += (1<<(i - lsbMsbTransitionBit - 1));
    }

    ESP_LOGI("I2S-DMA", "Recalculated number of DMA descriptors per row: %d", numDMAdescriptorsPerRow);

    // Refer to 'DMA_LL_PAYLOAD_SPLIT' code in configureDMA() below to understand why this exists.
    // numDMAdescriptorsPerRow is also used to calculate descount which is super important in i2s_parallel_config_t SoC DMA setup. 
    if ( dma_buff.rowBits[0]->size() > DMA_MAX ) 
    {

        ESP_LOGW("I2S-DMA", "rowColorDepthStruct struct is too large, split DMA payload required. Adding %d DMA descriptors\n", PIXEL_COLOUR_DEPTH_BITS-1);

        numDMAdescriptorsPerRow += PIXEL_COLOUR_DEPTH_BITS-1; 
        // Note: If numDMAdescriptorsPerRow is even just one descriptor too large, DMA linked list will not correctly loop.
    }


  /***
   * Step 3: Allocate memory for DMA linked list, linking up each framebuffer row in sequence for GPIO output.
   */        

    // malloc the DMA linked list descriptors that i2s_parallel will need
    desccount = numDMAdescriptorsPerRow * ROWS_PER_FRAME;

    if (m_cfg.double_buff)
      dma_bus.enable_double_dma_desc();
      
    dma_bus.allocate_dma_desc_memory(desccount);

    // Just os we know
    initialized = true;

    return true;

} // end allocateDMAmemory()



void MatrixPanel_I2S_DMA::configureDMA(const HUB75_I2S_CFG& _cfg)
{

 //   lldesc_t *previous_dmadesc_a     = 0;
 //   lldesc_t *previous_dmadesc_b     = 0;
    int current_dmadescriptor_offset = 0;

    // HACK: If we need to split the payload in 1/2 so that it doesn't breach DMA_MAX, lets do it by the colour_depth.
    int num_dma_payload_colour_depths = PIXEL_COLOUR_DEPTH_BITS;
    if ( dma_buff.rowBits[0]->size() > DMA_MAX ) {
        num_dma_payload_colour_depths = 1;
    }

    // Fill DMA linked lists for both frames (as in, halves of the HUB75 panel) and if double buffering is enabled, link it up for both buffers.
    for(int row = 0; row < ROWS_PER_FRAME; row++) 
    {
        // first set of data is LSB through MSB, single pass (IF TOTAL SIZE < DMA_MAX) - all colour bits are displayed once, which takes care of everything below and including LSBMSB_TRANSITION_BIT
        // NOTE: size must be less than DMA_MAX - worst case for library: 16-bpp with 256 pixels per row would exceed this, need to break into two
        //link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, dma_buff.rowBits[row]->getDataPtr(), dma_buff.rowBits[row]->size(num_dma_payload_colour_depths));
       //   previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

        dma_bus.create_dma_desc_link(dma_buff.rowBits[row]->getDataPtr(0, 0), dma_buff.rowBits[row]->size(num_dma_payload_colour_depths), false);

        if (m_cfg.double_buff) 
        {
          dma_bus.create_dma_desc_link(dma_buff.rowBits[row]->getDataPtr(0, 1), dma_buff.rowBits[row]->size(num_dma_payload_colour_depths), true);          
        }

        current_dmadescriptor_offset++;

        // If the number of pixels per row is too great for the size of a DMA payload, so we need to split what we were going to send above.
        if ( dma_buff.rowBits[0]->size() > DMA_MAX )
        {
          
          for (int cd = 1; cd < PIXEL_COLOUR_DEPTH_BITS; cd++) 
          {
            dma_bus.create_dma_desc_link(dma_buff.rowBits[row]->getDataPtr(cd, 0), dma_buff.rowBits[row]->size(num_dma_payload_colour_depths), false);            

            if (m_cfg.double_buff) {
              dma_bus.create_dma_desc_link(dma_buff.rowBits[row]->getDataPtr(cd, 1), dma_buff.rowBits[row]->size(num_dma_payload_colour_depths), true);            
               }

            current_dmadescriptor_offset++;     

          } // additional linked list items           
        }  // row depth struct


        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOUR_DEPTH_BITS; i++) 
        {
            // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
            // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB

            for(int k=0; k < (1<<(i - lsbMsbTransitionBit - 1)); k++) 
            {
                dma_bus.create_dma_desc_link(dma_buff.rowBits[row]->getDataPtr(i, 0),  dma_buff.rowBits[row]->size(PIXEL_COLOUR_DEPTH_BITS - i), false);

                if (m_cfg.double_buff) {
                  dma_bus.create_dma_desc_link(dma_buff.rowBits[row]->getDataPtr(i, 1),  dma_buff.rowBits[row]->size(PIXEL_COLOUR_DEPTH_BITS - i), true );
                }

                current_dmadescriptor_offset++;

            } // end colour depth ^ 2 linked list
        } // end colour depth loop

    } // end frame rows

    ESP_LOGI("I2S-DMA", "%d DMA descriptors linked to buffer data.", current_dmadescriptor_offset);

//  
//    Setup DMA and Output to GPIO
//
      auto bus_cfg = dma_bus.config();         // バス設定用の構造体を取得します。
      
      bus_cfg.bus_freq = _cfg.i2sspeed; 
      bus_cfg.pin_wr   = m_cfg.gpio.clk;      // WR を接続しているピン番号
      
      bus_cfg.pin_d0	 = m_cfg.gpio.r1;	
      bus_cfg.pin_d1   = m_cfg.gpio.g1;
      bus_cfg.pin_d2   = m_cfg.gpio.b1;
      bus_cfg.pin_d3   = m_cfg.gpio.r2;
      bus_cfg.pin_d4   = m_cfg.gpio.g2;
      bus_cfg.pin_d5   = m_cfg.gpio.b2;
      bus_cfg.pin_d6   = m_cfg.gpio.lat;
      bus_cfg.pin_d7   = m_cfg.gpio.oe;
      bus_cfg.pin_d8   = m_cfg.gpio.a;
      bus_cfg.pin_d9   = m_cfg.gpio.b;
      bus_cfg.pin_d10  = m_cfg.gpio.c;
      bus_cfg.pin_d11  = m_cfg.gpio.d;
      bus_cfg.pin_d12  = m_cfg.gpio.e;
      bus_cfg.pin_d13  = -1;
      bus_cfg.pin_d14  = -1;
      bus_cfg.pin_d15  = -1;

      #if defined(SPIRAM_DMA_BUFFER)      
        bus_cfg.psram_clk_override = true;
      #endif

      dma_bus.config(bus_cfg);   

      dma_bus.init();      

      dma_bus.dma_transfer_start();

      flipDMABuffer(); // display back buffer 0, draw to 1, ignored if double buffering isn't enabled.

    //i2s_parallel_send_dma(ESP32_I2S_DEVICE, &dmadesc_a[0]);
    ESP_LOGI("I2S-DMA", "DMA setup completed");
        
} // end initMatrixDMABuff


/* There are 'bits' set in the frameStruct that we simply don't need to set every single time we change a pixel / DMA buffer co-ordinate.
 *  For example, the bits that determine the address lines, we don't need to set these every time. Once they're in place, and assuming we
 *  don't accidentally clear them, then we don't need to set them again.
 *  So to save processing, we strip this logic out to the absolute bare minimum, which is toggling only the R,G,B pixels (bits) per co-ord.
 *
 *  Critical dependency: That 'updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)' has been run at least once over the
 *                       entire frameBuffer to ensure all the non R,G,B bitmasks are in place (i.e. like OE, Address Lines etc.)
 *
 *  Note: If you change the brightness with setBrightness() you MUST then clearScreen() and repaint / flush the entire framebuffer.
 */

/** @brief - Update pixel at specific co-ordinate in the DMA buffer
 *  this is the main method used to update DMA buffer on pixel-by-pixel level so it must be fast, real fast!
 *  Let's put it into IRAM to avoid situations when it could be flushed out of instruction cache
 *  and had to be read from spi-flash over and over again.
 *  Yes, it is always a tradeoff between memory/speed/size, but compared to DMA-buffer size is not a big deal
 * 
 *  Note: Cannot pass a negative co-ord as it makes no sense in the DMA bit array lookup.
 */
void IRAM_ATTR MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint16_t x_coord, uint16_t y_coord, uint8_t red, uint8_t green, uint8_t blue)
{
    if ( !initialized ) return;

  /* 1) Check that the co-ordinates are within range, or it'll break everything big time.
  * Valid co-ordinates are from 0 to (MATRIX_XXXX-1)
  */
  if ( x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height) {
    return;
  }

  /* LED Brightness Compensation. Because if we do a basic "red & mask" for example, 
     * we'll NEVER send the dimmest possible colour, due to binary skew.
     * i.e. It's almost impossible for colour_depth_idx of 0 to be sent out to the MATRIX unless the 'value' of a color is exactly '1'
   * https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
     */
#ifndef NO_CIE1931
    red   = lumConvTab[red];
    green = lumConvTab[green];
    blue  = lumConvTab[blue];   
#endif

    /* When using the drawPixel, we are obviously only changing the value of one x,y position, 
     * however, the two-scan panels paint TWO lines at the same time
     * and this reflects the parallel in-DMA-memory data structure of uint16_t's that are getting
     * pumped out at high speed.
     * 
     * So we need to ensure we persist the bits (8 of them) of the uint16_t for the row we aren't changing.
     * 
     * The DMA buffer order has also been reversed (refer to the last code in this function)
     * so we have to check for this and check the correct position of the MATRIX_DATA_STORAGE_TYPE
     * data.
     */
/*
#if defined (ESP32_THE_ORIG)
    // We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
    // 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
    // Irrelevant for ESP32-S2 the way the FIFO ordering works is different - refer to page 679 of S2 technical reference manual
    x_coord & 1U ? --x_coord : ++x_coord;
#endif 
*/
    x_coord = ESP32_TX_FIFO_POSITION_ADJUST(x_coord);
    
    
    uint16_t _colourbitclear = BITMASK_RGB1_CLEAR, _colourbitoffset = 0;

    if (y_coord >= ROWS_PER_FRAME){    // if we are drawing to the bottom part of the panel
      _colourbitoffset = BITS_RGB2_OFFSET;
      _colourbitclear  = BITMASK_RGB2_CLEAR;
      y_coord -= ROWS_PER_FRAME;
    }

    // Iterating through colour depth bits, which we assume are 8 bits per RGB subpixel (24bpp)
    uint8_t colour_depth_idx = PIXEL_COLOUR_DEPTH_BITS;
    do {
        --colour_depth_idx;
/*        
//        uint8_t mask = (1 << (colour_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST)); // expect 24 bit colour (8 bits per RGB subpixel)
    #if PIXEL_COLOUR_DEPTH_BITS < 8
        uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit colour (8 bits per RGB subpixel)
    #else
        uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
    #endif      
*/
        uint8_t mask = PIXEL_COLOUR_MASK_BIT(colour_depth_idx);
        uint16_t RGB_output_bits = 0;

        /* Per the .h file, the order of the output RGB bits is:
          * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1     */
        RGB_output_bits |= (bool)(blue & mask);   // --B
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(green & mask);  // -BG
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(red & mask);    // BGR
        RGB_output_bits <<= _colourbitoffset;      // shift colour bits to the required position


        // Get the contents at this address,
        // it would represent a vector pointing to the full row of pixels for the specified color depth bit at Y coordinate
        ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, colour_depth_idx, back_buffer_id);


        // We need to update the correct uint16_t word in the rowBitStruct array pointing to a specific pixel at X - coordinate
        p[x_coord] &= _colourbitclear;   // reset RGB bits
        p[x_coord] |= RGB_output_bits;  // set new RGB bits

        #if defined(SPIRAM_DMA_BUFFER)          
            Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE)) ;
        #endif 

    } while(colour_depth_idx);  // end of colour depth loop (8)
} // updateMatrixDMABuffer (specific co-ords change)


/* Update the entire buffer with a single specific colour - quicker */
void MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  if ( !initialized ) return;
  
    /* https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/ */     
#ifndef NO_CIE1931
    red     = lumConvTab[red];
    green   = lumConvTab[green];
    blue    = lumConvTab[blue];     
#endif

  for(uint8_t colour_depth_idx=0; colour_depth_idx<PIXEL_COLOUR_DEPTH_BITS; colour_depth_idx++)  // color depth - 8 iterations
  {
    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    uint16_t RGB_output_bits = 0;
//    uint8_t mask = (1 << colour_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST); // 24 bit colour
    // #if PIXEL_COLOUR_DEPTH_BITS < 8
    //     uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
    // #else
    //     uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit colour (8 bits per RGB subpixel)
    // #endif      

    uint8_t mask = PIXEL_COLOUR_MASK_BIT(colour_depth_idx);

    /* Per the .h file, the order of the output RGB bits is:
     * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1      */
    RGB_output_bits |= (bool)(blue & mask);   // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green & mask);  // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red & mask);    // BGR
    
    // Duplicate and shift across so we have have 6 populated bits of RGB1 and RGB2 pin values suitable for DMA buffer
    RGB_output_bits |= RGB_output_bits << BITS_RGB2_OFFSET;  //BGRBGR
    
    //Serial.printf("Fill with: 0x%#06x\n", RGB_output_bits);

    // iterate rows
    int matrix_frame_parallel_row = dma_buff.rowBits.size();
    do {
      --matrix_frame_parallel_row;

      // The destination for the pixel row bitstream
      ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(matrix_frame_parallel_row, colour_depth_idx, back_buffer_id);

      // iterate pixels in a row
      int x_coord=dma_buff.rowBits[matrix_frame_parallel_row]->width;
      do { 
        --x_coord;
        p[x_coord] &= BITMASK_RGB12_CLEAR;  // reset colour bits
        p[x_coord] |= RGB_output_bits;      // set new colour bits


        #if defined(SPIRAM_DMA_BUFFER)          
            Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE)) ;
        #endif 

      } while(x_coord);

    } while(matrix_frame_parallel_row); // end row iteration
  } // colour depth loop (8)
} // updateMatrixDMABuffer (full frame paint)

/**
 * @brief - clears and reinitializes colour/control data in DMA buffs
 * When allocated, DMA buffs might be dirty, so we need to blank it and initialize ABCDE,LAT,OE control bits.
 * Those control bits are constants during the entire DMA sweep and never changed when updating just pixel colour data
 * so we could set it once on DMA buffs initialization and forget. 
 * This effectively clears buffers to blank BLACK and makes it ready to display output.
 * (Brightness control via OE bit manipulation is another case) - this must be done as well seperately!
 */
void MatrixPanel_I2S_DMA::clearFrameBuffer(bool _buff_id){
  if (!initialized)
    return;

  // we start with iterating all rows in dma_buff structure
  int row_idx = dma_buff.rowBits.size();
  do {
    --row_idx;
    
    ESP32_I2S_DMA_STORAGE_TYPE* row = dma_buff.rowBits[row_idx]->getDataPtr(0, _buff_id);   // set pointer to the HEAD of a buffer holding data for the entire matrix row

    ESP32_I2S_DMA_STORAGE_TYPE abcde = (ESP32_I2S_DMA_STORAGE_TYPE)row_idx;
    abcde <<= BITS_ADDR_OFFSET;    // shift row y-coord to match ABCDE bits in vector from 8 to 12

    // get last pixel index in a row of all colourdepths
    int x_pixel = dma_buff.rowBits[row_idx]->width * dma_buff.rowBits[row_idx]->colour_depth;
    //Serial.printf(" from pixel %d, ", x_pixel);

    // fill all x_pixels except colour_index[0] (LSB) ones, this also clears all colour data to 0's black
    do {
      --x_pixel;
      
      if ( m_cfg.driver == HUB75_I2S_CFG::SM5266P) {
        // modifications here for row shift register type SM5266P 
        // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164
        row[x_pixel] = abcde & (0x18 << BITS_ADDR_OFFSET); // mask out the bottom 3 bits which are the clk di bk inputs  
      } else {        
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = abcde;
      }
   //   ESP_LOGI("", "x pixel 1: %d", x_pixel);
    } while(x_pixel!=dma_buff.rowBits[row_idx]->width && x_pixel);

    // colour_index[0] (LSB) x_pixels must be "marked" with a previous's row address, 'cause  it is used to display
    //  previous row while we pump in LSB's for a new row
    abcde = ((ESP32_I2S_DMA_STORAGE_TYPE)row_idx-1) << BITS_ADDR_OFFSET;
    do {
      --x_pixel;
      
      if ( m_cfg.driver == HUB75_I2S_CFG::SM5266P) {
        // modifications here for row shift register type SM5266P 
        // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164
        row[x_pixel] = abcde & (0x18 << BITS_ADDR_OFFSET); // mask out the bottom 3 bits which are the clk di bk inputs  
      } else {        
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = abcde;
      }   
      //row[x_pixel] = abcde;
  //    ESP_LOGI("", "x pixel 2: %d", x_pixel);
    } while(x_pixel);
    
    
    // modifications here for row shift register type SM5266P 
    // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164    
    if ( m_cfg.driver == HUB75_I2S_CFG::SM5266P) {  
        uint16_t serialCount;
        uint16_t latch;
        x_pixel = dma_buff.rowBits[row_idx]->width - 16; // come back 8*2 pixels to allow for 8 writes
        serialCount = 8;
        do{
            serialCount--;
            latch = row[x_pixel] | (((((ESP32_I2S_DMA_STORAGE_TYPE)row_idx) % 8) == serialCount) << 1) << BITS_ADDR_OFFSET; // data on 'B'
            row[x_pixel++] = latch| (0x05<< BITS_ADDR_OFFSET); // clock high on 'A'and BK high for update
            row[x_pixel++] = latch| (0x04<< BITS_ADDR_OFFSET); // clock low on 'A'and BK high for update
        } while (serialCount);
    } // end SM5266P    
    

    // let's set LAT/OE control bits for specific pixels in each color_index subrows
    // Need to consider the original ESP32's (WROOM) DMA TX FIFO reordering of bytes...
    uint8_t colouridx = dma_buff.rowBits[row_idx]->colour_depth;
    do {
      --colouridx;

      // switch pointer to a row for a specific color index
      row = dma_buff.rowBits[row_idx]->getDataPtr(colouridx, _buff_id);

      /*
      #if defined(ESP32_THE_ORIG)
        // We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
        // 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
        // Irrelevant for ESP32-S2 the way the FIFO ordering works is different - refer to page 679 of S2 technical reference manual
        row[dma_buff.rowBits[row_idx]->width - 2] |= BIT_LAT;   // -2 in the DMA array is actually -1 when it's reordered by TX FIFO                  
      #else
        // -1 works better on ESP32-S2 ? Because bytes get sent out in order...
        row[dma_buff.rowBits[row_idx]->width - 1] |= BIT_LAT;   // -1 pixel to compensate array index starting at 0                 
      #endif
      */
      row[ESP32_TX_FIFO_POSITION_ADJUST(dma_buff.rowBits[row_idx]->width - 1)] |= BIT_LAT;   // -1 pixel to compensate array index starting at 0     

      //ESP32_TX_FIFO_POSITION_ADJUST(dma_buff.rowBits[row_idx]->width - 1)

      // need to disable OE before/after latch to hide row transition
      // Should be one clock or more before latch, otherwise can get ghosting
      uint8_t _blank = m_cfg.latch_blanking;
      do {
        --_blank;
      /*
      #if defined(ESP32_THE_ORIG)  
            // Original ESP32 WROOM FIFO Ordering Sucks
            uint8_t _blank_row_tx_fifo_tmp = 0 + _blank;
            (_blank_row_tx_fifo_tmp & 1U) ? --_blank_row_tx_fifo_tmp : ++_blank_row_tx_fifo_tmp; 
            row[_blank_row_tx_fifo_tmp] |= BIT_OE;
            
            _blank_row_tx_fifo_tmp = dma_buff.rowBits[row_idx]->width - _blank - 1; // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0
            (_blank_row_tx_fifo_tmp & 1U) ? --_blank_row_tx_fifo_tmp : ++_blank_row_tx_fifo_tmp; 
            row[_blank_row_tx_fifo_tmp] |= BIT_OE;
      #else      
            row[0 + _blank] |= BIT_OE;
            row[dma_buff.rowBits[row_idx]->width - _blank - 1 ] |= BIT_OE;    // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0
      #endif
      */

      row[ESP32_TX_FIFO_POSITION_ADJUST(0 + _blank)] |= BIT_OE; // disable output
	  row[ESP32_TX_FIFO_POSITION_ADJUST(dma_buff.rowBits[row_idx]->width - 1)] |= BIT_OE; // disable output
      row[ESP32_TX_FIFO_POSITION_ADJUST(dma_buff.rowBits[row_idx]->width - _blank - 1)] |= BIT_OE;    // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0


      } while (_blank);

    } while(colouridx);



    #if defined(SPIRAM_DMA_BUFFER)          
        Cache_WriteBack_Addr((uint32_t)row, sizeof(ESP32_I2S_DMA_STORAGE_TYPE) * ((dma_buff.rowBits[row_idx]->width * dma_buff.rowBits[row_idx]->colour_depth)-1)) ;
    #endif 
    

  } while(row_idx);
}

/**
 * @brief - reset OE bits in DMA buffer in a way to control brightness
 * @param brt - brightness level from 0 to row_width
 * @param _buff_id - buffer id to control
 */
void MatrixPanel_I2S_DMA::brtCtrlOE(int brt, const bool _buff_id){
	/*
  if (!initialized)
    return;

  if (brt > PIXELS_PER_ROW - (MAX_LAT_BLANKING + 2))   // can't control values larger than (row_width - latch_blanking) to avoid ongoing issues being raised about brightness and ghosting.
    brt = PIXELS_PER_ROW   - (MAX_LAT_BLANKING + 2);   // +2 for a bit of buffer...

  if (brt < 0)
    brt = 0;

  // start with iterating all rows in dma_buff structure
  int row_idx = dma_buff.rowBits.size();
  do {
    --row_idx;

    // let's set OE control bits for specific pixels in each color_index subrows
    uint8_t colouridx = dma_buff.rowBits[row_idx]->colour_depth;
    do {
      --colouridx;

      // switch pointer to a row for a specific color index
      ESP32_I2S_DMA_STORAGE_TYPE* row = dma_buff.rowBits[row_idx]->getDataPtr(colouridx, _buff_id);

      int x_coord = dma_buff.rowBits[row_idx]->width;
      do {
        --x_coord;
        
       // clear OE bit for all other pixels
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] &= BITMASK_OE_CLEAR;       

        // Brightness control via OE toggle - disable matrix output at specified x_coord
        if((colouridx > lsbMsbTransitionBit || !colouridx) && ((x_coord) >= brt)){
          row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE; // Disable output after this point.
          continue;  
        }
        // special case for the bits *after* LSB through (lsbMsbTransitionBit) - OE is output after data is shifted, so need to set OE to fractional brightness
        if(colouridx && colouridx <= lsbMsbTransitionBit) {
            // divide brightness in half for each bit below lsbMsbTransitionBit
            int lsbBrightness = brt >> (lsbMsbTransitionBit - colouridx + 1);
            if((x_coord) >= lsbBrightness) {
                row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE;  // Disable output after this point.
                continue;
            }
        }

 
      } while(x_coord);

      // need to disable OE before/after latch to hide row transition
      // Should be one clock or more before latch, otherwise can get ghosting
      uint8_t _blank = m_cfg.latch_blanking;
      do {
        --_blank;


      row[ESP32_TX_FIFO_POSITION_ADJUST(0 + _blank)] |= BIT_OE;    
      


        //row[0 + _blank] |= BIT_OE;
        // no need, has been done already
        //row[dma_buff.rowBits[row_idx]->width - _blank - 3 ] |= BIT_OE;    // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0
      } while (_blank);

    } while(colouridx);

    // switch pointer to a row for a specific color index
    #if defined(SPIRAM_DMA_BUFFER)          
        ESP32_I2S_DMA_STORAGE_TYPE* row_hack = dma_buff.rowBits[row_idx]->getDataPtr(colouridx, _buff_id);    
        Cache_WriteBack_Addr((uint32_t)row_hack, sizeof(ESP32_I2S_DMA_STORAGE_TYPE) * ((dma_buff.rowBits[row_idx]->width * dma_buff.rowBits[row_idx]->colour_depth)-1)) ;
    #endif 


  } while(row_idx);
  */
}




/**
 * @brief - reset OE bits in DMA buffer in a way to control brightness
 * @param brt - brightness level from 0 to 255 - NOT MATRIX_WIDTH
 * @param _buff_id - buffer id to control
 */
void MatrixPanel_I2S_DMA::brtCtrlOEv2(uint8_t brt, const int _buff_id) {
	
  if (!initialized)
    return;

/*
  int x_coord_oe_adjust_stop_point = PIXELS_PER_ROW - 
	
  if (brt > PIXELS_PER_ROW - (MAX_LAT_BLANKING + 2))   // can't control values larger than (row_width - latch_blanking) to avoid ongoing issues being raised about brightness and ghosting.
    brt = PIXELS_PER_ROW   - (MAX_LAT_BLANKING + 2);   // +2 for a bit of buffer...

  if (brt < 0)
    brt = 0;
*/

  int brightness_in_x_pixels = PIXELS_PER_ROW * ((float)brt/256);
  
  //Serial.println(brightness_in_x_pixels, DEC);
  uint8_t _blank 			 = m_cfg.latch_blanking; // don't want to inadvertantly blast over this


  // start with iterating all rows in dma_buff structure
  int row_idx = dma_buff.rowBits.size();
  do {
    --row_idx;

    // let's set OE control bits for specific pixels in each color_index subrows
    uint8_t colouridx = dma_buff.rowBits[row_idx]->colour_depth;
    do {
      --colouridx;

      // switch pointer to a row for a specific color index
      ESP32_I2S_DMA_STORAGE_TYPE* row = dma_buff.rowBits[row_idx]->getDataPtr(colouridx, _buff_id);

      int x_coord = dma_buff.rowBits[row_idx]->width;
      do {
        --x_coord;
		
		if (x_coord <= _blank) // Can't touch blanking. They need to stay blank. 
		{
			row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE;  // Disable output after this point.		
		}
		else if(x_coord >= (PIXELS_PER_ROW - _blank - 1) )  // Can't touch blanking. They need to stay blank.
		{
			row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE;  // Disable output after this point.					
		}
		else if (x_coord < brightness_in_x_pixels) 
		{
			row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] &= BITMASK_OE_CLEAR;						
		}
		else
		{
			row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE;  // Disable output after this point.	
		}

        // clear OE bit for all other pixels (that is, turn on output)
        //row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] &= BITMASK_OE_CLEAR;       
		
		/*

        // Brightness control via OE toggle - disable matrix output at specified x_coord
        if((colouridx > lsbMsbTransitionBit || !colouridx) && ((x_coord) >= brt)){
          row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE; // Disable output after this point.
          continue;  
        }
        // special case for the bits *after* LSB through (lsbMsbTransitionBit) - OE is output after data is shifted, so need to set OE to fractional brightness
        if(colouridx && colouridx <= lsbMsbTransitionBit) {
            // divide brightness in half for each bit below lsbMsbTransitionBit
            int lsbBrightness = brt >> (lsbMsbTransitionBit - colouridx + 1);
            if((x_coord) >= lsbBrightness) {
                row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE;  // Disable output after this point.
                continue;
            }
        }
		*/
 
      } while(x_coord);
	  
    } while(colouridx);

    // switch pointer to a row for a specific color index
    #if defined(SPIRAM_DMA_BUFFER)          
        ESP32_I2S_DMA_STORAGE_TYPE* row_hack = dma_buff.rowBits[row_idx]->getDataPtr(colouridx, _buff_id);    
        Cache_WriteBack_Addr((uint32_t)row_hack, sizeof(ESP32_I2S_DMA_STORAGE_TYPE) * ((dma_buff.rowBits[row_idx]->width * dma_buff.rowBits[row_idx]->colour_depth)-1)) ;
    #endif 
  } while(row_idx);
  
}


/*
 *  overload for compatibility
 */
 
bool MatrixPanel_I2S_DMA::begin(int r1, int g1, int b1, int r2, int g2, int b2, int a, int b, int c, int d, int e, int lat, int oe, int clk) {

  // RGB
  m_cfg.gpio.r1 = r1; m_cfg.gpio.g1 = g1; m_cfg.gpio.b1 = b1;
  m_cfg.gpio.r2 = r2; m_cfg.gpio.g2 = g2; m_cfg.gpio.b2 = b2;
  
  // Line Select
  m_cfg.gpio.a = a; m_cfg.gpio.b = b; m_cfg.gpio.c = c;
  m_cfg.gpio.d = d; m_cfg.gpio.e = e; 
  
  // Clock & Control
  m_cfg.gpio.lat = lat; m_cfg.gpio.oe = oe; m_cfg.gpio.clk = clk;

  return begin();
}


/**
 * @brief - Sets how many clock cycles to blank OE before/after LAT signal change
 * @param uint8_t pulses - clocks before/after OE
 * default is DEFAULT_LAT_BLANKING
 * Max is MAX_LAT_BLANKING
 * @returns - new value for m_cfg.latch_blanking
 */
uint8_t MatrixPanel_I2S_DMA::setLatBlanking(uint8_t pulses){
  if (pulses > MAX_LAT_BLANKING)
    pulses = MAX_LAT_BLANKING;

  if (!pulses)
    pulses = DEFAULT_LAT_BLANKING;

  m_cfg.latch_blanking = pulses;
  
  // remove brightness var for now.
  //setPanelBrightness(brightness);    // set brightness to reset OE bits to the values matching new LAT blanking setting
  return m_cfg.latch_blanking;
}


#ifndef NO_FAST_FUNCTIONS
/**
 * @brief - update DMA buff drawing horizontal line at specified coordinates
 * @param x_coord - line start coordinate x
 * @param y_coord - line start coordinate y
 * @param l - line length
 * @param r,g,b, - RGB888 color
 */
void MatrixPanel_I2S_DMA::hlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue){
  if ( !initialized )
    return;

  if ( x_coord < 0 || y_coord < 0 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
    return;


  l = ( (x_coord + l) >= PIXELS_PER_ROW ) ? (PIXELS_PER_ROW - x_coord):l; 

  //if (x_coord+l > PIXELS_PER_ROW)
//    l = PIXELS_PER_ROW - x_coord + 1;     // reset width to end of row

  /* LED Brightness Compensation */
#ifndef NO_CIE1931
    red   = lumConvTab[red];
    green = lumConvTab[green];
    blue  = lumConvTab[blue];   
#endif

  uint16_t _colourbitclear = BITMASK_RGB1_CLEAR, _colourbitoffset = 0;

  if (y_coord >= ROWS_PER_FRAME){    // if we are drawing to the bottom part of the panel
    _colourbitoffset = BITS_RGB2_OFFSET;
    _colourbitclear  = BITMASK_RGB2_CLEAR;
    y_coord -= ROWS_PER_FRAME;
  }

  // Iterating through color depth bits (8 iterations)
  uint8_t colour_depth_idx = PIXEL_COLOUR_DEPTH_BITS;
  do {
    --colour_depth_idx;

    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    uint16_t RGB_output_bits = 0;
//    uint8_t mask = (1 << colour_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST);
    // #if PIXEL_COLOUR_DEPTH_BITS < 8
    //     uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
    // #else
    //     uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
    // #endif      
    uint8_t mask = PIXEL_COLOUR_MASK_BIT(colour_depth_idx);

    /* Per the .h file, the order of the output RGB bits is:
      * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1     */
    RGB_output_bits |= (bool)(blue & mask);   // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green & mask);  // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red & mask);    // BGR
    RGB_output_bits <<= _colourbitoffset;      // shift color bits to the required position

    // Get the contents at this address,
    // it would represent a vector pointing to the full row of pixels for the specified color depth bit at Y coordinate
    ESP32_I2S_DMA_STORAGE_TYPE *p = dma_buff.rowBits[y_coord]->getDataPtr(colour_depth_idx, back_buffer_id);
    // inlined version works slower here, dunno why :(
    // ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, colour_depth_idx, back_buffer_id);

    int16_t _l = l;
    do {                 // iterate pixels in a row
        int16_t _x = x_coord + --_l;

  /*        
    #if defined(ESP32_THE_ORIG)
            // Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
            uint16_t &v = p[_x & 1U ? --_x : ++_x];
    #else
            // ESP 32 doesn't need byte flipping for TX FIFO.
            uint16_t &v = p[_x];        
    #endif      
  */
        uint16_t &v = p[ESP32_TX_FIFO_POSITION_ADJUST(_x)];

        v &= _colourbitclear;      // reset color bits
        v |= RGB_output_bits;    // set new color bits
    } while(_l);        // iterate pixels in a row
  } while(colour_depth_idx);  // end of color depth loop (8)
} // hlineDMA()


/**
 * @brief - update DMA buff drawing vertical line at specified coordinates
 * @param x_coord - line start coordinate x
 * @param y_coord - line start coordinate y
 * @param l - line length
 * @param r,g,b, - RGB888 color
 */
void MatrixPanel_I2S_DMA::vlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue){
  if ( !initialized )
    return;

  if ( x_coord < 0 || y_coord < 0 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
    return;

  // check for a length that goes beyond the height of the screen! Array out of bounds dma memory changes = screwed output #163
  l = ( (y_coord + l) >= m_cfg.mx_height ) ? (m_cfg.mx_height - y_coord):l; 
  //if (y_coord + l > m_cfg.mx_height)
  ///    l = m_cfg.mx_height - y_coord + 1;     // reset width to end of col

  /* LED Brightness Compensation */
#ifndef NO_CIE1931
    red   = lumConvTab[red];
    green = lumConvTab[green];
    blue  = lumConvTab[blue];   
#endif

/*
#if defined(ESP32_THE_ORIG)
  // Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering 
  x_coord & 1U ? --x_coord : ++x_coord;
#endif  
*/
  x_coord = ESP32_TX_FIFO_POSITION_ADJUST(x_coord);

  uint8_t colour_depth_idx = PIXEL_COLOUR_DEPTH_BITS;
  do {    // Iterating through color depth bits (8 iterations)
    --colour_depth_idx;

    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
//    uint8_t mask = (1 << colour_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST);
    // #if PIXEL_COLOUR_DEPTH_BITS < 8
    //     uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
    // #else
    //     uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
    // #endif      

    uint8_t mask = PIXEL_COLOUR_MASK_BIT(colour_depth_idx);
    uint16_t RGB_output_bits = 0;

    /* Per the .h file, the order of the output RGB bits is:
    * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1   */
    RGB_output_bits |= (bool)(blue & mask);   // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green & mask);  // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red & mask);    // BGR

    int16_t _l = 0, _y = y_coord;
    uint16_t _colourbitclear = BITMASK_RGB1_CLEAR;
    do {    // iterate pixels in a column

      if (_y >= ROWS_PER_FRAME){    // if y-coord overlapped bottom-half panel
        _y -= ROWS_PER_FRAME;
        _colourbitclear  = BITMASK_RGB2_CLEAR;
        RGB_output_bits <<= BITS_RGB2_OFFSET;
      }

      // Get the contents at this address,
      // it would represent a vector pointing to the full row of pixels for the specified color depth bit at Y coordinate
      //ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(_y, colour_depth_idx, back_buffer_id);
      ESP32_I2S_DMA_STORAGE_TYPE *p = dma_buff.rowBits[_y]->getDataPtr(colour_depth_idx, back_buffer_id);      

      p[x_coord] &= _colourbitclear;   // reset RGB bits
      p[x_coord] |= RGB_output_bits;  // set new RGB bits
      ++_y;
    } while(++_l!=l);        // iterate pixels in a col
  } while(colour_depth_idx);  // end of color depth loop (8)
} // vlineDMA()


/**
 * @brief - update DMA buff drawing a rectangular at specified coordinates
 * this works much faster than multiple consecutive per-pixel calls to updateMatrixDMABuffer()
 * @param int16_t x, int16_t y - coordinates of a top-left corner
 * @param int16_t w, int16_t h - width and height of a rectangular, min is 1 px
 * @param uint8_t r - RGB888 color
 * @param uint8_t g - RGB888 color
 * @param uint8_t b - RGB888 color
 */
void MatrixPanel_I2S_DMA::fillRectDMA(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b){

  // h-lines are >2 times faster than v-lines
  // so will use it only for tall rects with h >2w
  if (h>2*w){
    // draw using v-lines
    do {
      --w;
      vlineDMA(x+w, y, h, r,g,b);
    } while(w);
  } else {
    // draw using h-lines
    do {
      --h;
      hlineDMA(x, y+h, w, r,g,b);
    } while(h);
  }
}

#endif  // NO_FAST_FUNCTIONS
