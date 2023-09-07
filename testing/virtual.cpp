#include <iostream>
#include <string>
#include <list>


struct VirtualCoords
{
  int16_t x;
  int16_t y;
  int16_t virt_row; // chain of panels row
  int16_t virt_col; // chain of panels col

  VirtualCoords() : x(0), y(0)
  {
  }
};

enum PANEL_SCAN_RATE
{
  NORMAL_TWO_SCAN, NORMAL_ONE_SIXTEEN, // treated as the same
  FOUR_SCAN_32PX_HIGH,
  FOUR_SCAN_16PX_HIGH
};

// Chaining approach... From the perspective of the DISPLAY / LED side of the chain of panels.
enum PANEL_CHAIN_TYPE
{
	CHAIN_TOP_LEFT_DOWN,
	CHAIN_TOP_RIGHT_DOWN,
	CHAIN_BOTTOM_LEFT_UP,
	CHAIN_BOTTOM_RIGHT_UP,
	CHAIN_TOP_RIGHT_DOWN_ZZ, /// ZigZag chaining. Might need a big ass cable to do this, all panels right way up.
	CHAIN_BOTTOM_RIGHT_UP_ZZ
};


class VirtualMatrixPanelTest
{

public:
  VirtualMatrixPanelTest(int _vmodule_rows, int _vmodule_cols, int _panelResX, int _panelResY, PANEL_CHAIN_TYPE _panel_chain_type = CHAIN_TOP_RIGHT_DOWN)
  {

    panelResX = _panelResX;
    panelResY = _panelResY;

    vmodule_rows = _vmodule_rows;
    vmodule_cols = _vmodule_cols;

    virtualResX = vmodule_cols * _panelResX;
    virtualResY = vmodule_rows * _panelResY;

    dmaResX = panelResX * vmodule_rows * vmodule_cols;
	
	panel_chain_type = _panel_chain_type;

    /* Virtual Display width() and height() will return a real-world value. For example:
     * Virtual Display width: 128
     * Virtual Display height: 64
     *
     * So, not values that at 0 to X-1
     */

    coords.x = coords.y = -1; // By default use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
	
	switch (panel_chain_type) {
		case CHAIN_TOP_LEFT_DOWN:
			chain_type_str = "CHAIN_TOP_LEFT_DOWN";
			break;

		case CHAIN_TOP_RIGHT_DOWN:
			chain_type_str = "CHAIN_TOP_RIGHT_DOWN";
			break;
			
		case CHAIN_TOP_RIGHT_DOWN_ZZ:
			chain_type_str = "CHAIN_TOP_RIGHT_DOWN_ZZ";
			break;			
			
		case CHAIN_BOTTOM_RIGHT_UP:
			chain_type_str = "CHAIN_BOTTOM_RIGHT_UP";
			break;
			
		case CHAIN_BOTTOM_LEFT_UP:
			chain_type_str = "CHAIN_BOTTOM_LEFT_UP";
			break;	

		default:
			chain_type_str = "WTF!";
			break;					
	}
	std::cout << "\n\n***************************************************************************\n";	
	std::cout << "Chain type: " << chain_type_str << " ";
	std::printf("Testing chain of panels of %d rows, %d columns, %d px by %d px resolution. \n\n", vmodule_rows, vmodule_cols, panelResX, panelResX, panelResY);
	
	
  }

  // equivalent methods of the matrix library so it can be just swapped out.
  void drawPixel(int16_t x, int16_t y, int16_t expected_x, int16_t expected_y);

  std::string chain_type_str = "UNKNOWN";
  
  // Internal co-ord conversion function
  VirtualCoords getCoords_Dev(int16_t x, int16_t y);
  
  VirtualCoords getCoords_WorkingBaslineMarch2023(int16_t x, int16_t y);
  
  VirtualCoords coords;
  
	
private:

  int16_t virtualResX;
  int16_t virtualResY;

  int16_t vmodule_rows;
  int16_t vmodule_cols;

  int16_t panelResX;
  int16_t panelResY;

  int16_t dmaResX; // The width of the chain in pixels (as the DMA engine sees it)

  PANEL_CHAIN_TYPE 	panel_chain_type;    
  PANEL_SCAN_RATE 	panel_scan_rate    = NORMAL_TWO_SCAN;    
    
  bool _rotate = false;

}; // end Class header

#include "baseline.hpp"

/**
 * Development version for testing.
 */
inline VirtualCoords VirtualMatrixPanelTest::getCoords_Dev(int16_t virt_x, int16_t virt_y)
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
			
		case CHAIN_TOP_RIGHT_DOWN_ZZ:
		{
			//	Right side up. Starting from top left all the way down.
			//  Connected in a Zig Zag manner = some long ass cables being used potentially
		
			//Serial.printf("Condition 2, row %d ", row);
			coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
			coords.y =  virt_y % panelResY;				
			
		}
		
		case CHAIN_BOTTOM_RIGHT_UP_ZZ:
		{
			//	Right side up. Starting from top left all the way down.
			//  Connected in a Zig Zag manner = some long ass cables being used potentially
		
			//Serial.printf("Condition 2, row %d ", row);
			coords.x =  (row*virtualResX)+virt_x;
			coords.y =  virt_y % panelResY;				
			
		}
								
						
						

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

