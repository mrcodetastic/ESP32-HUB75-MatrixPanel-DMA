#include <Arduino.h>
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
//#include "xtensa/core-macros.h"

// Credits: Louis Beaudoin <https://github.com/pixelmatix/SmartMatrix/tree/teensylc>
// and Sprite_TM: 			https://www.esp32.com/viewtopic.php?f=17&t=3188 and https://www.esp32.com/viewtopic.php?f=13&t=3256

/*

    This is example code to driver a p3(2121)64*32 -style RGB LED display. These types of displays do not have memory and need to be refreshed
    continuously. The display has 2 RGB inputs, 4 inputs to select the active line, a pixel clock input, a latch enable input and an output-enable
    input. The display can be seen as 2 64x16 displays consisting of the upper half and the lower half of the display. Each half has a separate 
    RGB pixel input, the rest of the inputs are shared.

    Each display half can only show one line of RGB pixels at a time: to do this, the RGB data for the line is input by setting the RGB input pins
    to the desired value for the first pixel, giving the display a clock pulse, setting the RGB input pins to the desired value for the second pixel,
    giving a clock pulse, etc. Do this 64 times to clock in an entire row. The pixels will not be displayed yet: until the latch input is made high, 
    the display will still send out the previously clocked in line. Pulsing the latch input high will replace the displayed data with the data just 
    clocked in.

    The 4 line select inputs select where the currently active line is displayed: when provided with a binary number (0-15), the latched pixel data
    will immediately appear on this line. Note: While clocking in data for a line, the *previous* line is still displayed, and these lines should
    be set to the value to reflect the position the *previous* line is supposed to be on.

    Finally, the screen has an OE input, which is used to disable the LEDs when latching new data and changing the state of the line select inputs:
    doing so hides any artefacts that appear at this time. The OE line is also used to dim the display by only turning it on for a limited time every
    line.

    All in all, an image can be displayed by 'scanning' the display, say, 100 times per second. The slowness of the human eye hides the fact that
    only one line is showed at a time, and the display looks like every pixel is driven at the same time.

    Now, the RGB inputs for these types of displays are digital, meaning each red, green and blue subpixel can only be on or off. This leads to a
    color palette of 8 pixels, not enough to display nice pictures. To get around this, we use binary code modulation.

    Binary code modulation is somewhat like PWM, but easier to implement in our case. First, we define the time we would refresh the display without
    binary code modulation as the 'frame time'. For, say, a four-bit binary code modulation, the frame time is divided into 15 ticks of equal length.

    We also define 4 subframes (0 to 3), defining which LEDs are on and which LEDs are off during that subframe. (Subframes are the same as a 
    normal frame in non-binary-coded-modulation mode, but are showed faster.)  From our (non-monochrome) input image, we take the (8-bit: bit 7 
    to bit 0) RGB pixel values. If the pixel values have bit 7 set, we turn the corresponding LED on in subframe 3. If they have bit 6 set,
    we turn on the corresponding LED in subframe 2, if bit 5 is set subframe 1, if bit 4 is set in subframe 0.

    Now, in order to (on average within a frame) turn a LED on for the time specified in the pixel value in the input data, we need to weigh the
    subframes. We have 15 pixels: if we show subframe 3 for 8 of them, subframe 2 for 4 of them, subframe 1 for 2 of them and subframe 1 for 1 of
    them, this 'automatically' happens. (We also distribute the subframes evenly over the ticks, which reduces flicker.)

    In this code, we use the I2S peripheral in parallel mode to achieve this. Essentially, first we allocate memory for all subframes. This memory
    contains a sequence of all the signals (2xRGB, line select, latch enable, output enable) that need to be sent to the display for that subframe.
    Then we ask the I2S-parallel driver to set up a DMA chain so the subframes are sent out in a sequence that satisfies the requirement that
    subframe x has to be sent out for (2^x) ticks. Finally, we fill the subframes with image data.

    We use a front buffer/back buffer technique here to make sure the display is refreshed in one go and drawing artifacts do not reach the display.
    In practice, for small displays this is not really necessarily.
    
*/


// macro's to calculate sizes of a single buffer (double buffer takes twice as this)
#define rowBitStructBuffSize        sizeof(ESP32_I2S_DMA_STORAGE_TYPE) * (PIXELS_PER_ROW + CLKS_DURING_LATCH) * PIXEL_COLOR_DEPTH_BITS
#define frameStructBuffSize         ROWS_PER_FRAME * rowBitStructBuffSize

/* this replicates same function in rowBitStruct, but due to induced inlining it might be MUCH faster when used in tight loops
 * while method from struct could be flushed out of instruction cache between loop cycles
 * do NOT forget about buff_id param if using this
 */
