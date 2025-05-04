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
// #define getRowDataPtr(row, _dpth, buff_id) &(dma_buff.rowBits[row]->data[_dpth * dma_buff.rowBits[row]->width + buff_id*(dma_buff.rowBits[row]->width * dma_buff.rowBits[row]->colour_depth)])

// BufferID is now ignored, seperate global pointer pointer!
#define getRowDataPtr(row, _dpth) &(fb->rowBits[row]->data[_dpth * fb->rowBits[row]->width])

/* We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
 * 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
 * Irrelevant for ESP32-S2 the way the FIFO ordering works is different - refer to page 679 of S2 technical reference manual
 */
#if defined(ESP32_THE_ORIG)
#define ESP32_TX_FIFO_POSITION_ADJUST(x_coord) (((x_coord)&1U) ? (x_coord - 1) : (x_coord + 1))
#else
#define ESP32_TX_FIFO_POSITION_ADJUST(x_coord) x_coord
#endif

/* This library is designed to take an 8 bit / 1 byte value (0-255) for each R G B colour sub-pixel.
 * The PIXEL_COLOR_DEPTH_BITS should always be '8' as a result.
 * However, if the library is to be used with lower colour depth (i.e. 6 bit colour), then we need to ensure the 8-bit value passed to the colour masking
 * is adjusted accordingly to ensure the LSB's are shifted left to MSB, by the difference. Otherwise the colours will be all screwed up.
 */
#define PIXEL_COLOR_MASK_BIT(color_depth_index, mask_offset) (1 << (color_depth_index + mask_offset))

