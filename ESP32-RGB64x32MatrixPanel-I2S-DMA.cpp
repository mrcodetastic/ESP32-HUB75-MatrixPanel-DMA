#include "ESP32-RGB64x32MatrixPanel-I2S-DMA.h"

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


// For development testing only
//#define IGNORE_REFRESH_RATE 1



uint8_t val2PWM(int val) {
    if (val<0) val=0;
    if (val>255) val=255;
    return lumConvTab[val];
}

bool RGB64x32MatrixPanel_I2S_DMA::allocateDMAmemory()
{

   /***
    * Step 1: Look at the overall DMA capable memory for the DMA FRAMEBUFFER data only (not the DMA linked list descriptors yet) 
    *         and do some pre-checks.
    */

    int    _num_frame_buffers                   = (double_buffering_enabled) ? 2:1;
    size_t _frame_buffer_memory_required        = sizeof(frameStruct) * _num_frame_buffers; 
    size_t _dma_linked_list_memory_required     = 0; 
    size_t _total_dma_capable_memory_reserved   = 0;   
    
	// 1. Calculate the amount of DMA capable memory that's actually available
    #if SERIAL_DEBUG    
        Serial.printf("Panel Height: %d pixels.\r\n", MATRIX_HEIGHT);
        Serial.printf("Panel Width: %d pixels.\r\n",  MATRIX_WIDTH);

        if (double_buffering_enabled) {
          Serial.println("DOUBLE FRAME BUFFERS / DOUBLE BUFFERING IS ENABLED. DOUBLE THE RAM REQUIRED!");        
        }
        
        Serial.println("DMA memory blocks available before any malloc's: ");
        heap_caps_print_heap_info(MALLOC_CAP_DMA);
        
        Serial.printf("We're going to need %d bytes of SRAM just for the frame buffer(s).\r\n", _frame_buffer_memory_required);  
		Serial.printf("The total amount of DMA capable SRAM memory is %d bytes.\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));          		
        Serial.printf("Largest DMA capable SRAM memory block is %d bytes.\r\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA));          
		
    #endif

    // Can we potentially fit the framebuffer into the DMA capable memory that's available?
    if ( heap_caps_get_free_size(MALLOC_CAP_DMA) < _frame_buffer_memory_required  ) {
      
      #if SERIAL_DEBUG      
        Serial.printf("######### Insufficient memory for requested resolution. Reduce MATRIX_COLOR_DEPTH and try again.\r\n\tAdditional %d bytes of memory required.\r\n\r\n", (_frame_buffer_memory_required-heap_caps_get_free_size(MALLOC_CAP_DMA)) );
      #endif

      return false;
    }
	
	// Alright, theoretically we should be OK, so let us do this, so
	// lets allocate a chunk of memory for each row (a row could span multiple panels if chaining is in place)
	for (int malloc_num =0; malloc_num < ROWS_PER_FRAME; malloc_num++)
	{
		matrix_row_framebuffer_malloc[malloc_num] = (rowColorDepthStruct *)heap_caps_malloc( (sizeof(rowColorDepthStruct) * _num_frame_buffers) , MALLOC_CAP_DMA);
		// If the ESP crashes here, then we must have a horribly fragmented memory space, or trying to allocate a ludicrous resolution.
 #if SERIAL_DEBUG  
		Serial.printf("Malloc'ing %d bytes of memory @ address %d for frame row %d.\r\n", (sizeof(rowColorDepthStruct) * _num_frame_buffers), matrix_row_framebuffer_malloc[malloc_num], malloc_num);
 #endif	
		if ( matrix_row_framebuffer_malloc[malloc_num] == NULL ) { 
		        Serial.printf("ERROR: Couldn't malloc matrix_row_framebuffer %d! Critical fail.\r\n", malloc_num);            
				return false;
		}    
	}

    _total_dma_capable_memory_reserved += _frame_buffer_memory_required;    


  /***
   * Step 2: Calculate the amount of memory required for the DMA engine's linked list descriptors.
   *         Credit to SmartMatrix for this stuff.
   */    
   

    // Calculate what colour depth is actually possible based on memory available vs. required dma linked-list descriptors.
    // aka. Calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory
    int numDMAdescriptorsPerRow = 0;
    lsbMsbTransitionBit = 0;
	

    while(1) {
        numDMAdescriptorsPerRow = 1;
        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++) {
            numDMAdescriptorsPerRow += (1<<(i - lsbMsbTransitionBit - 1));
        }

        int ramrequired = numDMAdescriptorsPerRow * ROWS_PER_FRAME * _num_frame_buffers * sizeof(lldesc_t);
        int largestblockfree = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
        #if SERIAL_DEBUG  
          Serial.printf("lsbMsbTransitionBit of %d with %d DMA descriptors per frame row, requires %d bytes RAM, %d available, leaving %d free: \r\n", lsbMsbTransitionBit, numDMAdescriptorsPerRow, ramrequired, largestblockfree, largestblockfree - ramrequired);
        #endif

        if(ramrequired < largestblockfree)
            break;
            
        if(lsbMsbTransitionBit < PIXEL_COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    Serial.printf("Raised lsbMsbTransitionBit to %d/%d to fit in remaining RAM\r\n", lsbMsbTransitionBit, PIXEL_COLOR_DEPTH_BITS - 1);


   #ifndef IGNORE_REFRESH_RATE	
    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory that will meet or exceed the configured refresh rate
    while(1) {           
        int psPerClock = 1000000000000UL/ESP32_I2S_CLOCK_SPEED;
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
          Serial.printf("lsbMsbTransitionBit of %d gives %d Hz refresh: \r\n", lsbMsbTransitionBit, actualRefreshRate);        
		    #endif

        if (actualRefreshRate > min_refresh_rate) // HACK Hard Coded: 100
          break;
                  
        if(lsbMsbTransitionBit < PIXEL_COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    Serial.printf("Raised lsbMsbTransitionBit to %d/%d to meet minimum refresh rate\r\n", lsbMsbTransitionBit, PIXEL_COLOR_DEPTH_BITS - 1);
	#endif

  /***
   * Step 2a: lsbMsbTransition bit is now finalised - recalculate the DMA descriptor count required, which is used for
   *          memory allocation of the DMA linked list memory structure.
   */          
    numDMAdescriptorsPerRow = 1;
    for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++) {
        numDMAdescriptorsPerRow += (1<<(i - lsbMsbTransitionBit - 1));
    }

    // Refer to 'DMA_LL_PAYLOAD_SPLIT' code in configureDMA() below to understand why this exists.
    // numDMAdescriptorsPerRow is also used to calcaulte descount which is super important in i2s_parallel_config_t SoC DMA setup. 
    if ( sizeof(rowColorDepthStruct) > DMA_MAX ) {

        #if SERIAL_DEBUG  
          Serial.println("Split DMA payload required.");        
		    #endif      

        numDMAdescriptorsPerRow += PIXEL_COLOR_DEPTH_BITS-1; 
        // Not if numDMAdescriptorsPerRow is even just one descriptor too large, DMA linked list will not correctly loop.
    }


  /***
   * Step 3: Allocate memory for DMA linked list, linking up each framebuffer row in sequence for GPIO output.
   */        

    _dma_linked_list_memory_required = numDMAdescriptorsPerRow * ROWS_PER_FRAME * _num_frame_buffers * sizeof(lldesc_t);
    #if SERIAL_DEBUG 	
		Serial.printf("Descriptors for lsbMsbTransitionBit of %d/%d with %d frame rows require %d bytes of DMA RAM with %d numDMAdescriptorsPerRow.\r\n", lsbMsbTransitionBit, PIXEL_COLOR_DEPTH_BITS - 1, ROWS_PER_FRAME, _dma_linked_list_memory_required, numDMAdescriptorsPerRow);    
	#endif   

    _total_dma_capable_memory_reserved += _dma_linked_list_memory_required;

    // Do a final check to see if we have enough space for the additional DMA linked list descriptors that will be required to link it all up!
    if(_dma_linked_list_memory_required > heap_caps_get_largest_free_block(MALLOC_CAP_DMA)) {
       Serial.printf("ERROR: Not enough SRAM left over for DMA linked-list descriptor memory reservation! Oh so close!\r\n");
  
        return false;
    } // linked list descriptors memory check

    // malloc the DMA linked list descriptors that i2s_parallel will need
    desccount = numDMAdescriptorsPerRow * ROWS_PER_FRAME;

    //lldesc_t * dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Can't allocate descriptor framebuffer a");
    if(!dmadesc_a) {
        Serial.printf("ERROR: Could not malloc descriptor framebuffer a.");
        return false;
    }
	
    if (double_buffering_enabled) // reserve space for second framebuffer linked list
    {
        //lldesc_t * dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
        dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
        assert("Could not malloc descriptor framebuffer b.");
        if(!dmadesc_b) {
            Serial.printf("ERROR: Could not malloc descriptor framebuffer b.");
            return false;
        }
    }

    Serial.printf("*** ESP32-RGB64x32MatrixPanel-I2S-DMA: Memory Allocations Complete *** \r\n");
    Serial.printf("Total memory that was reserved: %d kB.\r\n", _total_dma_capable_memory_reserved/1024);
	Serial.printf("... of which was used for the DMA Linked List(s): %d kB.\r\n", _dma_linked_list_memory_required/1024);
	
    Serial.printf("Heap Memory Available: %d bytes total. Largest free block: %d bytes.\r\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    Serial.printf("General RAM Available: %d bytes total. Largest free block: %d bytes.\r\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));


    #if SERIAL_DEBUG    
        Serial.println("DMA capable memory map available after malloc's: ");
        heap_caps_print_heap_info(MALLOC_CAP_DMA);
        delay(1000);
    #endif

    // Just os we know
  	everything_OK = true;

    return true;

} // end initMatrixDMABuffer()



void RGB64x32MatrixPanel_I2S_DMA::configureDMA(int r1_pin, int  g1_pin, int  b1_pin, int  r2_pin, int  g2_pin, int  b2_pin, int  a_pin, int   b_pin, int  c_pin, int  d_pin, int  e_pin, int  lat_pin, int   oe_pin, int clk_pin)
{
    #if SERIAL_DEBUG  
      Serial.println("configureDMA(): Starting configuration of DMA engine.\r\n");
    #endif   


    lldesc_t *previous_dmadesc_a     = 0;
    lldesc_t *previous_dmadesc_b     = 0;
    int current_dmadescriptor_offset = 0;

    // HACK: If we need to split the payload in 1/2 so that it doesn't breach DMA_MAX, lets do it by the color_depth.
    int num_dma_payload_color_depths = PIXEL_COLOR_DEPTH_BITS;
    if ( sizeof(rowColorDepthStruct) > DMA_MAX ) {
        num_dma_payload_color_depths = 1;
    }

    // Fill DMA linked lists for both frames (as in, halves of the HUB75 panel) and if double buffering is enabled, link it up for both buffers.
    for(int row = 0; row < ROWS_PER_FRAME; row++) {

        // Split framebuffer malloc hack 'improvement'
        rowColorDepthStruct *fb_malloc_ptr = matrix_row_framebuffer_malloc[row]; 

        #if SERIAL_DEBUG          
          Serial.printf("DMA payload of %d bytes. DMA_MAX is %d.\r\n", sizeof(rowBitStruct) * PIXEL_COLOR_DEPTH_BITS, DMA_MAX);
        #endif        

        
        // first set of data is LSB through MSB, single pass (IF TOTAL SIZE < DMA_MAX) - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
        // NOTE: size must be less than DMA_MAX - worst case for library: 16-bpp with 256 pixels per row would exceed this, need to break into two
        link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, &(fb_malloc_ptr[0].rowbits[0].data), sizeof(rowBitStruct) * num_dma_payload_color_depths);
          previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

        if (double_buffering_enabled) {
          link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, &(fb_malloc_ptr[1].rowbits[0].data), sizeof(rowBitStruct) * num_dma_payload_color_depths);
          previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset]; }

        current_dmadescriptor_offset++;

        // If the number of pixels per row is to great for the size of a DMA payload, so we need to split what we were going to send above.
        if ( sizeof(rowColorDepthStruct) > DMA_MAX ) 
        {
                   
          #if SERIAL_DEBUG     
              Serial.printf("Spliting DMA payload for %d color depths into %d byte payloads.\r\n", PIXEL_COLOR_DEPTH_BITS-1, sizeof(rowBitStruct) );
          #endif
          
          for (int cd = 1; cd < PIXEL_COLOR_DEPTH_BITS; cd++) 
          {
            // first set of data is LSB through MSB, single pass - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
            // TODO: size must be less than DMA_MAX - worst case for library: 16-bpp with 256 pixels per row would exceed this, need to break into two
            link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, &(fb_malloc_ptr[0].rowbits[cd].data), sizeof(rowBitStruct) );
            previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

            if (double_buffering_enabled) {
              link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, &(fb_malloc_ptr[1].rowbits[cd].data), sizeof(rowBitStruct) );
            previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset]; }

            current_dmadescriptor_offset++;     

          } // additional linked list items           
        }  // row depth struct


        for(int i=lsbMsbTransitionBit + 1; i<PIXEL_COLOR_DEPTH_BITS; i++) 
        {
            // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
            // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB
            //Serial.printf("buffer %d: repeat %d times, size: %d, from %d - %d\r\n", current_dmadescriptor_offset, 1<<(i - lsbMsbTransitionBit - 1), (PIXEL_COLOR_DEPTH_BITS - i), i, PIXEL_COLOR_DEPTH_BITS-1);

          #if SERIAL_DEBUG  
            Serial.printf("configureDMA(): DMA Loops for PIXEL_COLOR_DEPTH_BITS %d is: %d.\r\n", i, (1<<(i - lsbMsbTransitionBit - 1)));
          #endif  

            for(int k=0; k < (1<<(i - lsbMsbTransitionBit - 1)); k++) 
            {
                link_dma_desc(&dmadesc_a[current_dmadescriptor_offset], previous_dmadesc_a, &(fb_malloc_ptr[0].rowbits[i].data), sizeof(rowBitStruct) * (PIXEL_COLOR_DEPTH_BITS - i));
                previous_dmadesc_a = &dmadesc_a[current_dmadescriptor_offset];

                if (double_buffering_enabled) {
                  link_dma_desc(&dmadesc_b[current_dmadescriptor_offset], previous_dmadesc_b, &(fb_malloc_ptr[1].rowbits[i].data), sizeof(rowBitStruct) * (PIXEL_COLOR_DEPTH_BITS - i));
                previous_dmadesc_b = &dmadesc_b[current_dmadescriptor_offset]; }
        
                current_dmadescriptor_offset++;

            } // end color depth ^ 2 linked list
        } // end color depth loop

    } // end frame rows

   #if SERIAL_DEBUG  
      Serial.printf("configureDMA(): Configured LL structure. %d DMA Linked List descriptors populated.\r\n", current_dmadescriptor_offset);
    #endif  

      dmadesc_a[desccount-1].eof = 1;
      dmadesc_a[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_a[0];

    //End markers for DMA LL
    if (double_buffering_enabled) {    
      dmadesc_b[desccount-1].eof = 1;
      dmadesc_b[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_b[0]; 
    } else {
      dmadesc_b = dmadesc_a; // link to same 'a' buffer
    }

    
/*
    //End markers
    dmadesc_a[desccount-1].eof = 1;
    dmadesc_b[desccount-1].eof = 1;
    dmadesc_a[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_a[0];
    dmadesc_b[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_b[0];   
*/
    //Serial.printf("Performing I2S setup.\n");
	
    i2s_parallel_config_t cfg={
        .gpio_bus={r1_pin, g1_pin, b1_pin, r2_pin, g2_pin, b2_pin, lat_pin, oe_pin, a_pin, b_pin, c_pin, d_pin, e_pin, -1, -1, -1},
        .gpio_clk=clk_pin,
        .clkspeed_hz=ESP32_I2S_CLOCK_SPEED, //ESP32_I2S_CLOCK_SPEED,  // formula used is 80000000L/(cfg->clkspeed_hz + 1), must result in >=2.  Acceptable values 26.67MHz, 20MHz, 16MHz, 13.34MHz...
        .bits=ESP32_I2S_DMA_MODE, //ESP32_I2S_DMA_MODE,
        .bufa=0,
        .bufb=0,
        desccount,
        desccount,
        dmadesc_a,
        dmadesc_b
    };

    //Setup I2S
    i2s_parallel_setup_without_malloc(&I2S1, &cfg);

    #if SERIAL_DEBUG  
      Serial.println("configureDMA(): DMA configuration completed on I2S1.\r\n");
    #endif      

    #if SERIAL_DEBUG  
        Serial.println("DMA Memory Map after DMA LL allocations: ");
            heap_caps_print_heap_info(MALLOC_CAP_DMA);        

        delay(1000);
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
#define GO_FOR_SPEED 1

#ifdef GO_FOR_SPEED
/* Update a specific co-ordinate in the DMA buffer */
void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(int16_t x_coord, int16_t y_coord, uint8_t red, uint8_t green, uint8_t blue)
{
	
    // Check that the co-ordinates are within range, or it'll break everything big time.
    // Valid co-ordinates are from 0 to (MATRIX_XXXX-1)
	if ( x_coord < 0 || y_coord < 0 || x_coord >= MATRIX_WIDTH || y_coord >= MATRIX_HEIGHT) {
		  return;
    }
	
	// https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
	red 	= lumConvTab[red];
	green 	= lumConvTab[green];
	blue 	= lumConvTab[blue]; 	

    bool painting_top_frame = true;
    if ( y_coord >= ROWS_PER_FRAME) // co-ords start at zero, y_coord = 15 = 16 (rows per frame)
    {
        y_coord -= ROWS_PER_FRAME;  // Subtract the ROWS_PER_FRAME from the pixel co-ord to get the panel ROW (not really the 'y_coord' anymore)
        painting_top_frame = false;
    }
	
	// We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
	// 16 bit parallel mode - Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
	int rowBitStruct_x_coord_uint16_t_position = (x_coord % 2) ? (x_coord-1):(x_coord+1);

	// Find the memory address for the malloc for this framebuffer row.
	rowColorDepthStruct *fb_row_malloc_ptr = (rowColorDepthStruct *) matrix_row_framebuffer_malloc[y_coord]; 	
	
    for(int color_depth_idx=0; color_depth_idx<PIXEL_COLOR_DEPTH_BITS; color_depth_idx++)  // color depth - 8 iterations
    {
        uint8_t mask = (1 << color_depth_idx); // PWM bit colour mask (max 8bits per pixel colour)
        
        // The destination for the pixel bitstream 
        //rowBitStruct *p = &matrix_framebuffer_malloc_1[back_buffer_id].rowdata[y_coord].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's
        // Get the contents at this address, cast as a rowColorDepthStruct  
        //rowBitStruct *p = &fb_row_malloc_ptr[back_buffer_id].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's
		uint16_t &v = fb_row_malloc_ptr[back_buffer_id].rowbits[color_depth_idx].data[rowBitStruct_x_coord_uint16_t_position]; 
		
        //int v=0; // the output bitstream
      
        if (painting_top_frame) // Painting to pixel in the top half of the HUB75 panel use the R1, B1 and G1 pins
        { 
           // Set the colour of the pixel of interest
		   // https://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit
          if (red & mask)   { v|=BIT_R1; }  else {  v &= ~(BIT_R1); }
          if (green & mask) { v|=BIT_G1; }  else {  v &= ~(BIT_G1); }
          if (blue & mask)  { v|=BIT_B1; }  else {  v &= ~(BIT_B1); }
        }
        else
        { // Paint to a pixel in the bottom half
	
          if (red & mask)   { v|=BIT_R2; } else {  v &= ~(BIT_R2); }
          if (green & mask) { v|=BIT_G2; } else {  v &= ~(BIT_G2); }
          if (blue & mask)  { v|=BIT_B2; } else {  v &= ~(BIT_B2); }
 
        } // paint

        // 16 bit parallel mode
        //Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
        //p->data[rowBitStruct_x_coord_uint16_t_position] = v;	
		// NOTE: No need to do this as 'v' is now a reference directly to the frameStruct
		
    } // color depth loop (8)
	
} // updateMatrixDMABuffer (specific co-ords change)

#else

/* Update a specific co-ordinate in the DMA buffer */
/* Original version were we re-create the bitstream from scratch for each x,y co-ordinate / pixel changed. Slightly slower. */
void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(int16_t x_coord, int16_t y_coord, uint8_t red, uint8_t green, uint8_t blue)
{
    if ( !everything_OK ) { 

      #if SERIAL_DEBUG 
              Serial.println("Cannot updateMatrixDMABuffer as setup failed!");
      #endif         
      
      return;
    }
	
	/* LED Brightness Compensation. Because if we do a basic "red & mask" for example, 
	 * we'll NEVER send the dimmest possible colour, due to binary skew.
	   
	   i.e. It's almost impossible for color_depth_idx of 0 to be sent out to the MATRIX unless the 'value' of a color is exactly '1'
	   
	 */	 
	red 	= lumConvTab[red];
	green 	= lumConvTab[green];
	blue 	= lumConvTab[blue]; 	

    
   /* 1) Check that the co-ordinates are within range, or it'll break everything big time.
    * Valid co-ordinates are from 0 to (MATRIX_XXXX-1)
    */
	  if ( x_coord < 0 || y_coord < 0 || x_coord >= MATRIX_WIDTH || y_coord >= MATRIX_HEIGHT) {
      return;
    }

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
    bool painting_top_frame = true;
    if ( y_coord >= ROWS_PER_FRAME) // co-ords start at zero, y_coord = 15 = 16 (rows per frame)
    {
        y_coord -= ROWS_PER_FRAME;  // Subtract the ROWS_PER_FRAME from the pixel co-ord to get the panel ROW (not really the 'y_coord' anymore)
        painting_top_frame = false;
    }
	
	// We need to update the correct uint16_t in the rowBitStruct array, that gets sent out in parallel
	int rowBitStruct_x_coord_uint16_t_position = (x_coord % 2) ? (x_coord-1):(x_coord+1);
	
    for(int color_depth_idx=0; color_depth_idx<PIXEL_COLOR_DEPTH_BITS; color_depth_idx++)  // color depth - 8 iterations
    {
        int mask = (1 << color_depth_idx); // 24 bit color
        
        // The destination for the pixel bitstream 
        //rowBitStruct *p = &matrix_framebuffer_malloc_1[back_buffer_id].rowdata[y_coord].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's

        // Find the memory address for the malloc for this framebuffer row.
        rowColorDepthStruct *fb_row_malloc_ptr = (rowColorDepthStruct *) matrix_row_framebuffer_malloc[y_coord]; 
        // Get the contents at this address, cast as a rowColorDepthStruct  
        rowBitStruct *p = &fb_row_malloc_ptr[back_buffer_id].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's



		// int v = p->data[rowBitStruct_x_coord_uint16_t_position]; // persist what we already have
        int v=0; // the output bitstream
        
        // if there is no latch to hold address, output ADDX lines directly to GPIO and latch data at end of cycle
        int gpioRowAddress = y_coord;
        
        // normally output current rows ADDX, special case for LSB, output previous row's ADDX (as previous row is being displayed for one latch cycle)
        if(color_depth_idx == 0)
          gpioRowAddress = y_coord-1;
        
        if (gpioRowAddress & 0x01) v|=BIT_A; // 1
        if (gpioRowAddress & 0x02) v|=BIT_B; // 2
        if (gpioRowAddress & 0x04) v|=BIT_C; // 4
        if (gpioRowAddress & 0x08) v|=BIT_D; // 8
        if (gpioRowAddress & 0x10) v|=BIT_E; // 16
  
        // need to disable OE after latch to hide row transition
        if((x_coord) == 0 ) v|=BIT_OE;
        
        // drive latch while shifting out last bit of RGB data
        if((x_coord) == PIXELS_PER_ROW-1) v|=BIT_LAT;
		
        // need to turn off OE one clock before latch, otherwise can get ghosting
        if((x_coord)==PIXELS_PER_ROW-2) v|=BIT_OE;		
		        
        // turn off OE after brightness value is reached when displaying MSBs
        // MSBs always output normal brightness
        // LSB (!color_depth_idx) outputs normal brightness as MSB from previous row is being displayed
        if((color_depth_idx > lsbMsbTransitionBit || !color_depth_idx) && ((x_coord) >= brightness)) v|=BIT_OE; // For Brightness
        
        // special case for the bits *after* LSB through (lsbMsbTransitionBit) - OE is output after data is shifted, so need to set OE to fractional brightness
        if(color_depth_idx && color_depth_idx <= lsbMsbTransitionBit) {
          // divide brightness in half for each bit below lsbMsbTransitionBit
          int lsbBrightness = brightness >> (lsbMsbTransitionBit - color_depth_idx + 1);
          if((x_coord) >= lsbBrightness) v|=BIT_OE; // For Brightness
        }
        
		/*
		// Development / testing code only.
		Serial.printf("r value of %d, color depth: %d, mask: %d\r\n", red,	color_depth_idx, mask);
		if (red & mask) { Serial.println("Success - Binary"); v|=BIT_R1; }
		Serial.printf("val2pwm r value:  %d\r\n", val2PWM(red));
		if (val2PWM(red) & mask) { Serial.println("Success - PWM"); v|=BIT_R2; }
		*/
		
                 
        if (painting_top_frame)
        { // Need to copy what the RGB status is for the bottom pixels

           // Set the color of the pixel of interest
           if (green & mask) {  v|=BIT_G1; }
           if (blue & mask)  {  v|=BIT_B1; }
           if (red & mask)   {  v|=BIT_R1; }

           // Persist what was painted to the other half of the frame equiv. pixel
           if (p->data[rowBitStruct_x_coord_uint16_t_position] & BIT_R2)
                v|=BIT_R2;
                
           if (p->data[rowBitStruct_x_coord_uint16_t_position] & BIT_G2)
                v|=BIT_G2;

           if (p->data[rowBitStruct_x_coord_uint16_t_position] & BIT_B2)
                v|=BIT_B2;
        }
        else
        { // Do it the other way around 

          // Color to set
          if (red & mask)   { v|=BIT_R2; }
          if (green & mask) { v|=BIT_G2; }
          if (blue & mask)  { v|=BIT_B2; }
          
          // Copy / persist
          if (p->data[rowBitStruct_x_coord_uint16_t_position] & BIT_R1)
              v|=BIT_R1;
              
          if (p->data[rowBitStruct_x_coord_uint16_t_position] & BIT_G1)
              v|=BIT_G1;
          
          if (p->data[rowBitStruct_x_coord_uint16_t_position] & BIT_B1)
              v|=BIT_B1; 
               
        } // paint
		
        // 16 bit parallel mode
        //Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
		/*
        if(x_coord%2){
          p->data[(x_coord)-1] = v;
        } else {
          p->data[(x_coord)+1] = v;
        } // end reordering
		*/

		// 16 bit parallel mode
        p->data[rowBitStruct_x_coord_uint16_t_position] = v;		
		
          
    } // color depth loop (8)

} // updateMatrixDMABuffer (specific co-ords change)
#endif	


/* Update the entire buffer with a single specific colour - quicker */
void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)
{
  if ( !everything_OK ) return;
  
	/* https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/ */	 
	/*
	red 	= val2PWM(red);
	green 	= val2PWM(green);
	blue 	= val2PWM(blue);  
	*/
	red 	= lumConvTab[red];
	green 	= lumConvTab[green];
	blue 	= lumConvTab[blue]; 	

  for (unsigned int matrix_frame_parallel_row = 0; matrix_frame_parallel_row < ROWS_PER_FRAME; matrix_frame_parallel_row++) // half height - 16 iterations
  {	
    for(int color_depth_idx=0; color_depth_idx<PIXEL_COLOR_DEPTH_BITS; color_depth_idx++)  // color depth - 8 iterations
    {
        uint16_t mask = (1 << color_depth_idx); // 24 bit color
        
        // The destination for the pixel bitstream 
        //rowBitStruct *p = &matrix_framebuffer_malloc_1[back_buffer_id].rowdata[matrix_frame_parallel_row].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's 
        rowColorDepthStruct *fb_row_malloc_ptr = (rowColorDepthStruct *) matrix_row_framebuffer_malloc[matrix_frame_parallel_row]; 
        //Serial.printf("Accessing fb address: %d\r\n", fb_row_malloc_ptr);
        
        rowBitStruct *p = &fb_row_malloc_ptr[back_buffer_id].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's

        for(int x_coord=0; x_coord < MATRIX_WIDTH; x_coord++) // row pixel width 64 iterations
        { 		
          
          int v=0; // the output bitstream
          
          // if there is no latch to hold address, output ADDX lines directly to GPIO and latch data at end of cycle
          int gpioRowAddress = matrix_frame_parallel_row;
          
          // normally output current rows ADDX, special case for LSB, output previous row's ADDX (as previous row is being displayed for one latch cycle)
          if(color_depth_idx == 0)
            gpioRowAddress = matrix_frame_parallel_row-1;
          
          if (gpioRowAddress & 0x01) v|=BIT_A; // 1
          if (gpioRowAddress & 0x02) v|=BIT_B; // 2
          if (gpioRowAddress & 0x04) v|=BIT_C; // 4
          if (gpioRowAddress & 0x08) v|=BIT_D; // 8
          if (gpioRowAddress & 0x10) v|=BIT_E; // 16
          
            
          /* ORIG
          // need to disable OE after latch to hide row transition
          if((x_coord) == 0) v|=BIT_OE;
          
          // drive latch while shifting out last bit of RGB data
          if((x_coord) == PIXELS_PER_LATCH-1) v|=BIT_LAT;
          
          // need to turn off OE one clock before latch, otherwise can get ghosting
          if((x_coord)==PIXELS_PER_LATCH-1) v|=BIT_OE;
          */
          
          // need to disable OE after latch to hide row transition
          if((x_coord) == 0 ) v|=BIT_OE;
          
          // drive latch while shifting out last bit of RGB data
          if((x_coord) == PIXELS_PER_ROW-1) v|=BIT_LAT;
          
          // need to turn off OE one clock before latch, otherwise can get ghosting
          if((x_coord)==PIXELS_PER_ROW-2) v|=BIT_OE;	
          
          
          // turn off OE after brightness value is reached when displaying MSBs
          // MSBs always output normal brightness
          // LSB (!color_depth_idx) outputs normal brightness as MSB from previous row is being displayed
          if((color_depth_idx > lsbMsbTransitionBit || !color_depth_idx) && ((x_coord) >= brightness)) v|=BIT_OE; // For Brightness
          
          // special case for the bits *after* LSB through (lsbMsbTransitionBit) - OE is output after data is shifted, so need to set OE to fractional brightness
          if(color_depth_idx && color_depth_idx <= lsbMsbTransitionBit) {
            // divide brightness in half for each bit below lsbMsbTransitionBit
            int lsbBrightness = brightness >> (lsbMsbTransitionBit - color_depth_idx + 1);
            if((x_coord) >= lsbBrightness) v|=BIT_OE; // For Brightness
          }
          
          // Top and bottom matrix MATRIX_ROWS_IN_PARALLEL half colours
          if (green & mask) { v|=BIT_G1; v|=BIT_G2; }
          if (blue & mask)  { v|=BIT_B1; v|=BIT_B2; }
          if (red & mask)   { v|=BIT_R1; v|=BIT_R2; }
          
          // 16 bit parallel mode
          //Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
          if(x_coord%2) {
            p->data[(x_coord)-1] = v;
          } else {
            p->data[(x_coord)+1] = v;
          } // end reordering
          
        } // end x_coord iteration
    } // colour depth loop (8)
  } // end row iteration

} // updateMatrixDMABuffer (full frame paint)