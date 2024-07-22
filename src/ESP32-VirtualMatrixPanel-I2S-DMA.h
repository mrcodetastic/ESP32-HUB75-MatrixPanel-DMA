#ifndef _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA
#define _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA

/*******************************************************************
    Class contributed by Brian Lough, and expanded by Faptastic.

    Originally designed to allow CHAINING of panels together to create
    a 'bigger' display of panels. i.e. Chaining 4 panels into a 2x2
    grid.

    However, the function of this class has expanded now to also manage
    the output for

    1) TWO scan panels = Two rows updated in parallel.
        * 64px high panel =  sometimes referred to as 1/32 scan
        * 32px high panel =  sometimes referred to as 1/16 scan
        * 16px high panel =  sometimes referred to as 1/8 scan

    2) FOUR scan panels = Four rows updated in parallel
        * 32px high panel = sometimes referred to as 1/8 scan
        * 16px high panel = sometimes referred to as 1/4 scan

    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#ifdef USE_GFX_LITE
// Slimmed version of Adafruit GFX + FastLED: https://github.com/mrcodetastic/GFX_Lite
    #include "GFX_Lite.h"
    #include <Fonts/FreeSansBold12pt7b.h>
#elif !defined NO_GFX
    #include "Adafruit_GFX.h" // Adafruit class with all the other stuff
    #include <Fonts/FreeSansBold12pt7b.h>
#endif


// #include <iostream>

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
    NORMAL_TWO_SCAN,
    NORMAL_ONE_SIXTEEN, // treated as the same
    FOUR_SCAN_32PX_HIGH,
    FOUR_SCAN_16PX_HIGH,
    FOUR_SCAN_64PX_HIGH
};

// Chaining approach... From the perspective of the DISPLAY / LED side of the chain of panels.
enum PANEL_CHAIN_TYPE
{
    CHAIN_NONE,
    CHAIN_TOP_LEFT_DOWN,
    CHAIN_TOP_RIGHT_DOWN,
    CHAIN_BOTTOM_LEFT_UP,
    CHAIN_BOTTOM_RIGHT_UP,
    CHAIN_TOP_LEFT_DOWN_ZZ, /// ZigZag chaining. Might need a big ass cable to do this, all panels right way up.
    CHAIN_TOP_RIGHT_DOWN_ZZ,
    CHAIN_BOTTOM_RIGHT_UP_ZZ,
    CHAIN_BOTTOM_LEFT_UP_ZZ
};

#ifdef USE_GFX_LITE
class VirtualMatrixPanel : public GFX
#elif !defined NO_GFX
class VirtualMatrixPanel : public Adafruit_GFX
#else
class VirtualMatrixPanel
#endif
{

public:
    VirtualMatrixPanel(MatrixPanel_I2S_DMA &disp, int _vmodule_rows, int _vmodule_cols, int _panelResX, int _panelResY, PANEL_CHAIN_TYPE _panel_chain_type = CHAIN_NONE)
#ifdef USE_GFX_LITE
        : GFX(_vmodule_cols * _panelResX, _vmodule_rows * _panelResY)
#elif !defined NO_GFX
        : Adafruit_GFX(_vmodule_cols * _panelResX, _vmodule_rows * _panelResY)
#endif
    {
        this->display = &disp;

        panel_chain_type = _panel_chain_type;

        panelResX = _panelResX;
        panelResY = _panelResY;

        vmodule_rows = _vmodule_rows;
        vmodule_cols = _vmodule_cols;

        virtualResX = vmodule_cols * _panelResX;
        virtualResY = vmodule_rows * _panelResY;

	_virtualResX = virtualResX;
	_virtualResY = virtualResY;
	    
        dmaResX = panelResX * vmodule_rows * vmodule_cols - 1;

        /* Virtual Display width() and height() will return a real-world value. For example:
         * Virtual Display width: 128
         * Virtual Display height: 64
         *
         * So, not values that at 0 to X-1
         */

        coords.x = coords.y = -1; // By default use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
    }

    // equivalent methods of the matrix library so it can be just swapped out.
    void drawPixel(int16_t x, int16_t y, uint16_t color);   // overwrite adafruit implementation
    void fillScreen(uint16_t color); 			// overwrite adafruit implementation
    void setRotation(uint8_t rotate); 				// overwrite adafruit implementation
    
	void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);
    void clearScreen() { display->clearScreen(); }
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