bool MatrixPanel_I2S_DMA::setupDMA(const HUB75_I2S_CFG &_cfg)
{
  
  /***
   * Step 0: Allocate basic DMA framebuffer memory for the data we send out in parallel to the HUB75 panel.
   *         Colour depth is the only consideration.
   * 
   */
  ESP_LOGI("I2S-DMA", "Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  ESP_LOGI("I2S-DMA", "Free SPIRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  size_t allocated_fb_memory = 0;

  int fbs_required = (m_cfg.double_buff) ? 2 : 1;

  for (int fb = 0; fb < (fbs_required); fb++)
  {
    frame_buffer[fb].rowBits.reserve(ROWS_PER_FRAME);

    for (int malloc_num = 0; malloc_num < ROWS_PER_FRAME; malloc_num++)
    {
      auto ptr = std::make_shared<rowBitStruct>(PIXELS_PER_ROW, m_cfg.getPixelColorDepthBits());

      if (ptr->data == nullptr) {

        ESP_LOGE("I2S-DMA", "CRITICAL ERROR: Not enough memory for requested colour depth of %d bits! Please reduce pixel_color_depth_bits value.\r\n", m_cfg.getPixelColorDepthBits());

        return false;
        // TODO: should we release all previous rowBitStructs here???
      }

      allocated_fb_memory += ptr->getColorDepthSize(false); // byte required to display all colour depths for the two parallel rows
      frame_buffer[fb].rowBits.emplace_back(ptr); // save new rowBitStruct pointer into rows vector
      ++frame_buffer[fb].rows;
    }
  }
  ESP_LOGI("I2S-DMA", "Allocating %d bytes memory for DMA BCM framebuffer(s).", allocated_fb_memory);


  /***
   * Step 1: Check what the minimum refresh rate is, and calculate the lsbMsbTransitionBit
   *         which is the bit at which we can reduce the colour depth to achieve the minimum refresh rate.
   * 
   *         This also determines the number of DMA descriptors required per row.
   */  
   
//#define FORCE_COLOR_DEPTH 1   
  
#if !defined(FORCE_COLOR_DEPTH)

  ESP_LOGI("I2S-DMA", "Minimum visual refresh rate (scan rate from panel top to bottom) requested: %d Hz", m_cfg.min_refresh_rate);

  while (1)
  {
    int psPerClock = 1000000000000UL / m_cfg.i2sspeed;
    int nsPerLatch = ((PIXELS_PER_ROW + CLKS_DURING_LATCH) * psPerClock) / 1000; // time per row

    // add time to shift out LSBs + LSB-MSB transition bit - this ignores fractions...
    int nsPerRow = m_cfg.getPixelColorDepthBits() * nsPerLatch;

    // Now add the time for the remaining bit depths
    for (int i = lsbMsbTransitionBit + 1; i < m_cfg.getPixelColorDepthBits(); i++) {
      //nsPerRow += (1 << (i - lsbMsbTransitionBit - 1)) * (m_cfg.getPixelColorDepthBits() - i) * nsPerLatch;
	  nsPerRow += (1 << (i - lsbMsbTransitionBit - 1)) *  nsPerLatch;
	}

    int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
    int actualRefreshRate = 1000000000UL / (nsPerFrame);
    calculated_refresh_rate = actualRefreshRate;

    ESP_LOGW("I2S-DMA", "lsbMsbTransitionBit of %d gives %d Hz refresh rate.", lsbMsbTransitionBit, actualRefreshRate);

    if (actualRefreshRate > m_cfg.min_refresh_rate)
      break;

    if (lsbMsbTransitionBit < m_cfg.getPixelColorDepthBits() - 1)
      lsbMsbTransitionBit++;
    else
      break;
  }

  if (lsbMsbTransitionBit > 0)
  {
    ESP_LOGW("I2S-DMA", "lsbMsbTransitionBit of %d used to achieve refresh rate of %d Hz. Percieved colour depth to the eye may be reduced.", lsbMsbTransitionBit, m_cfg.min_refresh_rate);
  }

  ESP_LOGI("I2S-DMA", "DMA frame buffer color depths: %d", m_cfg.getPixelColorDepthBits());

#endif

  /***
   * Step 2:  Calculate the DMA descriptors required, which is used for memory allocation of the DMA linked list memory structure.
   *          This determines the number of passes required to shift out the colour bits in the framebuffer.
   *          We need to also take into consderation where a chain of panels (pixels) is so long, it requires more than one DMA payload,
   *          give this library's DMA output memory allocation approach is by the row.
   */
	
  int    dma_descs_per_row_1cdepth	 	= (frame_buffer[0].rowBits[0]->getColorDepthSize(true) + DMA_MAX - 1 ) / DMA_MAX;
  size_t last_dma_desc_bytes_1cdepth    = (frame_buffer[0].rowBits[0]->getColorDepthSize(true) % DMA_MAX);
  
  int    dma_descs_per_row_all_cdepths	  = (frame_buffer[0].rowBits[0]->getColorDepthSize(false) + DMA_MAX - 1 ) / DMA_MAX;
  size_t last_dma_desc_bytes_all_cdepths  = (frame_buffer[0].rowBits[0]->getColorDepthSize(false) % DMA_MAX);

  // Logging the calculated values
  ESP_LOGV("I2S-DMA", "dma_descs_per_row_1cdepth: %d", dma_descs_per_row_1cdepth);
  ESP_LOGV("I2S-DMA", "last_dma_desc_bytes_1cdepth: %zu", last_dma_desc_bytes_1cdepth);
  ESP_LOGV("I2S-DMA", "dma_descs_per_row_all_cdepths: %d", dma_descs_per_row_all_cdepths);
  ESP_LOGV("I2S-DMA", "last_dma_desc_bytes_all_cdepths: %zu", last_dma_desc_bytes_all_cdepths);
  
 
  // Calculate per-row number
  int dma_descriptors_per_row = dma_descs_per_row_all_cdepths;

  // Add descriptors for MSB bits after transition
  for (int i = lsbMsbTransitionBit + 1; i < m_cfg.getPixelColorDepthBits(); i++) {
    dma_descriptors_per_row += (1 << (i - lsbMsbTransitionBit - 1)) * dma_descs_per_row_1cdepth;
  }
  
  //dma_descriptors_per_row = 1;

  // Allocate DMA descriptors 
  int dma_descriptions_required = dma_descriptors_per_row * ROWS_PER_FRAME;
  
  ESP_LOGV("I2S-DMA", "DMA descriptors per row: %d", dma_descriptors_per_row);  
  ESP_LOGV("I2S-DMA", "DMA descriptors required per buffer: %d", dma_descriptions_required);    

  /***
   * Step 3:  Allocate the DMA descriptor memory via. the relevant platform DMA implementation class.
   */

  if (m_cfg.double_buff) {
    dma_bus.enable_double_dma_desc();
  }

  if ( !dma_bus.allocate_dma_desc_memory(dma_descriptions_required) )
  {
    return false;
  }


  /***
   * Step 4:  Link up the DMA descriptors per the colour depth and rows.
   */

  //fbs_required = 1; // (m_cfg.double_buff) ? 2 : 1;
  for (int fb = 0; fb < (fbs_required); fb++)
  {  
	
	int _dmadescriptor_count = 0; // for tracking
	
    for (int row = 0; row < ROWS_PER_FRAME; row++)
    {
	  //ESP_LOGV("I2S-DMA", ">>> Linking DMA descriptors for output row %d", row);    	
		
	  // Link and send all colour data, all passes of everything in one hit. 1 bit colour at least...
	  for (int dma_desc_all = 0; dma_desc_all < dma_descs_per_row_all_cdepths; dma_desc_all++) 
	  {
			size_t payload_bytes = (dma_desc_all == (dma_descs_per_row_all_cdepths-1)) ? last_dma_desc_bytes_all_cdepths:DMA_MAX;
			
			// Log the current descriptor number and the payload size being used.
			//ESP_LOGV("I2S-DMA", "Processing dma_desc_all: %d, payload_bytes: %zu, memory location: %p", dma_desc_all, payload_bytes, (frame_buffer[fb].rowBits[row]->getDataPtr(0)+(dma_desc_all*(DMA_MAX/sizeof(ESP32_I2S_DMA_STORAGE_TYPE)))));
				
		    dma_bus.create_dma_desc_link(frame_buffer[fb].rowBits[row]->getDataPtr(0)+(dma_desc_all*(DMA_MAX/sizeof(ESP32_I2S_DMA_STORAGE_TYPE))), payload_bytes, (fb==1));
			_dmadescriptor_count++;
			
			// Log the updated descriptor count after each operation.
			//ESP_LOGV("I2S-DMA", "Updated _dmadescriptor_count: %d", _dmadescriptor_count);			
	  }
	
      // Step 2: Handle additional descriptors for bits beyond the lsbMsbTransitionBit 
      for (int i = lsbMsbTransitionBit + 1; i < m_cfg.getPixelColorDepthBits(); i++) 
	  {
		  // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
		  // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
		  // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB

		  for (int k = 0; k < (1 << (i - lsbMsbTransitionBit - 1)); k++)
		  {		  
			  // Link and send all colour data, all passes of everything in one hit.
			  for (int dma_desc_1cdepth = 0; dma_desc_1cdepth < dma_descs_per_row_1cdepth; dma_desc_1cdepth++) 
			  {		  
				size_t payload_bytes = (dma_desc_1cdepth == (dma_descs_per_row_1cdepth-1)) ? last_dma_desc_bytes_1cdepth:DMA_MAX;
				
				// Log the current bit and the corresponding payload size.
				//ESP_LOGV("I2S-DMA", "Processing dma_desc_1cdepth: %d, payload_bytes: %zu, memory location: %p", dma_desc_1cdepth, payload_bytes, (frame_buffer[fb].rowBits[row]->getDataPtr(i)+(dma_desc_1cdepth*(DMA_MAX/sizeof(ESP32_I2S_DMA_STORAGE_TYPE)))));
		
				dma_bus.create_dma_desc_link(frame_buffer[fb].rowBits[row]->getDataPtr(i)+(dma_desc_1cdepth*(DMA_MAX/sizeof(ESP32_I2S_DMA_STORAGE_TYPE))), payload_bytes, (fb==1));
				_dmadescriptor_count++;
				
				// Log the updated descriptor count after each operation.
			//	ESP_LOGV("I2S-DMA", "Updated _dmadescriptor_count: %d", _dmadescriptor_count);		
			  }
		  } // end K
		
      } // end all other colour depth bits
	  

    } // end all rows
	
    ESP_LOGI("I2S-DMA", "Created %d DMA descriptors for buffer %d.", _dmadescriptor_count, fb);	
	
  } // end framebuffer loop
	  
	/*
	#include <iostream>
	#include <cmath> // For pow function (if needed, though bit shifts are better)

	int main() {

		int colorDepthBits = 8;
		int lsbMsbTransitionBit = 0; // Assuming lsbMsbTransitionBit is 0

		// Step 2: Handle additional descriptors for bits beyond the lsbMsbTransitionBit
		for (int i = lsbMsbTransitionBit + 1; i < colorDepthBits; i++) {
			// Calculate the number of passes required
			int passes = 1 << (i - lsbMsbTransitionBit - 1);

			std::cout << "Bit Level: " << i << ", Passes Required: " << passes << std::endl;

			// Simulate the inner loop for the number of passes
			for (int k = 0; k < passes; k++) {
				std::cout << "  Pass " << k + 1 << " for bit " << i << std::endl;
			}
		}

		return 0;
	} 
	*/
  


  /***
   * Step 5:  Set default framebuffer to fb[0]
   */  

  fb = &frame_buffer[0];
  

  //
  //    Setup DMA and Output to GPIO
  //
  auto bus_cfg = dma_bus.config(); // バス設定用の構造体を取得します。

  bus_cfg.bus_freq    = m_cfg.i2sspeed;
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
  
  initialized = true;

  return true;

} // end setupDMA

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
  if (!initialized)
    return;

  /* 1) Check that the co-ordinates are within range, or it'll break everything big time.
   * Valid co-ordinates are from 0 to (MATRIX_XXXX-1)
   */
  if (x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
  {
    return;
  }

  /* LED Brightness Compensation. Because if we do a basic "red & mask" for example,
   * we'll NEVER send the dimmest possible colour, due to binary skew.
   * i.e. It's almost impossible for colour_depth_idx of 0 to be sent out to the MATRIX unless the 'value' of a colour is exactly '1'
   * https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
   */
  uint16_t red16, green16, blue16;
#ifdef NO_CIE1931
  red16 	= red;
  green16 	= green;
  blue16 	= blue;
#else  
  red16 = lumConvTab[red];
  green16 = lumConvTab[green];
  blue16 = lumConvTab[blue];
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
  x_coord = ESP32_TX_FIFO_POSITION_ADJUST(x_coord);

  uint16_t _colourbitclear = BITMASK_RGB1_CLEAR, _colourbitoffset = 0;

  if (y_coord >= ROWS_PER_FRAME)
  { // if we are drawing to the bottom part of the panel
    _colourbitoffset = BITS_RGB2_OFFSET;
    _colourbitclear = BITMASK_RGB2_CLEAR;
    y_coord -= ROWS_PER_FRAME;
  }

  // Iterating through colour depth bits, which we assume are 8 bits per RGB subpixel (24bpp)
  uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
  do
  {
    --colour_depth_idx;

#ifdef NO_CIE1931
    uint16_t mask = colour_depth_idx;
#else	
    uint16_t mask = PIXEL_COLOR_MASK_BIT(colour_depth_idx, MASK_OFFSET);
#endif	
    uint16_t RGB_output_bits = 0;

    /* Per the .h file, the order of the output RGB bits is:
     * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1     */
    RGB_output_bits |= (bool)(blue16 & mask); // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green16 & mask); // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red16 & mask); // BGR
    RGB_output_bits <<= _colourbitoffset;    // shift colour bits to the required position

    // Get the contents at this address,
    // it would represent a vector pointing to the full row of pixels for the specified colour depth bit at Y coordinate
    ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, colour_depth_idx);

    // We need to update the correct uint16_t word in the rowBitStruct array pointing to a specific pixel at X - coordinate
    p[x_coord] &= _colourbitclear; // reset RGB bits
    p[x_coord] |= RGB_output_bits; // set new RGB bits

#if defined(SPIRAM_DMA_BUFFER)
    Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE));