bool check(VirtualCoords expected, VirtualCoords result, int x = -1, int y = -1)
{

	  if ( result.x != expected.x || result.y != expected.y )
	  {
		  std::printf("Requested (%d, %d) -> expecting physical (%d, %d) got (%d, %d).", x, y, expected.x, expected.y, result.x, result.y);
		  std::cout << "\t *** FAIL ***\n ";
		  std::cout << "\n";

		  return false;
	  }
	  else
	  {
		  return true;
	  }	
}


main(int argc, char* argv[])
{
    std::cout << "Starting Testing...\n";
	
	std::list <PANEL_CHAIN_TYPE> chain_t_test_list { 	CHAIN_TOP_LEFT_DOWN,	CHAIN_TOP_RIGHT_DOWN,	CHAIN_BOTTOM_LEFT_UP, 	CHAIN_BOTTOM_RIGHT_UP };
	

	// Draw pixel at virtual position 70x, 70y = 
	// x, y  x, y
	
	// x == horizontal
	// y = vert :-)
	
	// 192 x 192 pixel virtual display
	int rows = 3;
	int cols = 3;
	int panel_width_x 	= 64;
	int panel_height_y	= 64;
	
	std::string panel_scan_type = "NORMAL_TWO_SCAN";
	
	for (auto chain_t : chain_t_test_list) {


			VirtualMatrixPanelTest test = VirtualMatrixPanelTest(rows,cols,panel_width_x,panel_height_y, chain_t);
			int pass_counter = 0;
			int fail_counter = 0;
			for (int16_t x = 0; x < panel_width_x*cols; x++)
			{
				for (int16_t y = 0; y < panel_height_y*rows; y++)
				{
					VirtualCoords expected = test.getCoords_WorkingBaslineMarch2023(x,y);
					VirtualCoords result   = test.getCoords_Dev(x,y);
					
					bool chk_result = check(expected, result, x, y);

					  if ( chk_result )
					  { 
						  fail_counter++;
					  }
					  else
					  {
						  pass_counter++;
					  }
				}
			}
			
			if ( fail_counter > 0) {
				std::printf("ERROR: %d tests failed.\n", fail_counter); 
			} else{
				std::printf("SUCCESS: %d coord tests passed.\n", pass_counter); 		
			}
			
	}	// end chain type test list
	
	
	std::cout << "Performing NON-SERPENTINE (ZIG ZAG) TEST";
	
	rows = 3;
	cols = 1;
	panel_width_x 	= 64;
	panel_height_y	= 64;	
	
	VirtualMatrixPanelTest test = VirtualMatrixPanelTest(rows,cols,panel_width_x,panel_height_y, CHAIN_TOP_RIGHT_DOWN_ZZ);
	
	// CHAIN_TOP_RIGHT_DOWN_ZZ test 1
	// (x,y)
	VirtualCoords result   = test.getCoords_Dev(0,0);	
	VirtualCoords expected; expected.x = 64*2; expected.y = 0;
    std::printf("Expected physical (%d, %d) got (%d, %d).\n", expected.x, expected.y, result.x, result.y);

	// CHAIN_TOP_RIGHT_DOWN_ZZ test 2
	result   = test.getCoords_Dev(10,64*3-1);	
	expected.x = 10; expected.y = 63; 
    std::printf("Expected physical (%d, %d) got (%d, %d).\n", expected.x, expected.y, result.x, result.y);
		
		
	// CHAIN_TOP_RIGHT_DOWN_ZZ test 3
	result   = test.getCoords_Dev(16,64*2-1);	
	expected.x = 80; expected.y = 63; 
    std::printf("Expected physical (%d, %d) got (%d, %d).\n", expected.x, expected.y, result.x, result.y);
	
		
		
	// CHAIN_BOTTOM_RIGHT_UP_ZZ test 4
	result   = test.getCoords_Dev(0,0);	
	expected.x = 0; expected.y = 0; 
    std::printf("Expected physical (%d, %d) got (%d, %d).\n", expected.x, expected.y, result.x, result.y);	
			
	// CHAIN_BOTTOM_RIGHT_UP_ZZ test 4
	result   = test.getCoords_Dev(63,64);	
	expected.x = 64*2-1; expected.y = 0; 
    std::printf("Expected physical (%d, %d) got (%d, %d).\n", expected.x, expected.y, result.x, result.y);	
				
				
	
	std::cout << "\n\n";

    return 0;
}