#ifdef USE_GFX_LITE
    // 24bpp FASTLED CRGB colour struct support
    void fillScreen(CRGB color);
    void drawPixel(int16_t x, int16_t y, CRGB color);
#endif

#ifdef NO_GFX
    inline int16_t width() const { return _virtualResX; }
    inline int16_t height() const { return _virtualResY; }
#endif

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b)
    {
        return display->color444(r, g, b);
    }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
    uint16_t color333(uint8_t r, uint8_t g, uint8_t b) { return display->color333(r, g, b); }

    void flipDMABuffer() { display->flipDMABuffer(); }
    void drawDisplayTest();

    void setPhysicalPanelScanRate(PANEL_SCAN_RATE rate);
	void setZoomFactor(int scale);

    virtual VirtualCoords getCoords(int16_t x, int16_t y);
    VirtualCoords coords;
    int16_t panelResX;
    int16_t panelResY;

private:
    MatrixPanel_I2S_DMA *display;

    PANEL_CHAIN_TYPE panel_chain_type;
    PANEL_SCAN_RATE panel_scan_rate = NORMAL_TWO_SCAN;

    int16_t virtualResX;	///< Display width as combination of panels
    int16_t virtualResY;	///< Display height as combination of panels


	int16_t _virtualResX;       ///< Display width as modified by current rotation
	int16_t _virtualResY;       ///< Display height as modified by current rotation	

    int16_t vmodule_rows;
    int16_t vmodule_cols;

    int16_t dmaResX; // The width of the chain in pixels (as the DMA engine sees it)

    int _rotate = 0;
	
	int _scale_factor = 0;

}; // end Class header

/**
 * Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
 * Updates the private class member variable 'coords', so no need to use the return value.
 * Not thread safe, but not a concern for ESP32 sketch anyway... I think.
 */