#endif

  } while (colour_depth_idx); // end of colour depth loop (8)
} // updateMatrixDMABuffer (specific co-ords change)


/* Update the entire buffer with a single specific colour - quicker */
void MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  if (!initialized)
    return;

  /* https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/ */
  uint16_t red16, green16, blue16;
#ifdef NO_CIE1931
  red16 	= red;
  green16 	= green;
  blue16 	= blue;
#else  
  red16 = lumConvTab[red];
  green16 = lumConvTab[green];
  blue16 = lumConvTab[blue];
#endif

  for (uint8_t colour_depth_idx = 0; colour_depth_idx < m_cfg.getPixelColorDepthBits(); colour_depth_idx++) // colour depth - 8 iterations
  {
    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    uint16_t RGB_output_bits = 0;

#ifdef NO_CIE1931
    uint16_t mask = colour_depth_idx;
#else	
    uint16_t mask = PIXEL_COLOR_MASK_BIT(colour_depth_idx, MASK_OFFSET);
#endif	

    /* Per the .h file, the order of the output RGB bits is:
     * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1      */
    RGB_output_bits |= (bool)(blue16 & mask); // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green16 & mask); // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red16 & mask); // BGR

    // Duplicate and shift across so we have have 6 populated bits of RGB1 and RGB2 pin values suitable for DMA buffer
    RGB_output_bits |= RGB_output_bits << BITS_RGB2_OFFSET; // BGRBGR

    // Serial.printf("Fill with: 0x%#06x\n", RGB_output_bits);

    // iterate rows
    int matrix_frame_parallel_row = fb->rowBits.size();
    do
    {
      --matrix_frame_parallel_row;

      // The destination for the pixel row bitstream
      ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(matrix_frame_parallel_row, colour_depth_idx);

      // iterate pixels in a row
      int x_coord = fb->rowBits[matrix_frame_parallel_row]->width;
      do
      {
        --x_coord;
        p[x_coord] &= BITMASK_RGB12_CLEAR; // reset colour bits
        p[x_coord] |= RGB_output_bits;     // set new colour bits

#if defined(SPIRAM_DMA_BUFFER)
        Cache_WriteBack_Addr((uint32_t)&p[x_coord], sizeof(ESP32_I2S_DMA_STORAGE_TYPE));
#endif

      } while (x_coord);

    } while (matrix_frame_parallel_row); // end row iteration
  }                                      // colour depth loop (8)
} // updateMatrixDMABuffer (full frame paint)

