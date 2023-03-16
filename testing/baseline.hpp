
/**
 * Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
 * Updates the private class member variable 'coords', so no need to use the return value.
 * Not thread safe, but not a concern for ESP32 sketch anyway... I think.
 */
// DO NOT CHANGE
inline VirtualCoords VirtualMatrixPanelTest::getCoords_WorkingBaslineMarch2023(int16_t virt_x, int16_t virt_y)
{
    coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer

	if (virt_x < 0 || virt_x >= virtualResX || virt_y < 0 || virt_y >= virtualResY)
	{ // Co-ordinates go from 0 to X-1 remember! otherwise they are out of range!
		return coords;
	}

    // Do we want to rotate?
    if (_rotate)
    {
        int16_t temp_x = virt_x;
        virt_x = virt_y;
        virt_y = virtualResY - 1 - temp_x;
    }

	int row  = (virt_y / panelResY); // 0 indexed
	switch(panel_chain_type)
	{
			
		case (CHAIN_TOP_RIGHT_DOWN): 
		{
            if ( (row % 2) == 1 ) 
            { 	// upside down panel

                //Serial.printf("Condition 1, row %d ", row);

                // refersed for the row
                coords.x = dmaResX - virt_x - (row*virtualResX);			

                // y co-ord inverted within the panel
                coords.y = panelResY - 1 - (virt_y % panelResY);	
            

            }
            else
            { 
                //Serial.printf("Condition 2, row %d ", row);

                coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
                coords.y =  virt_y % panelResY;	
                        
            }

		}
			break;	


		case (CHAIN_TOP_LEFT_DOWN): // OK -> modulus opposite of CHAIN_TOP_RIGHT_DOWN
		{
			if ( (row % 2) == 0 ) 
			{ 	// refersed panel 

                //Serial.printf("Condition 1, row %d ", row);
				coords.x = dmaResX - virt_x - (row*virtualResX);			

				// y co-ord inverted within the panel
				coords.y = panelResY - 1 - (virt_y % panelResY);	

			}
			else
			{ 
                //Serial.printf("Condition 2, row %d ", row);
				coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
			    coords.y =  virt_y % panelResY;	
						
			}

		}
			break;	            
			
			


		case (CHAIN_BOTTOM_LEFT_UP): 	// 
		{
            row = vmodule_rows - row - 1;
		
			if ( (row % 2) == 1 ) 
			{ 	
                // Serial.printf("Condition 1, row %d ", row);
				coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
				coords.y = virt_y % panelResY;	
 
			}
			else
			{  // inverted panel                     

                // Serial.printf("Condition 2, row %d ", row);
				coords.x =  dmaResX - (row*virtualResX) - virt_x;         
				coords.y = panelResY - 1 - (virt_y % panelResY);
			}
			
		}
			break;
	
		case (CHAIN_BOTTOM_RIGHT_UP): 	// OK -> modulus opposite of CHAIN_BOTTOM_LEFT_UP
		{
            row = vmodule_rows - row - 1;
		
			if ( (row % 2) == 0 ) 
			{ 	// right side up

                // Serial.printf("Condition 1, row %d ", row);
				// refersed for the row
				coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
				coords.y = virt_y % panelResY;	
 
			}
			else
			{  // inverted panel                     

                // Serial.printf("Condition 2, row %d ", row);
				coords.x =  dmaResX - (row*virtualResX) - virt_x;         
				coords.y = panelResY - 1 - (virt_y % panelResY);
			}
			
		}
			break;
						

		default:
            return coords;
			break;
			
	} // end switch
	


  /* START: Pixel remapping AGAIN to convert TWO parallel scanline output that the
   *        the underlying hardware library is designed for (because
   *        there's only 2 x RGB pins... and convert this to 1/4 or something
   */
  if (panel_scan_rate == FOUR_SCAN_32PX_HIGH)
  {
    /* Convert Real World 'VirtualMatrixPanel' co-ordinates (i.e. Real World pixel you're looking at
       on the panel or chain of panels, per the chaining configuration) to a 1/8 panels
       double 'stretched' and 'squished' coordinates which is what needs to be sent from the
       DMA buffer.

       Note: Look at the FourScanPanel example code and you'll see that the DMA buffer is setup
       as if the panel is 2 * W and 0.5 * H !
    */

    if ((virt_y & 8) == 0)
    {
      coords.x += ((coords.x / panelResX) + 1) * panelResX; // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }
    else
    {
      coords.x += (coords.x / panelResX) * panelResX; // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }

    // http://cpp.sh/4ak5u
    // Real number of DMA y rows is half reality
    // coords.y = (y / 16)*8 + (y & 0b00000111);
    coords.y = (virt_y >> 4) * 8 + (virt_y & 0b00000111);

  }
  else if (panel_scan_rate == FOUR_SCAN_16PX_HIGH)
  {
    if ((virt_y & 8) == 0)
    {
      coords.x += (panelResX >> 2) * (((coords.x & 0xFFF0) >> 4) + 1); // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }
    else
    {
      coords.x += (panelResX >> 2) * (((coords.x & 0xFFF0) >> 4)); // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }

    if (virt_y < 32)
      coords.y = (virt_y >> 4) * 8 + (virt_y & 0b00000111);
    else
    {
      coords.y = ((virt_y - 32) >> 4) * 8 + (virt_y & 0b00000111);
      coords.x += 256;
    }
  }

  return coords;
}