#define getRowDataPtr(row, _dpth, buff_id) &(dma_buff.rowBits[row]->data[_dpth * dma_buff.rowBits[row]->width + buff_id*(dma_buff.rowBits[row]->width * dma_buff.rowBits[row]->color_depth)])

bool MatrixPanel_I2S_DMA::allocateDMAmemory()
{

   /***
    * Step 1: Look at the overall DMA capable memory for the DMA FRAMEBUFFER data only (not the DMA linked list descriptors yet) 
    *         and do some pre-checks.
    */

    int    _num_frame_buffers                   = (m_cfg.double_buff) ? 2:1;
    size_t _frame_buffer_memory_required        = frameStructBuffSize * _num_frame_buffers; 
    size_t _dma_linked_list_memory_required     = 0; 
    size_t _total_dma_capable_memory_reserved   = 0;   
    
	// 1. Calculate the amount of DMA capable memory that's actually available
    #if SERIAL_DEBUG    
        Serial.printf_P(PSTR("Panel Width: %d pixels.\r\n"),  PIXELS_PER_ROW);
        Serial.printf_P(PSTR("Panel Height: %d pixels.\r\n"), m_cfg.mx_height);

        if (m_cfg.double_buff) {
          Serial.println(F("DOUBLE FRAME BUFFERS / DOUBLE BUFFERING IS ENABLED. DOUBLE THE RAM REQUIRED!"));
        }
        
        Serial.println(F("DMA memory blocks available before any malloc's: "));
        heap_caps_print_heap_info(MALLOC_CAP_DMA);
		Serial.println(F("******************************************************************"));
        Serial.printf_P(PSTR("We're going to need %d bytes of SRAM just for the frame buffer(s).\r\n"), _frame_buffer_memory_required);  
        Serial.printf_P(PSTR("The total amount of DMA capable SRAM memory is %d bytes.\r\n"), heap_caps_get_free_size(MALLOC_CAP_DMA));
        Serial.printf_P(PSTR("Largest DMA capable SRAM memory block is %d bytes.\r\n"), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));          
		Serial.println(F("******************************************************************"));	    
		
    #endif

    // Can we potentially fit the framebuffer into the DMA capable memory that's available?
    if ( heap_caps_get_free_size(MALLOC_CAP_DMA) < _frame_buffer_memory_required  ) {
      
      #if SERIAL_DEBUG      
        Serial.printf_P(PSTR("######### Insufficient memory for requested resolution. Reduce MATRIX_COLOR_DEPTH and try again.\r\n\tAdditional %d bytes of memory required.\r\n\r\n"), (_frame_buffer_memory_required-heap_caps_get_free_size(MALLOC_CAP_DMA)) );
      #endif

      return false;
    }
	
	// Alright, theoretically we should be OK, so let us do this, so
	// lets allocate a chunk of memory for each row (a row could span multiple panels if chaining is in place)
  dma_buff.rowBits.reserve(ROWS_PER_FRAME);

  // iterate through number of rows
	for (int malloc_num =0; malloc_num < ROWS_PER_FRAME; ++malloc_num)
	{
    auto ptr = std::make_shared<rowBitStruct>(PIXELS_PER_ROW, PIXEL_COLOR_DEPTH_BITS, m_cfg.double_buff);

    if (ptr->data == nullptr){
      #if SERIAL_DEBUG
		      Serial.printf_P(PSTR("ERROR: Couldn't malloc rowBitStruct %d! Critical fail.\r\n"), malloc_num);
      #endif
    			return false;
          // TODO: should we release all previous rowBitStructs here???
    }

    dma_buff.rowBits.emplace_back(ptr);     // save new rowBitStruct into rows vector
    ++dma_buff.rows;
    #if SERIAL_DEBUG
        Serial.printf_P(PSTR("Malloc'ing %d bytes of memory @ address %ud for frame row %d.\r\n"), ptr->size()*_num_frame_buffers, (unsigned int)ptr->getDataPtr(), malloc_num);
     #endif

	}

    _total_dma_capable_memory_reserved += _frame_buffer_memory_required;    


  /***
   * Step 2: Calculate the amount of memory required for the DMA engine's linked list descriptors.
   *         Credit to SmartMatrix for this stuff.
   */    
   

    // Calculate what colour depth is actually possible based on memory available vs. required DMA linked-list descriptors.
    // aka. Calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory
    int numDMAdescriptorsPerRow = 0;
    lsbMsbTransitionBit = 0;
	

    while(1) {
        numDMAdescriptorsPerRow = 1;
        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++) {
            numDMAdescriptorsPerRow += (1<<(i - lsbMsbTransitionBit - 1));
        }

        size_t ramrequired = numDMAdescriptorsPerRow * ROWS_PER_FRAME * _num_frame_buffers * sizeof(lldesc_t);
        size_t largestblockfree = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
        #if SERIAL_DEBUG  
          Serial.printf_P(PSTR("lsbMsbTransitionBit of %d with %d DMA descriptors per frame row, requires %d bytes RAM, %d available, leaving %d free: \r\n"), lsbMsbTransitionBit, numDMAdescriptorsPerRow, ramrequired, largestblockfree, largestblockfree - ramrequired);
        #endif

        if(ramrequired < largestblockfree)
            break;
            
        if(lsbMsbTransitionBit < PIXEL_COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }
    
    #if SERIAL_DEBUG  
      Serial.printf_P(PSTR("Raised lsbMsbTransitionBit to %d/%d to fit in remaining RAM\r\n"), lsbMsbTransitionBit, PIXEL_COLOR_DEPTH_BITS - 1);
    #endif


   #ifndef IGNORE_REFRESH_RATE	
    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory that will meet or exceed the configured refresh rate
    while(1) {           
        int psPerClock = 1000000000000UL/m_cfg.i2sspeed;
        int nsPerLatch = ((PIXELS_PER_ROW + CLKS_DURING_LATCH) * psPerClock) / 1000;

        // add time to shift out LSBs + LSB-MSB transition bit - this ignores fractions...
        int nsPerRow = PIXEL_COLOR_DEPTH_BITS * nsPerLatch;

        // add time to shift out MSBs
        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++)
            nsPerRow += (1<<(i - lsbMsbTransitionBit - 1)) * (PIXEL_COLOR_DEPTH_BITS - i) * nsPerLatch;

        int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
        int actualRefreshRate = 1000000000UL/(nsPerFrame);
        calculated_refresh_rate = actualRefreshRate;

        #if SERIAL_DEBUG  
          Serial.printf_P(PSTR("lsbMsbTransitionBit of %d gives %d Hz refresh: \r\n"), lsbMsbTransitionBit, actualRefreshRate);        
		    #endif

        if (actualRefreshRate > m_cfg.min_refresh_rate) 
          break;
                  
        if(lsbMsbTransitionBit < PIXEL_COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    #if SERIAL_DEBUG  
    Serial.printf_P(PSTR("Raised lsbMsbTransitionBit to %d/%d to meet minimum refresh rate\r\n"), lsbMsbTransitionBit, PIXEL_COLOR_DEPTH_BITS - 1);
    #endif

	#endif

  /***
   * Step 2a: lsbMsbTransition bit is now finalised - recalculate the DMA descriptor count required, which is used for
   *          memory allocation of the DMA linked list memory structure.
   */          
    numDMAdescriptorsPerRow = 1;
    for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++) {
        numDMAdescriptorsPerRow += (1<<(i - lsbMsbTransitionBit - 1));
    }
    #if SERIAL_DEBUG
      Serial.printf_P(PSTR("Recalculated number of DMA descriptors per row: %d\n"), numDMAdescriptorsPerRow);
    #endif

    // Refer to 'DMA_LL_PAYLOAD_SPLIT' code in configureDMA() below to understand why this exists.
    // numDMAdescriptorsPerRow is also used to calculate descount which is super important in i2s_parallel_config_t SoC DMA setup. 
    if ( rowBitStructBuffSize > DMA_MAX ) {

        #if SERIAL_DEBUG  
          Serial.printf_P(PSTR("rowColorDepthStruct struct is too large, split DMA payload required. Adding %d DMA descriptors\n"), PIXEL_COLOR_DEPTH_BITS-1);
		    #endif

        numDMAdescriptorsPerRow += PIXEL_COLOR_DEPTH_BITS-1; 
        // Note: If numDMAdescriptorsPerRow is even just one descriptor too large, DMA linked list will not correctly loop.
    }


  /***
   * Step 3: Allocate memory for DMA linked list, linking up each framebuffer row in sequence for GPIO output.
   */        

    _dma_linked_list_memory_required = numDMAdescriptorsPerRow * ROWS_PER_FRAME * _num_frame_buffers * sizeof(lldesc_t);
  #if SERIAL_DEBUG
		Serial.printf_P(PSTR("Descriptors for lsbMsbTransitionBit of %d/%d with %d frame rows require %d bytes of DMA RAM with %d numDMAdescriptorsPerRow.\r\n"), lsbMsbTransitionBit, PIXEL_COLOR_DEPTH_BITS - 1, ROWS_PER_FRAME, _dma_linked_list_memory_required, numDMAdescriptorsPerRow);    
	#endif   

    _total_dma_capable_memory_reserved += _dma_linked_list_memory_required;

    // Do a final check to see if we have enough space for the additional DMA linked list descriptors that will be required to link it all up!
    if(_dma_linked_list_memory_required > heap_caps_get_largest_free_block(MALLOC_CAP_DMA)) {
       Serial.println(F("ERROR: Not enough SRAM left over for DMA linked-list descriptor memory reservation! Oh so close!\r\n"));
  
        return false;
    } // linked list descriptors memory check

    // malloc the DMA linked list descriptors that i2s_parallel will need
    desccount = numDMAdescriptorsPerRow * ROWS_PER_FRAME;

    //lldesc_t * dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Can't allocate descriptor framebuffer a");
    if(!dmadesc_a) {
        Serial.println(F("ERROR: Could not malloc descriptor framebuffer a."));
        return false;
    }
	
    if (m_cfg.double_buff) // reserve space for second framebuffer linked list
    {
        //lldesc_t * dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
        dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
        assert("Could not malloc descriptor framebuffer b.");
        if(!dmadesc_b) {
            Serial.println(F("ERROR: Could not malloc descriptor framebuffer b."));
            return false;
        }
    }

    Serial.println(F("*** ESP32-HUB75-MatrixPanel-I2S-DMA: Memory Allocations Complete ***"));
    Serial.printf_P(PSTR("Total memory that was reserved: %d kB.\r\n"), _total_dma_capable_memory_reserved/1024);
    Serial.printf_P(PSTR("... of which was used for the DMA Linked List(s): %d kB.\r\n"), _dma_linked_list_memory_required/1024);
	
    Serial.printf_P(PSTR("Heap Memory Available: %d bytes total. Largest free block: %d bytes.\r\n"), heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    Serial.printf_P(PSTR("General RAM Available: %d bytes total. Largest free block: %d bytes.\r\n"), heap_caps_get_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));


    // Just os we know
  	initialized = true;

    return true;

} // end allocateDMAmemory()