/**
 * @brief - clears and reinitializes colour/control data in DMA buffs
 * When allocated, DMA buffs might be dirty, so we need to blank it and initialize ABCDE,LAT,OE control bits.
 * Those control bits are constants during the entire DMA sweep and never changed when updating just pixel colour data
 * so we could set it once on DMA buffs initialization and forget.
 * This effectively clears buffers to blank BLACK and makes it ready to display output.
 * (Brightness control via OE bit manipulation is another case) - this must be done as well seperately!
 */
void MatrixPanel_I2S_DMA::clearFrameBuffer(bool _buff_id)
{
  if (!initialized)
    return;

  frameStruct *fb = &frame_buffer[_buff_id];

  // we start with iterating all rows in dma_buff structure
  int row_idx = fb->rowBits.size();
  do
  {
    --row_idx;

    ESP32_I2S_DMA_STORAGE_TYPE *row = fb->rowBits[row_idx]->getDataPtr(0); // set pointer to the HEAD of a buffer holding data for the entire matrix row
    ESP32_I2S_DMA_STORAGE_TYPE abcde = (ESP32_I2S_DMA_STORAGE_TYPE)row_idx;

    // get last pixel index in a row of all colourdepths
    int x_pixel = fb->rowBits[row_idx]->width * fb->rowBits[row_idx]->colour_depth;

	abcde <<= BITS_ADDR_OFFSET; // shift row y-coord to match ABCDE bits in vector from 8 to 12
	do
	{
		--x_pixel;
		if (m_cfg.driver == HUB75_I2S_CFG::SM5266P)
		{
			// modifications here for row shift register type SM5266P
			// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164
			row[x_pixel] = abcde & (0x18 << BITS_ADDR_OFFSET); // mask out the bottom 3 bits which are the clk di bk inputs
		}
		else if (m_cfg.driver == HUB75_I2S_CFG::DP3246_SM5368) 
		{
			row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = 0x0000;
		}
		else
		{
			row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = abcde;
		}

	} while (x_pixel != fb->rowBits[row_idx]->width); // spare the first "width's" worth of pixels as they are the LSB pixels/colordepth

	// The colour_index[0] (LSB) x_pixels must be "marked" with a previous's row address, because it is used to display
	// previous row while we pump in MSBs's for the next row.
	if (row_idx == 0) { 
		abcde = ROWS_PER_FRAME-1; // wrap around
	} else {
		abcde = row_idx-1;	
	}

    abcde <<= BITS_ADDR_OFFSET; // shift row y-coord to match ABCDE bits in vector from 8 to 12		
	do
    {
      --x_pixel;

      if (m_cfg.driver == HUB75_I2S_CFG::SM5266P)
      {
        // modifications here for row shift register type SM5266P
        // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164
        row[x_pixel] = abcde & (0x18 << BITS_ADDR_OFFSET); // mask out the bottom 3 bits which are the clk di bk inputs
      }
      else if (m_cfg.driver == HUB75_I2S_CFG::DP3246_SM5368) 
      {
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = 0x0000;
      }
      else
      {
        row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel)] = abcde;
      }

    } while (x_pixel);

    // modifications here for row shift register type SM5266P
    // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/164
    if (m_cfg.driver == HUB75_I2S_CFG::SM5266P)
    {
      uint16_t serialCount;
      uint16_t latch;
      x_pixel = fb->rowBits[row_idx]->width - 16; // come back 8*2 pixels to allow for 8 writes
      serialCount = 8;
      do
      {
        serialCount--;
        latch = row[x_pixel] | (((((ESP32_I2S_DMA_STORAGE_TYPE)row_idx) % 8) == serialCount) << 1) << BITS_ADDR_OFFSET; // data on 'B'
        row[x_pixel++] = latch | (0x05 << BITS_ADDR_OFFSET);                                                            // clock high on 'A'and BK high for update
        row[x_pixel++] = latch | (0x04 << BITS_ADDR_OFFSET);                                                            // clock low on 'A'and BK high for update
      } while (serialCount);
    } // end SM5266P

    // row selection for SM5368 shift regs with ABC-only addressing. A is row clk, B is BK and C is row data
    if (m_cfg.driver == HUB75_I2S_CFG::DP3246_SM5368) 
    {
      x_pixel = fb->rowBits[row_idx]->width - 1;                                                                        // last pixel in first block)
      uint16_t c = (row_idx == 0) ? BIT_C : 0x0000;                                                                     // set row data (C) when row==0, then push through shift regs for all other rows
      row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel - 1)] |= c;                                                             // set row data
      row[ESP32_TX_FIFO_POSITION_ADJUST(x_pixel + 0)] |= c | BIT_A | BIT_B;                                             // set row clk and bk, carry row data
    } // end DP3246_SM5368

    // let's set LAT/OE control bits for specific pixels in each colour_index subrows
    // Need to consider the original ESP32's (WROOM) DMA TX FIFO reordering of bytes...
    uint8_t colouridx = fb->rowBits[row_idx]->colour_depth;
    do
    {
      --colouridx;

      // switch pointer to a row for a specific colour index
      row = fb->rowBits[row_idx]->getDataPtr(colouridx);

      // DP3246 needs the latch high for 3 clock cycles, so start 2 cycles earlier
      if (m_cfg.driver == HUB75_I2S_CFG::DP3246_SM5368) 
      {
        row[ESP32_TX_FIFO_POSITION_ADJUST(fb->rowBits[row_idx]->width - 3)] |= BIT_LAT;   // DP3246 needs 3 clock cycle latch 
        row[ESP32_TX_FIFO_POSITION_ADJUST(fb->rowBits[row_idx]->width - 2)] |= BIT_LAT;   // DP3246 needs 3 clock cycle latch 
      } // DP3246_SM5368
      
      row[ESP32_TX_FIFO_POSITION_ADJUST(fb->rowBits[row_idx]->width - 1)] |= BIT_LAT; // -1 pixel to compensate array index starting at 0

      // ESP32_TX_FIFO_POSITION_ADJUST(dma_buff.rowBits[row_idx]->width - 1)

      // need to disable OE before/after latch to hide row transition
      // Should be one clock or more before latch, otherwise can get ghosting
      uint8_t _blank = m_cfg.latch_blanking;
      do
      {
        --_blank;

        row[ESP32_TX_FIFO_POSITION_ADJUST(0 + _blank)] |= BIT_OE;                               // disable output
        row[ESP32_TX_FIFO_POSITION_ADJUST(fb->rowBits[row_idx]->width - 1)] |= BIT_OE;          // disable output
        row[ESP32_TX_FIFO_POSITION_ADJUST(fb->rowBits[row_idx]->width - _blank - 1)] |= BIT_OE; // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0

      } while (_blank);

    } while (colouridx);