inline VirtualCoords VirtualMatrixPanel::getCoords(int16_t virt_x, int16_t virt_y)
{
	
#if !defined NO_GFX
	// I don't give any support if Adafruit GFX isn't being used.
	
    if (virt_x < 0 || virt_x >= _width || virt_y < 0 || virt_y >= _height) // _width and _height are defined in the adafruit constructor
    {                             // Co-ordinates go from 0 to X-1 remember! otherwise they are out of range!
        coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
        return coords;
    }
#else
	
    if (virt_x < 0 || virt_x >= _virtualResX || virt_y < 0 || virt_y >= _virtualResY) // _width and _height are defined in the adafruit constructor
    {                             // Co-ordinates go from 0 to X-1 remember! otherwise they are out of range!
        coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
        return coords;
    }
	
#endif
    
    // Do we want to rotate?
    switch (_rotate) {
      case 0: //no rotation, do nothing
      break;
      
      case (1): //90 degree rotation
      {
        int16_t temp_x = virt_x;
        virt_x = virt_y;
        virt_y = virtualResY - 1 - temp_x;
        break;
      }

      case (2): //180 rotation
      {
        virt_x = virtualResX - 1 - virt_x;
        virt_y = virtualResY - 1 - virt_y;
        break;
      }

      case (3): //270 rotation
      {
        int16_t temp_x = virt_x;
        virt_x = virtualResX - 1 - virt_y;
        virt_y = temp_x;
        break;
      }
    }

    int row = (virt_y / panelResY); // 0 indexed
    switch (panel_chain_type)
    {
        case (CHAIN_TOP_RIGHT_DOWN):
        {
            if ((row % 2) == 1)
            { // upside down panel

                // Serial.printf("Condition 1, row %d ", row);

                // reversed for the row
                coords.x = dmaResX - virt_x - (row * virtualResX);

                // y co-ord inverted within the panel
                coords.y = panelResY - 1 - (virt_y % panelResY);
            }
            else
            {
                // Serial.printf("Condition 2, row %d ", row);
                coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
                coords.y = (virt_y % panelResY);
            }
        }
        break;

        case (CHAIN_TOP_RIGHT_DOWN_ZZ):
        {
            //	Right side up. Starting from top right all the way down.
            //  Connected in a Zig Zag manner = some long ass cables being used potentially

            // Serial.printf("Condition 2, row %d ", row);
            coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
            coords.y = (virt_y % panelResY);
        }
        break;

        case (CHAIN_TOP_LEFT_DOWN): // OK -> modulus opposite of CHAIN_TOP_RIGHT_DOWN
        {
            if ((row % 2) == 0)
            { // reversed panel

                // Serial.printf("Condition 1, row %d ", row);
                coords.x = dmaResX - virt_x - (row * virtualResX);

                // y co-ord inverted within the panel
                coords.y = panelResY - 1 - (virt_y % panelResY);
            }
            else
            {
                // Serial.printf("Condition 2, row %d ", row);
                coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
                coords.y = (virt_y % panelResY);
            }
        }
        break;

        case (CHAIN_TOP_LEFT_DOWN_ZZ):
        {
            // Serial.printf("Condition 2, row %d ", row);
            coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
            coords.y = (virt_y % panelResY);
        }
        break;

        case (CHAIN_BOTTOM_LEFT_UP): //
        {
            row = vmodule_rows - row - 1;

            if ((row % 2) == 1)
            {
                // Serial.printf("Condition 1, row %d ", row);
                coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
                coords.y = (virt_y % panelResY);
            }
            else
            { // inverted panel

                // Serial.printf("Condition 2, row %d ", row);
                coords.x = dmaResX - (row * virtualResX) - virt_x;
                coords.y = panelResY - 1 - (virt_y % panelResY);
            }
        }
        break;

        case (CHAIN_BOTTOM_LEFT_UP_ZZ): //
        {
            row = vmodule_rows - row - 1;
            // Serial.printf("Condition 1, row %d ", row);
            coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
            coords.y = (virt_y % panelResY);
        }
        break;

        case (CHAIN_BOTTOM_RIGHT_UP): // OK -> modulus opposite of CHAIN_BOTTOM_LEFT_UP
        {
            row = vmodule_rows - row - 1;

            if ((row % 2) == 0)
            { // right side up

                // Serial.printf("Condition 1, row %d ", row);
                // refersed for the row
                coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
                coords.y = (virt_y % panelResY);
            }
            else
            { // inverted panel

                // Serial.printf("Condition 2, row %d ", row);
                coords.x = dmaResX - (row * virtualResX) - virt_x;
                coords.y = panelResY - 1 - (virt_y % panelResY);
            }
        }
        break;

        case (CHAIN_BOTTOM_RIGHT_UP_ZZ):
        {
            //	Right side up. Starting bottom right all the way up.
            //  Connected in a Zig Zag manner = some long ass cables being used potentially

            row = vmodule_rows - row - 1;
            // Serial.printf("Condition 2, row %d ", row);
            coords.x = ((vmodule_rows - (row + 1)) * virtualResX) + virt_x;
            coords.y = (virt_y % panelResY);
        }
        break;

	    // Q: 1 row!? Why?
        // A: In cases people are only using virtual matrix panel for panels of non-standard scan rates.
        default: 
            coords.x = virt_x; coords.y = virt_y; 
            break;
            
    } // end switch
	

    /* START: Pixel remapping AGAIN to convert TWO parallel scanline output that the
     *        the underlying hardware library is designed for (because
     *        there's only 2 x RGB pins... and convert this to 1/4 or something
     */	
	
    if ((panel_scan_rate == FOUR_SCAN_32PX_HIGH) || (panel_scan_rate == FOUR_SCAN_64PX_HIGH))
    {

	if (panel_scan_rate == FOUR_SCAN_64PX_HIGH)
	{
	    // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/345#issuecomment-1510401192
	    if ((virt_y & 8) != ((virt_y & 16) >> 1)) { virt_y = (virt_y & 0b11000) ^ 0b11000 + (virt_y & 0b11100111); }
	}


        /* Convert Real World 'VirtualMatrixPanel' co-ordinates (i.e. Real World pixel you're looking at
           on the panel or chain of panels, per the chaining configuration) to a 1/8 panels
           double 'stretched' and 'squished' coordinates which is what needs to be sent from the
           DMA buffer.

           Note: Look at the FourScanPanel example code and you'll see that the DMA buffer is setup
           as if the panel is 2 * W and 0.5 * H !
        */

        if ((coords.y & 8) == 0)
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
        if ((coords.y & 4) == 0)
        {
            coords.x += ((coords.x / panelResX) + 1) * panelResX; // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
        }
        else
        {
            coords.x += (coords.x / panelResX) * panelResX; // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
        }
        coords.y = (coords.y >> 3) * 4 + (coords.y & 0b00000011);
    }

    return coords;
}

inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{ // adafruit virtual void override

	if (_scale_factor > 1) // only from 2 and beyond
	{
		int16_t scaled_x_start_pos = x * _scale_factor;
		int16_t scaled_y_start_pos = y * _scale_factor;
		
		for (int16_t x = 0; x < _scale_factor; x++) {
			for (int16_t y = 0; y < _scale_factor; y++) {	
				VirtualCoords result = this->getCoords(scaled_x_start_pos+x, scaled_y_start_pos+y);
				// Serial.printf("Requested virtual x,y coord (%d, %d), got phyical chain coord of (%d,%d)\n", x,y, coords.x, coords.y);
				this->display->drawPixel(result.x, result.y, color);
			}
		}
	}
	else
	{
		this->getCoords(x, y);
		// Serial.printf("Requested virtual x,y coord (%d, %d), got phyical chain coord of (%d,%d)\n", x,y, coords.x, coords.y);
		this->display->drawPixel(coords.x, coords.y, color);	
	}
}

inline void VirtualMatrixPanel::fillScreen(uint16_t color)
{ // adafruit virtual void override
    this->display->fillScreen(color);
}

inline void VirtualMatrixPanel::fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
{
    this->display->fillScreenRGB888(r, g, b);
}

inline void VirtualMatrixPanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    this->getCoords(x, y);
    this->display->drawPixelRGB888(coords.x, coords.y, r, g, b);
}

#ifdef USE_GFX_LITE
// Support for CRGB values provided via FastLED
inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, CRGB color)
{
    this->getCoords(x, y);
    this->display->drawPixel(coords.x, coords.y, color);
}

inline void VirtualMatrixPanel::fillScreen(CRGB color)
{
    this->display->fillScreen(color);
}
#endif

inline void VirtualMatrixPanel::setRotation(uint8_t rotate)
{
  if(rotate < 4 && rotate >= 0)
    _rotate = rotate;

  // Change the _width and _height variables used by the underlying adafruit gfx library.
  // Actual pixel rotation / mapping is done in the getCoords function.
#ifdef NO_GFX
    int8_t rotation;
#endif
  rotation = (rotate & 3);
  switch (rotation) {
  case 0: // nothing
  case 2: // 180
	_virtualResX = virtualResX;
	_virtualResY = virtualResY;

#if !defined NO_GFX	
    _width = virtualResX; // adafruit base class 
    _height = virtualResY; // adafruit base class 
#endif 
    break;
  case 1:
  case 3:
	_virtualResX = virtualResY;
	_virtualResY = virtualResX;
	
#if !defined NO_GFX		
    _width = virtualResY; // adafruit base class 
    _height = virtualResX; // adafruit base class 
#endif 	
    break;
  }

  
}

inline void VirtualMatrixPanel::setPhysicalPanelScanRate(PANEL_SCAN_RATE rate)
{
    panel_scan_rate = rate;
}

inline void VirtualMatrixPanel::setZoomFactor(int scale)
{
  if(scale < 5 && scale > 0)
	_scale_factor = scale;

}

#ifndef NO_GFX
inline void VirtualMatrixPanel::drawDisplayTest()
{
	// Write to the underlying panels only via the dma_display instance.
    this->display->setFont(&FreeSansBold12pt7b);
    this->display->setTextColor(this->display->color565(255, 255, 0));
    this->display->setTextSize(1);

    for (int panel = 0; panel < vmodule_cols * vmodule_rows; panel++)
    {
        int top_left_x = (panel == 0) ? 0 : (panel * panelResX);
        this->display->drawRect(top_left_x, 0, panelResX, panelResY, this->display->color565(0, 255, 0));
        this->display->setCursor((panel * panelResX) + 2, panelResY - 4);
        this->display->print((vmodule_cols * vmodule_rows) - panel);
    }
}
#endif

/*
// need to recreate this one, as it wouldn't work to just map where it starts.
inline void VirtualMatrixPanel::drawIcon (int *ico, int16_t x, int16_t y, int16_t icon_cols, int16_t icon_rows) {
  int i, j;
  for (i = 0; i < icon_rows; i++) {
    for (j = 0; j < icon_cols; j++) {
      // This is a call to this libraries version of drawPixel
      // which will map each pixel, which is what we want.
      //drawPixelRGB565 (x + j, y + i, ico[i * module_cols + j]);
    drawPixel (x + j, y + i, ico[i * icon_cols + j]);
    }
  }
}
*/

#endif
