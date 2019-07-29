#include "ESP32-RGB64x32MatrixPanel-I2S-DMA.h"

// Credits: Louis Beaudoin <https://github.com/pixelmatix/SmartMatrix/tree/teensylc>
// and Sprite_TM: 			https://www.esp32.com/viewtopic.php?f=17&t=3188 and https://www.esp32.com/viewtopic.php?f=13&t=3256

void RGB64x32MatrixPanel_I2S_DMA::configureDMA(int r1_pin, int  g1_pin, int  b1_pin, int  r2_pin, int  g2_pin, int  b2_pin, int  a_pin, int   b_pin, int  c_pin, int  d_pin, int  e_pin, int  lat_pin, int   oe_pin, int clk_pin)
{
    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory
    int numDescriptorsPerRow;
    lsbMsbTransitionBit = 0;
    while(1) {
        numDescriptorsPerRow = 1;
        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
            numDescriptorsPerRow += 1<<(i - lsbMsbTransitionBit - 1);
        }

        int ramrequired = numDescriptorsPerRow * ROWS_PER_FRAME * ESP32_NUM_FRAME_BUFFERS * sizeof(lldesc_t);
        ramrequired += 64000; // HACK Hard Coded: Keep at least 64k free!
        int largestblockfree = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

        Serial.printf("lsbMsbTransitionBit of %d requires %d RAM, %d available, leaving %d free: \r\n", lsbMsbTransitionBit, ramrequired, largestblockfree, largestblockfree - ramrequired);

        if(ramrequired < (largestblockfree))
            break;
            
        if(lsbMsbTransitionBit < COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    if(numDescriptorsPerRow * ROWS_PER_FRAME * ESP32_NUM_FRAME_BUFFERS * sizeof(lldesc_t) > heap_caps_get_largest_free_block(MALLOC_CAP_DMA)){
        assert("Not enough RAM for SmartMatrix descriptors");
        Serial.printf("Not enough RAM for SmartMatrix descriptors\r\n");
        return;
    }

    Serial.printf("Raised lsbMsbTransitionBit to %d/%d to fit in RAM\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1);

    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory that will meet or exceed the configured refresh rate
    while(1) {
        int psPerClock = 1000000000000UL/ESP32_I2S_CLOCK_SPEED;
        int nsPerLatch = ((PIXELS_PER_LATCH + CLKS_DURING_LATCH) * psPerClock) / 1000;
        Serial.printf("ns per latch: %d: \r\n", nsPerLatch);        

        // add time to shift out LSBs + LSB-MSB transition bit - this ignores fractions...
        int nsPerRow = COLOR_DEPTH_BITS * nsPerLatch;

        // add time to shift out MSBs
        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++)
            nsPerRow += (1<<(i - lsbMsbTransitionBit - 1)) * (COLOR_DEPTH_BITS - i) * nsPerLatch;

        //Serial.printf("nsPerRow: %d: \r\n", nsPerRow);        

        int nsPerFrame = nsPerRow * ROWS_PER_FRAME;
        Serial.printf("nsPerFrame: %d: \r\n", nsPerFrame);        

        int actualRefreshRate = 1000000000UL/(nsPerFrame);

        refreshRate = actualRefreshRate;

        Serial.printf("lsbMsbTransitionBit of %d gives %d Hz refresh: \r\n", lsbMsbTransitionBit, actualRefreshRate);        

        if (actualRefreshRate > min_refresh_rate) // HACK Hard Coded: 100
          break;
                  

        if(lsbMsbTransitionBit < COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    Serial.printf("Raised lsbMsbTransitionBit to %d/%d to meet minimum refresh rate\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1);

    // TODO: completely fill buffer with data before enabling DMA - can't do this now, lsbMsbTransition bit isn't set in the calc class - also this call will probably have no effect as matrixCalcDivider will skip the first call
    //matrixCalcCallback();

    // lsbMsbTransition Bit is now finalized - redo descriptor count in case it changed to hit min refresh rate
    numDescriptorsPerRow = 1;
    for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
        numDescriptorsPerRow += 1<<(i - lsbMsbTransitionBit - 1);
    }

    Serial.printf("Descriptors for lsbMsbTransitionBit %d/%d with %d rows require %d bytes of DMA RAM\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1, ROWS_PER_FRAME, 2 * numDescriptorsPerRow * ROWS_PER_FRAME * sizeof(lldesc_t));

    // malloc the DMA linked list descriptors that i2s_parallel will need
    int desccount = numDescriptorsPerRow * ROWS_PER_FRAME;
    lldesc_t * dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Can't allocate descriptor buffer a");
    if(!dmadesc_a) {
        Serial.printf("Could not malloc descriptor buffer a.");
        return;
    }
	
    lldesc_t * dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    assert("Could not malloc descriptor buffer b.");
    if(!dmadesc_b) {
        Serial.printf("can't malloc");
        return;
    }

    Serial.printf("SmartMatrix Mallocs Complete\r\n");
    Serial.printf("Heap Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    Serial.printf("8-bit Accessible Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("32-bit Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
    Serial.printf("DMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    lldesc_t *prevdmadesca = 0;
    lldesc_t *prevdmadescb = 0;
    int currentDescOffset = 0;

    // fill DMA linked lists for both frames
    for(int j=0; j<ROWS_PER_FRAME; j++) {
        // first set of data is LSB through MSB, single pass - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
        // TODO: size must be less than DMA_MAX - worst case for SmartMatrix Library: 16-bpp with 256 pixels per row would exceed this, need to break into two
        link_dma_desc(&dmadesc_a[currentDescOffset], prevdmadesca, &(matrixUpdateFrames[0].rowdata[j].rowbits[0].data), sizeof(rowBitStruct) * COLOR_DEPTH_BITS);
        prevdmadesca = &dmadesc_a[currentDescOffset];
        link_dma_desc(&dmadesc_b[currentDescOffset], prevdmadescb, &(matrixUpdateFrames[1].rowdata[j].rowbits[0].data), sizeof(rowBitStruct) * COLOR_DEPTH_BITS);
        prevdmadescb = &dmadesc_b[currentDescOffset];
        currentDescOffset++;
        //Serial.printf("row %d: \r\n", j);

        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
            // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
            // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB
            //Serial.printf("buffer %d: repeat %d times, size: %d, from %d - %d\r\n", nextBufdescIndex, 1<<(i - LSBMSB_TRANSITION_BIT - 1), (COLOR_DEPTH_BITS - i), i, COLOR_DEPTH_BITS-1);
            for(int k=0; k < 1<<(i - lsbMsbTransitionBit - 1); k++) {
                link_dma_desc(&dmadesc_a[currentDescOffset], prevdmadesca, &(matrixUpdateFrames[0].rowdata[j].rowbits[i].data), sizeof(rowBitStruct) * (COLOR_DEPTH_BITS - i));
                prevdmadesca = &dmadesc_a[currentDescOffset];
                link_dma_desc(&dmadesc_b[currentDescOffset], prevdmadescb, &(matrixUpdateFrames[1].rowdata[j].rowbits[i].data), sizeof(rowBitStruct) * (COLOR_DEPTH_BITS - i));
                prevdmadescb = &dmadesc_b[currentDescOffset];

                currentDescOffset++;
                //Serial.printf("i %d, j %d, k %d\r\n", i, j, k);
            }
        }
    }

    //End markers
    dmadesc_a[desccount-1].eof = 1;
    dmadesc_b[desccount-1].eof = 1;
    dmadesc_a[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_a[0];
    dmadesc_b[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_b[0];

    Serial.printf("Performing I2S setup.\n");

	
    i2s_parallel_config_t cfg={
        .gpio_bus={r1_pin, g1_pin, b1_pin, r2_pin, g2_pin, b2_pin, lat_pin, oe_pin, a_pin, b_pin, c_pin, d_pin, e_pin, -1, -1, -1},
        .gpio_clk=clk_pin,
        .clkspeed_hz=ESP32_I2S_CLOCK_SPEED, //ESP32_I2S_CLOCK_SPEED,  // formula used is 80000000L/(cfg->clkspeed_hz + 1), must result in >=2.  Acceptable values 26.67MHz, 20MHz, 16MHz, 13.34MHz...
        .bits=MATRIX_I2S_MODE, //MATRIX_I2S_MODE,
        .bufa=0,
        .bufb=0,
        desccount,
        desccount,
        dmadesc_a,
        dmadesc_b
    };

    //Setup I2S
    i2s_parallel_setup_without_malloc(&I2S1, &cfg);

    Serial.printf("I2S setup done.\n");
	
	// Just os we know
	dma_configuration_success = true;
	
} // end initMatrixDMABuff

/* Update a specific co-ordinate in the DMA buffer */
void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(int16_t x_coord, int16_t y_coord, uint8_t red, uint8_t green, uint8_t blue)
{
  
    if ( !dma_configuration_success)
      assert("DMA configuration in begin() not performed or completed successfully.");
    
    // Need to check that the co-ordinates are within range, or it'll break everything big time.
	if ( x_coord < 0 || y_coord < 0 || x_coord >= MATRIX_WIDTH || y_coord >= MATRIX_HEIGHT)
    {
		// Serial.printf("Invalid: x %d, y %d - r %d, g %d, b %d\n", (int)x_coord, (int)y_coord, (int)red, (int)green, (int)blue );
		// x_coord = y_coord = 1;				
		return;
    }

    // What half of the HUB75 panel are we painting to?
    bool paint_top_half = true;
    if ( y_coord > ROWS_PER_FRAME-1) // co-ords start at zero, y_coord = 15 = 16 (rows per frame)
    {
        y_coord -= ROWS_PER_FRAME; // if it's 16, subtract 16. Array position 0 again.
        paint_top_half = false;
    }
       
    for(int color_depth_idx=0; color_depth_idx<COLOR_DEPTH_BITS; color_depth_idx++)  // color depth - 8 iterations
    {
        uint16_t mask = (1 << color_depth_idx); // 24 bit color
        
        // The destination for the pixel bitstream 
        rowBitStruct *p = &matrixUpdateFrames[backbuf_id].rowdata[y_coord].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's

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
        if((x_coord) == PIXELS_PER_LATCH-1) v|=BIT_LAT;
		
        // need to turn off OE one clock before latch, otherwise can get ghosting
        if((x_coord)==PIXELS_PER_LATCH-2) v|=BIT_OE;		
		
		
        
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
        



        /* When using the Adafruit drawPixel, we only have one pixel co-ordinate and colour to draw (duh)
         * so we can't paint a top and bottom half (or whatever row split the panel is) at the same time.
         * Need to be smart and check the DMA buffer to see what the other half thinks (pun intended) 
         * and persist this when we refresh.
         * 
         * The DMA buffer order has also been reversed (refer to the last code in this function)
         * so we have to check for this and check the correct position of the MATRIX_DATA_STORAGE_TYPE
         * data.
         */
        int16_t tmp_x_coord = x_coord;
        if(x_coord%2)
        {
          tmp_x_coord -= 1;
        } else {
          tmp_x_coord += 1;
        } // end reordering
                 
        if (paint_top_half)
        { // Need to copy what the RGB status is for the bottom pixels

           // Set the color of the pixel of interest
           if (green & mask)
                v|=BIT_G1;
           if (blue & mask)
                v|=BIT_B1;           
           if (red & mask)
                v|=BIT_R1;

           // Persist what was painted to the other half of the frame equiv. pixel
           if (p->data[tmp_x_coord] & BIT_R2)
                v|=BIT_R2;
                
           if (p->data[tmp_x_coord] & BIT_G2)
                v|=BIT_G2;

           if (p->data[tmp_x_coord] & BIT_B2)
                v|=BIT_B2;
        }
        else
        { // Do it the other way around 

          // Color to set
          if (red & mask)
            v|=BIT_R2;
          if (green & mask) 
            v|=BIT_G2;
          if (blue & mask)
            v|=BIT_B2;
          
          // Copy
          if (p->data[tmp_x_coord] & BIT_R1)
              v|=BIT_R1;
              
          if (p->data[tmp_x_coord] & BIT_G1)
              v|=BIT_G1;
          
          if (p->data[tmp_x_coord] & BIT_B1)
              v|=BIT_B1; 
               
        } // paint
		
		//Serial.printf("x: %d, y: %d ", x_coord, y_coord );
		//Serial.println(v, BIN);


        // 16 bit parallel mode
        //Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
        if(x_coord%2){
          p->data[(x_coord)-1] = v;
        } else {
          p->data[(x_coord)+1] = v;
        } // end reordering
          
    } // color depth loop (8)

  //Show our work!
  //i2s_parallel_flip_to_buffer(&I2S1, backbuf_id);
  
  // If we've linked the DMA output to the same backbuf_id that this function is
  // currently writing too, then the output will be immediate. Else: flipDMABuffer(), then showDMABuffer()
  
  
} // updateDMABuffer


/* Update the entire buffer with a single specific colour - quicker */
void RGB64x32MatrixPanel_I2S_DMA::updateMatrixDMABuffer(uint8_t red, uint8_t green, uint8_t blue)
{
	
  for (unsigned int y_coord = 0; y_coord < ROWS_PER_FRAME; y_coord++) // half height - 16 iterations
  {	
    for(int color_depth_idx=0; color_depth_idx<COLOR_DEPTH_BITS; color_depth_idx++)  // color depth - 8 iterations
    {
        uint16_t mask = (1 << color_depth_idx); // 24 bit color
        
        // The destination for the pixel bitstream 
        rowBitStruct *p = &matrixUpdateFrames[backbuf_id].rowdata[y_coord].rowbits[color_depth_idx]; //matrixUpdateFrames location to write to uint16_t's

        for(int x_coord=0; x_coord < MATRIX_WIDTH; x_coord++) // row pixel width 64 iterations
        { 		
			
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
			if((x_coord) == PIXELS_PER_LATCH-1) v|=BIT_LAT;
			
			// need to turn off OE one clock before latch, otherwise can get ghosting
			if((x_coord)==PIXELS_PER_LATCH-2) v|=BIT_OE;	
			
			
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
			
			
			// Top half colours
			if (green & mask)
				v|=BIT_G1;
			if (blue & mask)
				v|=BIT_B1;           
			if (red & mask)
				v|=BIT_R1;

			// Bottom half colours
			if (red & mask)
			v|=BIT_R2;
			if (green & mask) 
			v|=BIT_G2;
			if (blue & mask)
			v|=BIT_B2;

			
			// 16 bit parallel mode
			//Save the calculated value to the bitplane memory in reverse order to account for I2S Tx FIFO mode1 ordering
			if(x_coord%2)
			{
			  p->data[(x_coord)-1] = v;
			} else {
			  p->data[(x_coord)+1] = v;
			} // end reordering
			
		} // end x_coord iteration
    } // colour depth loop (8)
  } // end row iteration
  
 
   
  //Show our work!
  //i2s_parallel_flip_to_buffer(&I2S1, backbuf_id);
  
  // If we've linked the DMA output to the same backbuf_id that this function is
  // currently writing too, then the output will be immediate. Else: flipDMABuffer(), then showDMABuffer()
  
} // updateDMABuffer