#if defined(SPIRAM_DMA_BUFFER)
    Cache_WriteBack_Addr((uint32_t)row, fb->rowBits[row_idx]->getColorDepthSize(false));
#endif

  } while (row_idx);
}

void MatrixPanel_I2S_DMA::setBrightnessOE(uint8_t brt, const int _buff_id)
{

  if (!initialized)
    return;

  frameStruct *fb = &frame_buffer[_buff_id];

  uint8_t _blank = m_cfg.latch_blanking; // don't want to inadvertantly blast over this
  uint8_t _depth = fb->rowBits[0]->colour_depth;
  uint16_t _width = fb->rowBits[0]->width;

  // start with iterating all rows in dma_buff structure
  int row_idx = fb->rowBits.size();
  do
  {
    --row_idx;

    // let's set OE control bits for specific pixels in each color_index subrows
    uint8_t colouridx = _depth;
    do
    {
      --colouridx;

      char bitplane = (2 * _depth - colouridx) % _depth;
      char bitshift = (_depth - lsbMsbTransitionBit - 1) >> 1;

      char rightshift = std::max(bitplane - bitshift - 2, 0);
      // calculate the OE disable period by brightness, and also blanking
      int brightness_in_x_pixels = ((_width - _blank) * brt) >> (7 + rightshift);
      brightness_in_x_pixels = (brightness_in_x_pixels >> 1) | (brightness_in_x_pixels & 1);

      // switch pointer to a row for a specific color index
      ESP32_I2S_DMA_STORAGE_TYPE *row = fb->rowBits[row_idx]->getDataPtr(colouridx);

      // define range of Output Enable on the center of the row
      int x_coord_max = (_width + brightness_in_x_pixels + 1) >> 1;
      int x_coord_min = (_width - brightness_in_x_pixels + 0) >> 1;
      int x_coord = _width;
      do
      {
        --x_coord;

        // (the check is already including "blanking" )
        if (x_coord >= x_coord_min && x_coord < x_coord_max)
        {
          row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] &= BITMASK_OE_CLEAR;
        }
        else
        {
          row[ESP32_TX_FIFO_POSITION_ADJUST(x_coord)] |= BIT_OE; // Disable output after this point.
        }

      } while (x_coord);

    } while (colouridx);

#if defined(SPIRAM_DMA_BUFFER)
	// Force the flush and update of the PSRAM for the memory address range of the 'row data' as
	// data changes probably aren't being sent out via DMA as they're sitting in a hadrware 'cache' 
    ESP32_I2S_DMA_STORAGE_TYPE *row_ptr = fb->rowBits[row_idx]->getDataPtr(0);
    Cache_WriteBack_Addr((uint32_t)row_ptr, fb->rowBits[row_idx]->getColorDepthSize(false));
#endif
  } while (row_idx);
}