void MatrixPanel_I2S_DMA::configureDMA(const HUB75_I2S_CFG& _cfg)
{
    #if SERIAL_DEBUG  
      Serial.println(F("configureDMA(): Starting configuration of DMA engine.\r\n"));
    #endif   


    lldesc_t *previous_dmadesc_a     = 0;
    lldesc_t *previous_dmadesc_b     = 0;
    int current_dmadescriptor_offset = 0;

    // HACK: If we need to split the payload in 1/2 so that it doesn't breach DMA_MAX, lets do it by the color_depth.
    int num_dma_payload_color_depths = PIXEL_COLOR_DEPTH_BITS;
    if ( rowBitStructBuffSize > DMA_MAX ) {
        num_dma_payload_color_depths = 1;
    }

    // Fill DMA linked lists for both frames (as in, halves of the HUB75 panel) and if double buffering is enabled, link it up for both buffers.
    for(int row = 0; row < ROWS_PER_FRAME; row++) {

        #if SERIAL_DEBUG          
          Serial.printf_P(PSTR( "Row %d DMA payload of %d bytes. DMA_MAX is %d.\n"), row, dma_buff.rowBits[row]->size(), DMA_MAX);
        #endif

        
        // first set of data is LSB through MSB, single pass (IF TOTAL SIZE < DMA_MAX) - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
        // NOTE: size must be less than DMA_MAX - worst case for library: 16-bpp with 256 pixels per row would exceed this, need to break into two
        link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, dma_buff.rowBits[row]->getDataPtr(), dma_buff.rowBits[row]->size(num_dma_payload_color_depths));
          previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

        if (m_cfg.double_buff) {
          link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, dma_buff.rowBits[row]->getDataPtr(0, 1), dma_buff.rowBits[row]->size(num_dma_payload_color_depths));
          previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset]; }

        current_dmadescriptor_offset++;

        // If the number of pixels per row is too great for the size of a DMA payload, so we need to split what we were going to send above.
        if ( rowBitStructBuffSize > DMA_MAX )
        {
          #if SERIAL_DEBUG     
              Serial.printf_P(PSTR("Spliting DMA payload for %d color depths into %d byte payloads.\r\n"), PIXEL_COLOR_DEPTH_BITS-1, rowBitStructBuffSize/PIXEL_COLOR_DEPTH_BITS );
          #endif
          
          for (int cd = 1; cd < PIXEL_COLOR_DEPTH_BITS; cd++) 
          {
            // first set of data is LSB through MSB, single pass - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
            // TODO: size must be less than DMA_MAX - worst case for library: 16-bpp with 256 pixels per row would exceed this, need to break into two
            link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, dma_buff.rowBits[row]->getDataPtr(cd, 0), dma_buff.rowBits[row]->size(num_dma_payload_color_depths) );
            previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

            if (m_cfg.double_buff) {
              link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, dma_buff.rowBits[row]->getDataPtr(cd, 1), dma_buff.rowBits[row]->size(num_dma_payload_color_depths));
              previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset]; }

            current_dmadescriptor_offset++;     

          } // additional linked list items           
        }  // row depth struct


        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++) 
        {
            // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
            // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB

          #if SERIAL_DEBUG  
            Serial.printf_P(PSTR("configureDMA(): DMA Loops for PIXEL_COLOR_DEPTH_BITS %d is: %d.\r\n"), i, (1<<(i - lsbMsbTransitionBit - 1)));
          #endif  

            for(int k=0; k < (1<<(i - lsbMsbTransitionBit - 1)); k++) 
            {
                link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, dma_buff.rowBits[row]->getDataPtr(i, 0), dma_buff.rowBits[row]->size(PIXEL_COLOR_DEPTH_BITS - i) );
                previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

                if (m_cfg.double_buff) {
                  link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, dma_buff.rowBits[row]->getDataPtr(i, 1), dma_buff.rowBits[row]->size(PIXEL_COLOR_DEPTH_BITS - i) );
                  previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset];
                }
        
                current_dmadescriptor_offset++;

            } // end color depth ^ 2 linked list
        } // end color depth loop

    } // end frame rows

   #if SERIAL_DEBUG  
      Serial.printf_P(PSTR("configureDMA(): Configured LL structure. %d DMA Linked List descriptors populated.\r\n"), current_dmadescriptor_offset);
	  
	  if ( desccount != current_dmadescriptor_offset)
	  {
		Serial.printf_P(PSTR("configureDMA(): ERROR! Expected descriptor count of %d != actual DMA descriptors of %d!\r\n"), desccount, current_dmadescriptor_offset);		  
	  }
    #endif  

    //End markers for DMA LL
    dmadesc_a[desccount-1].eof = 1;
    dmadesc_a[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_a[0];

    if (m_cfg.double_buff) {    
      dmadesc_b[desccount-1].eof = 1;
      dmadesc_b[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_b[0]; 
    } else {
      dmadesc_b = dmadesc_a; // link to same 'a' buffer
    }

#if SERIAL_DEBUG
    Serial.println(F("Performing I2S setup:"));
#endif

    i2s_parallel_config_t cfg = {
        .gpio_bus={_cfg.gpio.r1, _cfg.gpio.g1, _cfg.gpio.b1, _cfg.gpio.r2, _cfg.gpio.g2, _cfg.gpio.b2, _cfg.gpio.lat, _cfg.gpio.oe, _cfg.gpio.a, _cfg.gpio.b, _cfg.gpio.c, _cfg.gpio.d, _cfg.gpio.e, -1, -1, -1},
        .gpio_clk=_cfg.gpio.clk,
        .sample_rate=_cfg.i2sspeed,   
        .sample_width=ESP32_I2S_DMA_MODE,        
        .desccount_a=desccount,
        .lldesc_a=dmadesc_a,        
        .desccount_b=desccount,
        .lldesc_b=dmadesc_b,
        .clkphase=_cfg.clkphase
    };

    // Setup I2S 
    i2s_parallel_driver_install(I2S_NUM_1, &cfg);
    //i2s_parallel_setup_without_malloc(&I2S1, &cfg);

    // Start DMA Output
    i2s_parallel_send_dma(I2S_NUM_1, &dmadesc_a[0]);

    #if SERIAL_DEBUG  
      Serial.println(F("configureDMA(): DMA setup completed on I2S1.")); 
    #endif       
		
} // end initMatrixDMABuff


/* There are 'bits' set in the frameStruct that we simply don't need to set every single time we change a pixel / DMA buffer co-ordinate.
 * 	For example, the bits that determine the address lines, we don't need to set these every time. Once they're in place, and assuming we
 *  don't accidently clear them, then we don't need to set them again.
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
 */
void IRAM_ATTR MatrixPanel_I2S_DMA::updateMatrixDMABuffer(int16_t x_coord, int16_t y_coord, uint8_t red, uint8_t green, uint8_t blue)
{
    if ( !initialized ) { 
      #if SERIAL_DEBUG 
              Serial.println(F("Cannot updateMatrixDMABuffer as setup failed!"));
      #endif         
      return;
    }

  /* 1) Check that the co-ordinates are within range, or it'll break everything big time.
  * Valid co-ordinates are from 0 to (MATRIX_XXXX-1)
  */
  if ( x_coord < 0 || y_coord < 0 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height) {
    return;
  }

  /* LED Brightness Compensation. Because if we do a basic "red & mask" for example, 
	 * we'll NEVER send the dimmest possible colour, due to binary skew.
	 * i.e. It's almost impossible for color_depth_idx of 0 to be sent out to the MATRIX unless the 'value' of a color is exactly '1'
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

    // We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
    // 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
    x_coord & 1U ? --x_coord : ++x_coord;

    uint16_t _colorbitclear = BITMASK_RGB1_CLEAR, _colorbitoffset = 0;

    if (y_coord >= ROWS_PER_FRAME){    // if we are drawing to the bottom part of the panel
      _colorbitoffset = BITS_RGB2_OFFSET;
      _colorbitclear  = BITMASK_RGB2_CLEAR;
      y_coord -= ROWS_PER_FRAME;
    }

    // Iterating through colour depth bits, which we assume are 8 bits per RGB subpixel (24bpp)
    uint8_t color_depth_idx = PIXEL_COLOR_DEPTH_BITS;
    do {
        --color_depth_idx;
//        uint8_t mask = (1 << (color_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST)); // expect 24 bit color (8 bits per RGB subpixel)
	#if PIXEL_COLOR_DEPTH_BITS < 8
		uint8_t mask = (1 << (color_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
	#else
		uint8_t mask = (1 << (color_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
	#endif		
        uint16_t RGB_output_bits = 0;

        /* Per the .h file, the order of the output RGB bits is:
          * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1	  */
        RGB_output_bits |= (bool)(blue & mask);   // --B
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(green & mask);  // -BG
        RGB_output_bits <<= 1;
        RGB_output_bits |= (bool)(red & mask);    // BGR
        RGB_output_bits <<= _colorbitoffset;      // shift color bits to the required position


        // Get the contents at this address,
        // it would represent a vector pointing to the full row of pixels for the specified color depth bit at Y coordinate
        ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, color_depth_idx, back_buffer_id);


        // We need to update the correct uint16_t word in the rowBitStruct array poiting to a specific pixel at X - coordinate
        p[x_coord] &= _colorbitclear;   // reset RGB bits
        p[x_coord] |= RGB_output_bits;  // set new RGB bits

    } while(color_depth_idx);  // end of color depth loop (8)
} // updateMatrixDMABuffer (specific co-ords change)


