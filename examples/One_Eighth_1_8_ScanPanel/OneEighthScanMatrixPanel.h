#ifndef _ESP32_ONE_EIGTH_MATRIX_PANEL_I2S_DMA
#define _ESP32_ONE_EIGTH_MATRIX_PANEL_I2S_DMA

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

struct VirtualCoords {
  int16_t x;
  int16_t y;
};

 
#ifdef USE_GFX_ROOT
class OneEighthMatrixPanel : public GFX
#elif !defined NO_GFX
class OneEighthMatrixPanel : public Adafruit_GFX
#else
class OneEighthMatrixPanel
#endif	
{

  public:
    int16_t virtualResX;
    int16_t virtualResY;

    int16_t vmodule_rows;
    int16_t vmodule_cols;

    int16_t panelResX;
    int16_t panelResY;

    MatrixPanel_I2S_DMA *display;

    OneEighthMatrixPanel(MatrixPanel_I2S_DMA &disp, int _vmodule_rows, int _vmodule_cols, int _panelResX, int _panelResY, bool serpentine_chain = true, bool top_down_chain = false)
#ifdef USE_GFX_ROOT
      : GFX(_vmodule_cols*_panelResX, _vmodule_rows*_panelResY)
#elif !defined NO_GFX
      : Adafruit_GFX(_vmodule_cols*_panelResX, _vmodule_rows*_panelResY)
#endif            
    {
      this->display = &disp;

      panelResX = _panelResX;
      panelResY = _panelResY;

      vmodule_rows = _vmodule_rows;
      vmodule_cols = _vmodule_cols;

      virtualResX = vmodule_cols*_panelResX;      
      virtualResY = vmodule_rows*_panelResY;	  
	  
	  /* Virtual Display width() and height() will return a real-world value. For example:
	   * Virtual Display width: 128
	   * Virtual Display height: 64
	   *
	   * So, not values that at 0 to X-1
	   */
      _s_chain_party = serpentine_chain; // serpentine, or 'S' chain?
      _chain_top_down= top_down_chain;
	  
	  coords.x = coords.y = -1; // By default use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer

    }

    VirtualCoords getCoords(int16_t x, int16_t y);

    // equivalent methods of the matrix library so it can be just swapped out.
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color); // overwrite adafruit implementation
    void clearScreen() {
      display->clearScreen();
    }
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void drawIcon (int *ico, int16_t x, int16_t y, int16_t icon_cols, int16_t icon_rows);

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) {
      return display->color444(r, g, b);
    }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
      return display->color565(r, g, b);
    }
    uint16_t color333(uint8_t r, uint8_t g, uint8_t b) {
      return display->color333(r, g, b);
    }
	
	void flipDMABuffer() { display->flipDMABuffer(); }
	
    // Rotate display
    inline void setRotate(bool rotate);	

  private:
    VirtualCoords coords;
    bool _s_chain_party  = true; // Are we chained? Ain't no party like a... 
    bool _chain_top_down = false; // is the ESP at the top or bottom of the matrix of devices?
	bool _rotate 		 = false;

}; // end Class header

/**
 * Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
 * Then do further calculations for 1/8 scan panel.
 * Updates the private class member variable 'coords', so no need to use the return value. 
 * Not thread safe, but not a concern for ESP32 sketch anyway... I think.
 */
inline VirtualCoords OneEighthMatrixPanel::getCoords(int16_t x, int16_t y) {

  coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer

  // Check if virtual work co-ordinates are outside the virtual display resolution space. This does NOT check
  // against the physical real-world DMA matrix resolution / setup configured, that is used to actually output
  // the electrical pulse to the panel.
  
  if (x < 0 || x >= width() || y < 0 || y >= height() ) { // Co-ordinates go from 0 to X-1 remember! width() and height() are out of range!
    //Serial.printf("OneEighthMatrixPanel::getCoords(): Invalid virtual display coordinate. x,y: %d, %d\r\n", x, y);
    return coords;
  }
  
	 // We want to rotate?
	 if (_rotate){
		uint16_t temp_x=x;
		x=y;
		y=virtualResY-1-temp_x;
   }  

    uint8_t row = (y / panelResY) + 1; //a non indexed 0 row number
    uint8_t col = (x / panelResX) + 1; //a non indexed 0 row number    
    if(   ( _s_chain_party && !_chain_top_down && (row % 2 == 0) )  // serpentine vertically stacked chain starting from bottom row (i.e. ESP closest to ground), upwards
          ||
          ( _s_chain_party && _chain_top_down  && (row % 2 != 0) )  // serpentine vertically stacked chain starting from the sky downwards
    )
    {
      // First portion gets you to the correct offset for the row you need
      // Second portion inverts the x on the row
	    coords.x = ((y / panelResY) * (virtualResX)) + (virtualResX - x) - 1;
	  
      // inverts the y the row
      coords.y = panelResY - 1 - (y % panelResY);  
    } 
    else 
    {
	  // Normal chain pixel co-ordinate
      coords.x = x + ((y / panelResY) * (virtualResX)) ;
      coords.y = y % panelResY;
    }
	
	/* *******
	 * START: 1/8 Scan Panel Pixel Re-Mapping
	 *
	 * We have calculated the x, y co-ordinates as if we have a chain of standard panels this library is designed 
	 * for, this being 1/8 or 1/16 scan panels. We have to do some further hacking to convert co-ords to the 
	 * double length and 1/2 physical dma output length that is required for these panels to work electronically.
	 */

   // 1/8 Scan Panel - Is the final x-coord on the 1st or 3rd, 1/4ths (8 pixel 'blocks') of the panel (i.e. Row 0-7 or 17-24) ?
   // Double the length of the x-coord if required
   if ( ((coords.y /8) % 2) == 0) { // returns true/1 for the 1st and 3rd 8-pixel 1/4th of a 32px high panel
        coords.x += (panelResX*col);
   }

   // Half the y coord.
   coords.y = (y % 8);   
   if ( y >= panelResY/2 ) coords.y +=8;
           
  /* 
   * END: 1/8 Scan Panel Pixel Re-Mapping
   * *******
   */

  // Reverse co-ordinates if panel chain from ESP starts from the TOP RIGHT
  /*
  if (_chain_top_down)
  {
    coords.x = (panelResX * vmodule_rows * vmodule_cols - 1) - coords.x;
    coords.y = (panelResY-1) - coords.y;
	
  } 
  */  
	
	return coords;  

}