/*
 *  overload for compatibility
 */

bool MatrixPanel_I2S_DMA::begin(int r1, int g1, int b1, int r2, int g2, int b2, int a, int b, int c, int d, int e, int lat, int oe, int clk)
{
  if (initialized)
    return true;
  // RGB
  m_cfg.gpio.r1 = r1;
  m_cfg.gpio.g1 = g1;
  m_cfg.gpio.b1 = b1;
  m_cfg.gpio.r2 = r2;
  m_cfg.gpio.g2 = g2;
  m_cfg.gpio.b2 = b2;

  // Line Select
  m_cfg.gpio.a = a;
  m_cfg.gpio.b = b;
  m_cfg.gpio.c = c;
  m_cfg.gpio.d = d;
  m_cfg.gpio.e = e;

  // Clock & Control
  m_cfg.gpio.lat = lat;
  m_cfg.gpio.oe = oe;
  m_cfg.gpio.clk = clk;

  return begin();
}

bool MatrixPanel_I2S_DMA::begin(const HUB75_I2S_CFG &cfg)
{
  if (initialized)
    return true;

  if (!setCfg(cfg))
    return false;

  return begin();
}

/**
 * @brief - Sets how many clock cycles to blank OE before/after LAT signal change
 * @param uint8_t pulses - clocks before/after OE
 * default is DEFAULT_LAT_BLANKING
 * Max is MAX_LAT_BLANKING
 * @returns - new value for m_cfg.latch_blanking
 */