/* Update the entire buffer with a single specific colour - quicker */
void MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  if ( !initialized ) return;
  
	/* https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/ */	 
#ifndef NO_CIE1931
	red 	= lumConvTab[red];
	green	= lumConvTab[green];
	blue 	= lumConvTab[blue]; 	
#endif

  for(uint8_t color_depth_idx=0; color_depth_idx<PIXEL_COLOR_DEPTH_BITS; color_depth_idx++)  // color depth - 8 iterations
  {
    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    uint16_t RGB_output_bits = 0;
//    uint8_t mask = (1 << color_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST); // 24 bit color
	#if PIXEL_COLOR_DEPTH_BITS < 8
		uint8_t mask = (1 << (color_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
	#else
		uint8_t mask = (1 << (color_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
	#endif		

	/* Per the .h file, the order of the output RGB bits is:
	 * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1	  */
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
      ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(matrix_frame_parallel_row, color_depth_idx, back_buffer_id);

      // iterate pixels in a row
      int x_coord=dma_buff.rowBits[matrix_frame_parallel_row]->width;
      do { 
        --x_coord;
        p[x_coord] &= BITMASK_RGB12_CLEAR;  // reset color bits
        p[x_coord] |= RGB_output_bits;      // set new color bits
      } while(x_coord);

    } while(matrix_frame_parallel_row); // end row iteration
  } // colour depth loop (8)
} // updateMatrixDMABuffer (full frame paint)

/**
 * @brief - clears and reinitializes color/control data in DMA buffs
 * When allocated, DMA buffs might be dirtry, so we need to blank it and initialize ABCDE,LAT,OE control bits.
 * Those control bits are constants during the entire DMA sweep and never changed when updating just pixel color data
 * so we could set it once on DMA buffs initialization and forget. 
 * This effectively clears buffers to blank BLACK and makes it ready to display output.
 * (Brightness control via OE bit manipulation is another case)
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

    // get last pixel index in a row of all colordepths
    int x_pixel = dma_buff.rowBits[row_idx]->width * dma_buff.rowBits[row_idx]->color_depth;
    //Serial.printf(" from pixel %d, ", x_pixel);

    // fill all x_pixels except color_index[0] (LSB) ones, this also clears all color data to 0's black
    do {
      --x_pixel;
      row[x_pixel] = abcde;
    } while(x_pixel!=dma_buff.rowBits[row_idx]->width);

    // color_index[0] (LSB) x_pixels must be "marked" with a previous's row address, 'cause  it is used to display
    //  previous row while we pump in LSB's for a new row
    abcde = ((ESP32_I2S_DMA_STORAGE_TYPE)row_idx-1) << BITS_ADDR_OFFSET;
    do {
      --x_pixel;
      row[x_pixel] = abcde;
    } while(x_pixel);

    // let's set LAT/OE control bits for specific pixels in each color_index subrows
    uint8_t coloridx = dma_buff.rowBits[row_idx]->color_depth;
    do {
      --coloridx;

      // switch pointer to a row for a specific color index
      row = dma_buff.rowBits[row_idx]->getDataPtr(coloridx, _buff_id);

      // drive latch while shifting out last bit of RGB data
      row[dma_buff.rowBits[row_idx]->width - 2] |= BIT_LAT;   // -1 pixel to compensate array index starting at 0

      // need to disable OE before/after latch to hide row transition
      // Should be one clock or more before latch, otherwise can get ghosting
      uint8_t _blank = m_cfg.latch_blanking;
      do {
        --_blank;
        row[0 + _blank] |= BIT_OE;
        row[dma_buff.rowBits[row_idx]->width - _blank - 3 ] |= BIT_OE;    // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0
      } while (_blank);

    } while(coloridx);

  } while(row_idx);
}

/**
 * @brief - reset OE bits in DMA buffer in a way to control brightness
 * @param brt - brightness level from 0 to row_width
 * @param _buff_id - buffer id to control
 */
void MatrixPanel_I2S_DMA::brtCtrlOE(int brt, const bool _buff_id){
  if (!initialized)
    return;

  if (brt > PIXELS_PER_ROW - m_cfg.latch_blanking)   // can't control values larger than (row_width - latch_blanking) to avoid ongoing issues being raised about brightness and ghosting.
    brt = PIXELS_PER_ROW - m_cfg.latch_blanking;

  if (brt < 0)
    brt = 0;

  // start with iterating all rows in dma_buff structure
  int row_idx = dma_buff.rowBits.size();
  do {
    --row_idx;

    // let's set OE control bits for specific pixels in each color_index subrows
    uint8_t coloridx = dma_buff.rowBits[row_idx]->color_depth;
    do {
      --coloridx;

      // switch pointer to a row for a specific color index
      ESP32_I2S_DMA_STORAGE_TYPE* row = dma_buff.rowBits[row_idx]->getDataPtr(coloridx, _buff_id);

      int x_coord = dma_buff.rowBits[row_idx]->width;
      do {
        --x_coord;

        // Brightness control via OE toggle - disable matrix output at specified x_coord
        if((coloridx > lsbMsbTransitionBit || !coloridx) && ((x_coord) >= brt)){
          row[x_coord] |= BIT_OE; continue;  // For Brightness control
        }
        // special case for the bits *after* LSB through (lsbMsbTransitionBit) - OE is output after data is shifted, so need to set OE to fractional brightness
        if(coloridx && coloridx <= lsbMsbTransitionBit) {
          // divide brightness in half for each bit below lsbMsbTransitionBit
          int lsbBrightness = brt >> (lsbMsbTransitionBit - coloridx + 1);
          if((x_coord) >= lsbBrightness)
            row[x_coord] |= BIT_OE; // For Brightness

          continue;
        }

        // clear OE bit for all other pixels
        row[x_coord] &= BITMASK_OE_CLEAR;
      } while(x_coord);

      // need to disable OE before/after latch to hide row transition
      // Should be one clock or more before latch, otherwise can get ghosting
      uint8_t _blank = m_cfg.latch_blanking;
      do {
        --_blank;
        row[0 + _blank] |= BIT_OE;
        // no need, has been done already
        //row[dma_buff.rowBits[row_idx]->width - _blank - 3 ] |= BIT_OE;    // (LAT pulse is (width-2) -1 pixel to compensate array index starting at 0
      } while (_blank);

    } while(coloridx);
  } while(row_idx);
}


/*
 *  overload for compatibility
 */
bool MatrixPanel_I2S_DMA::begin(int r1, int g1, int b1, int r2, int g2, int b2, int a, int b, int c, int d, int e, int lat, int oe, int clk){

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
  setPanelBrightness(brightness);    // set brighness to reset OE bits to the values matching new LAT blanking setting
  return m_cfg.latch_blanking;
}


#ifndef NO_FAST_FUNCTIONS
/**
 * @brief - update DMA buff drawing horizontal line at specified coordinates
 * @param x_ccord - line start coordinate x
 * @param y_ccord - line start coordinate y
 * @param l - line length
 * @param r,g,b, - RGB888 color
 */
void MatrixPanel_I2S_DMA::hlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue){
  if ( !initialized )
    return;

  if ( x_coord < 0 || y_coord < 0 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
    return;

  if (x_coord+l > PIXELS_PER_ROW)
    l = PIXELS_PER_ROW - x_coord + 1;     // reset width to end of row

  /* LED Brightness Compensation */
#ifndef NO_CIE1931
	red   = lumConvTab[red];
	green = lumConvTab[green];
	blue  = lumConvTab[blue]; 	
#endif

  uint16_t _colorbitclear = BITMASK_RGB1_CLEAR, _colorbitoffset = 0;

  if (y_coord >= ROWS_PER_FRAME){    // if we are drawing to the bottom part of the panel
    _colorbitoffset = BITS_RGB2_OFFSET;
    _colorbitclear  = BITMASK_RGB2_CLEAR;
    y_coord -= ROWS_PER_FRAME;
  }

  // Iterating through color depth bits (8 iterations)
  uint8_t color_depth_idx = PIXEL_COLOR_DEPTH_BITS;
  do {
    --color_depth_idx;

    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
    uint16_t RGB_output_bits = 0;
//    uint8_t mask = (1 << color_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST);
	#if PIXEL_COLOR_DEPTH_BITS < 8
		uint8_t mask = (1 << (color_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
	#else
		uint8_t mask = (1 << (color_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
	#endif		

    /* Per the .h file, the order of the output RGB bits is:
      * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1	  */
    RGB_output_bits |= (bool)(blue & mask);   // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green & mask);  // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red & mask);    // BGR
    RGB_output_bits <<= _colorbitoffset;      // shift color bits to the required position

    // Get the contents at this address,
    // it would represent a vector pointing to the full row of pixels for the specified color depth bit at Y coordinate
    ESP32_I2S_DMA_STORAGE_TYPE *p = dma_buff.rowBits[y_coord]->getDataPtr(color_depth_idx, back_buffer_id);
    // inlined version works slower here, dunno why :(
    // ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(y_coord, color_depth_idx, back_buffer_id);

    int16_t _l = l;
    do {                 // iterate pixels in a row
        int16_t _x = x_coord + --_l;
        // Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
        uint16_t &v = p[_x & 1U ? --_x : ++_x];

        v &= _colorbitclear;      // reset color bits
        v |= RGB_output_bits;    // set new color bits
    } while(_l);        // iterate pixels in a row
  } while(color_depth_idx);  // end of color depth loop (8)
} // hlineDMA()


/**
 * @brief - update DMA buff drawing vertical line at specified coordinates
 * @param x_ccord - line start coordinate x
 * @param y_ccord - line start coordinate y
 * @param l - line length
 * @param r,g,b, - RGB888 color
 */
void MatrixPanel_I2S_DMA::vlineDMA(int16_t x_coord, int16_t y_coord, int16_t l, uint8_t red, uint8_t green, uint8_t blue){
  if ( !initialized )
    return;

  if ( x_coord < 0 || y_coord < 0 || l < 1 || x_coord >= PIXELS_PER_ROW || y_coord >= m_cfg.mx_height)
    return;

  if (y_coord + l > m_cfg.mx_height)
    l = m_cfg.mx_height - y_coord + 1;     // reset width to end of col

  /* LED Brightness Compensation */
#ifndef NO_CIE1931
	red   = lumConvTab[red];
	green = lumConvTab[green];
	blue  = lumConvTab[blue]; 	
#endif

  // Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
  x_coord & 1U ? --x_coord : ++x_coord;

  uint8_t color_depth_idx = PIXEL_COLOR_DEPTH_BITS;
  do {    // Iterating through color depth bits (8 iterations)
    --color_depth_idx;

    // let's precalculate RGB1 and RGB2 bits than flood it over the entire DMA buffer
//    uint8_t mask = (1 << color_depth_idx COLOR_DEPTH_LESS_THAN_8BIT_ADJUST);
	#if PIXEL_COLOR_DEPTH_BITS < 8
		uint8_t mask = (1 << (color_depth_idx+MASK_OFFSET)); // expect 24 bit color (8 bits per RGB subpixel)
	#else
		uint8_t mask = (1 << (color_depth_idx)); // expect 24 bit color (8 bits per RGB subpixel)
	#endif		
    uint16_t RGB_output_bits = 0;

    /* Per the .h file, the order of the output RGB bits is:
    * BIT_B2, BIT_G2, BIT_R2,    BIT_B1, BIT_G1, BIT_R1	  */
    RGB_output_bits |= (bool)(blue & mask);   // --B
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(green & mask);  // -BG
    RGB_output_bits <<= 1;
    RGB_output_bits |= (bool)(red & mask);    // BGR

    int16_t _l = 0, _y = y_coord;
    uint16_t _colorbitclear = BITMASK_RGB1_CLEAR;
    do {    // iterate pixels in a column

      if (_y >= ROWS_PER_FRAME){    // if y-coord overlaped bottom-half panel
        _y -= ROWS_PER_FRAME;
        _colorbitclear  = BITMASK_RGB2_CLEAR;
        RGB_output_bits <<= BITS_RGB2_OFFSET;
      }

      // Get the contents at this address,
      // it would represent a vector pointing to the full row of pixels for the specified color depth bit at Y coordinate
      ESP32_I2S_DMA_STORAGE_TYPE *p = getRowDataPtr(_y, color_depth_idx, back_buffer_id);

      p[x_coord] &= _colorbitclear;   // reset RGB bits
      p[x_coord] |= RGB_output_bits;  // set new RGB bits
      ++_y;
    } while(++_l!=l);        // iterate pixels in a col
  } while(color_depth_idx);  // end of color depth loop (8)
} // vlineDMA()


/**
 * @brief - update DMA buff drawing a rectangular at specified coordinates
 * this works much faster than mulltiple consecutive per-pixel calls to updateMatrixDMABuffer()
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