inline void OneEighthMatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  //VirtualCoords coords = getCoords(x, y);	
  getCoords(x, y);
  this->display->drawPixel(coords.x, coords.y, color);
}

inline void OneEighthMatrixPanel::fillScreen(uint16_t color)  // adafruit virtual void override
{
  // No need to map this.
  this->display->fillScreen(color);
}

inline void OneEighthMatrixPanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  //VirtualCoords coords = getCoords(x, y);	
  getCoords(x, y);
  this->display->drawPixelRGB888( coords.x, coords.y, r, g, b);
}

inline void OneEighthMatrixPanel::setRotate(bool rotate) {
	_rotate=rotate;	
	
	// We don't support rotation by degrees.
	if (rotate) { setRotation(1); } else { setRotation(0); }
}

// need to recreate this one, as it wouldnt work to just map where it starts.
inline void OneEighthMatrixPanel::drawIcon (int *ico, int16_t x, int16_t y, int16_t icon_cols, int16_t icon_rows) {
	
}

#endif

/*
// http://cpp.sh/6skpy


// Example program
#include <iostream>
#include <string>

int main()
{
  for (int i = 0; i < 32; i++)
  {
       int x = 0;
       int y = i;
       if ( ((y /8) % 2) == 0) { // returns true/1 for the 1st and 3rd 8-pixel 1/4th of a 32px high panel
            x += 64;
       }
       
  
       y = (i % 8);
       if ( i >= 32/2 ) y +=8;
       
       std::cout << "For input y =  " << i << " mapping to y: " << y << " and x " << x << " \n";
         
  }
  
  
}


For input y =  0 mapping to y: 0 and x 64 
For input y =  1 mapping to y: 1 and x 64 
For input y =  2 mapping to y: 2 and x 64 
For input y =  3 mapping to y: 3 and x 64 
For input y =  4 mapping to y: 4 and x 64 
For input y =  5 mapping to y: 5 and x 64 
For input y =  6 mapping to y: 6 and x 64 
For input y =  7 mapping to y: 7 and x 64 
For input y =  8 mapping to y: 0 and x 0 
For input y =  9 mapping to y: 1 and x 0 
For input y =  10 mapping to y: 2 and x 0 
For input y =  11 mapping to y: 3 and x 0 
For input y =  12 mapping to y: 4 and x 0 
For input y =  13 mapping to y: 5 and x 0 
For input y =  14 mapping to y: 6 and x 0 
For input y =  15 mapping to y: 7 and x 0 
For input y =  16 mapping to y: 8 and x 64 
For input y =  17 mapping to y: 9 and x 64 
For input y =  18 mapping to y: 10 and x 64 
For input y =  19 mapping to y: 11 and x 64 
For input y =  20 mapping to y: 12 and x 64 
For input y =  21 mapping to y: 13 and x 64 
For input y =  22 mapping to y: 14 and x 64 
For input y =  23 mapping to y: 15 and x 64 
For input y =  24 mapping to y: 8 and x 0 
For input y =  25 mapping to y: 9 and x 0 
For input y =  26 mapping to y: 10 and x 0 
For input y =  27 mapping to y: 11 and x 0 
For input y =  28 mapping to y: 12 and x 0 
For input y =  29 mapping to y: 13 and x 0 
For input y =  30 mapping to y: 14 and x 0 
For input y =  31 mapping to y: 15 and x 0 

*/