uint8_t MatrixPanel_I2S_DMA::setLatBlanking(uint8_t pulses)
{
  if (pulses > MAX_LAT_BLANKING)
    pulses = MAX_LAT_BLANKING;

  if (!pulses)
    pulses = DEFAULT_LAT_BLANKING;

  m_cfg.latch_blanking = pulses;

  // remove brightness var for now.
  // setPanelBrightness(brightness);    // set brightness to reset OE bits to the values matching new LAT blanking setting
  return m_cfg.latch_blanking;
}

#ifndef NO_FAST_FUNCTIONS
/**
 * @brief - update DMA buff drawing horizontal line at specified coordinates
 * @param x_coord - line start coordinate x
 * @param y_coord - line start coordinate y
 * @param l - line length
 * @param r,g,b, - RGB888 colour
 */
void MatrixPanel_I2S_DMA::hlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue)
{
  if (!initialized)
    return;

  if ((x_coord + l) < 1 || y_coord < 0 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
    return;

  l = x_coord < 0 ? l + x_coord : l;
  x_coord = x_coord < 0 ? 0 : x_coord;

  l = ((x_coord + l) >= PIXELS_PER_ROW) ? (PIXELS_PER_ROW - x_coord) : l;

  // if (x_coord+l > PIXELS_PER_ROW)
  //    l = PIXELS_PER_ROW - x_coord + 1;     // reset width to end of row

  /* LED Brightness Compensation */
  uint16_t red16, green16, blue16;
#ifndef NO_CIE1931
  red16 = lumConvTab[red];
  green16 = lumConvTab[green];
  blue16 = lumConvTab[blue];
#else
  red16 = red << MASK_OFFSET;
  green16 = green << MASK_OFFSET;
  blue16 = blue << MASK_OFFSET;
#endif

  uint16_t _colourbitclear = BITMASK_RGB1_CLEAR, _colourbitoffset = 0;

  if (y_coord >= ROWS_PER_FRAME)
  { // if we are drawing to the bottom part of the panel
    _colourbitoffset = BITS_RGB2_OFFSET;
    _colourbitclear = BITMASK_RGB2_CLEAR;
    y_coord -= ROWS_PER_FRAME;
  }

  // Iterating through colour depth bits (8 iterations)
  uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
  do
  {
    --colour_depth_idx;

    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    uint16_t RGB_output_bits = 0;
    //    uint8_t mask = (1 << colour_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST);
    // #if PIXEL_COLOR_DEPTH_BITS < 8
    //     uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit colour (8 bits per RGB subpixel)
    // #else
    //     uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit colour (8 bits per RGB subpixel)
    // #endif
    uint16_t mask = PIXEL_COLOR_MASK_BIT(colour_depth_idx, MASK_OFFSET);

    /* Per the .h file, the order of the output RGB bits is:
     * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1     */
    RGB_output_bits |= (bool)(blue16 & mask); // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green16 & mask); // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red16 & mask); // BGR
    RGB_output_bits <<= _colourbitoffset;    // shift color bits to the required position

    // Get the contents at this address,
    // it would represent a vector pointing to the full row of pixels for the specified colour depth bit at Y coordinate
    ESP32_I2S_DMA_STORAGE_TYPE *p = fb->rowBits[y_coord]->getDataPtr(colour_depth_idx);
    // inlined version works slower here, dunno why :(
    // ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, colour_depth_idx, back_buffer_id);

    int16_t _l = l;
    do
    { // iterate pixels in a row
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

      v &= _colourbitclear;   // reset colour bits
      v |= RGB_output_bits;   // set new colour bits
    } while (_l);             // iterate pixels in a row
  } while (colour_depth_idx); // end of colour depth loop (8)
} // hlineDMA()

/**
 * @brief - update DMA buff drawing vertical line at specified coordinates
 * @param x_coord - line start coordinate x
 * @param y_coord - line start coordinate y
 * @param l - line length
 * @param r,g,b, - RGB888 colour
 */
void MatrixPanel_I2S_DMA::vlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue)
{
  if (!initialized)
    return;

  if (x_coord < 0 || (y_coord + l) < 1 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
    return;

  l = y_coord < 0 ? l + y_coord : l;
  y_coord = y_coord < 0 ? 0 : y_coord;

  // check for a length that goes beyond the height of the screen! Array out of bounds dma memory changes = screwed output #163
  l = ((y_coord + l) >= m_cfg.mx_height) ? (m_cfg.mx_height - y_coord) : l;
  // if (y_coord + l > m_cfg.mx_height)
  ///    l = m_cfg.mx_height - y_coord + 1;     // reset width to end of col

  /* LED Brightness Compensation */
  uint16_t red16, green16, blue16;
#ifndef NO_CIE1931
  red16 = lumConvTab[red];
  green16 = lumConvTab[green];
  blue16 = lumConvTab[blue];
#else
  red16 = red << MASK_OFFSET;
  green16 = green << MASK_OFFSET;
  blue16 = blue << MASK_OFFSET;
#endif

  /*
  #if defined(ESP32_THE_ORIG)
    // Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
    x_coord & 1U ? --x_coord : ++x_coord;
  #endif
  */
  x_coord = ESP32_TX_FIFO_POSITION_ADJUST(x_coord);

  uint8_t colour_depth_idx = m_cfg.getPixelColorDepthBits();
  do
  { // Iterating through colour depth bits (8 iterations)
    --colour_depth_idx;

    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    //    uint8_t mask = (1 << colour_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST);
    // #if PIXEL_COLOR_DEPTH_BITS < 8
    //     uint8_t mask = (1 << (colour_depth_idx+MASK_OFFSET)); // expect 24 bit colour (8 bits per RGB subpixel)
    // #else
    //     uint8_t mask = (1 << (colour_depth_idx)); // expect 24 bit colour (8 bits per RGB subpixel)
    // #endif

    uint16_t mask = PIXEL_COLOR_MASK_BIT(colour_depth_idx, MASK_OFFSET);
    uint16_t RGB_output_bits = 0;

    /* Per the .h file, the order of the output RGB bits is:
     * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1   */
    RGB_output_bits |= (bool)(blue16 & mask); // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green16 & mask); // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red16 & mask); // BGR

    int16_t _l = 0, _y = y_coord;
    uint16_t _colourbitclear = BITMASK_RGB1_CLEAR;
    do
    { // iterate pixels in a column

      if (_y >= ROWS_PER_FRAME)
      { // if y-coord overlapped bottom-half panel
        _y -= ROWS_PER_FRAME;
        _colourbitclear = BITMASK_RGB2_CLEAR;
        RGB_output_bits <<= BITS_RGB2_OFFSET;
      }

      // Get the contents at this address,
      // it would represent a vector pointing to the full row of pixels for the specified colour depth bit at Y coordinate
      // ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(_y, colour_depth_idx, back_buffer_id);
      ESP32_I2S_DMA_STORAGE_TYPE *p = fb->rowBits[_y]->getDataPtr(colour_depth_idx);

      p[x_coord] &= _colourbitclear; // reset RGB bits
      p[x_coord] |= RGB_output_bits; // set new RGB bits
      ++_y;
    } while (++_l != l);      // iterate pixels in a col
  } while (colour_depth_idx); // end of colour depth loop (8)
} // vlineDMA()

/**
 * @brief - update DMA buff drawing a rectangular at specified coordinates
 * this works much faster than multiple consecutive per-pixel calls to updateMatrixDMABuffer()
 * @param int16_t x, int16_t y - coordinates of a top-left corner
 * @param int16_t w, int16_t h - width and height of a rectangular, min is 1 px
 * @param uint8_t r - RGB888 colour
 * @param uint8_t g - RGB888 colour
 * @param uint8_t b - RGB888 colour
 */
void MatrixPanel_I2S_DMA::fillRectDMA(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b)
{

  // h-lines are >2 times faster than v-lines
  // so will use it only for tall rects with h >2w
  if (h > 2 * w)
  {
    // draw using v-lines
    do
    {
      --w;
      vlineDMA(x + w, y, h, r, g, b);
    } while (w);
  }
  else
  {
    // draw using h-lines
    do
    {
      --h;
      hlineDMA(x, y + h, w, r, g, b);
    } while (h);
  }
}

#endif // NO_FAST_FUNCTIONS
