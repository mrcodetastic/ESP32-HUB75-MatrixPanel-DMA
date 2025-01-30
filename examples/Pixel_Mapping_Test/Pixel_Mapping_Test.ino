/*************************************************************************
   Description:

   The underlying implementation of the ESP32-HUB75-MatrixPanel-I2S-DMA only
   supports output to HALF scan panels - which means outputting
   two lines at the same time, 16 or 32 rows apart if a 32px or 64px high panel
   respectively.
   This cannot be changed at the DMA layer as it would require a messy and complex
   rebuild of the library's internals.

   However, it is possible to connect QUARTER (i.e. FOUR lines updated in parallel)
   scan panels to this same library and
   'trick' the output to work correctly on these panels by way of adjusting the
   pixel co-ordinates that are 'sent' to the ESP32-HUB75-MatrixPanel-I2S-DMA
   library.

 **************************************************************************/

/* Use the Virtual Display class to re-map co-ordinates such that they draw
   correctly on a 32x16 1/4 or 64x32 1/8 Scan panel (or chain of such panels).
*/
#include "ESP32-VirtualMatrixPanel-I2S-DMA.h"

// Define custom class derived from VirtualMatrixPanel
class CustomPxBasePanel : public VirtualMatrixPanel
{
  public:
    using VirtualMatrixPanel::VirtualMatrixPanel; // inherit VirtualMatrixPanel's constructor(s)

  protected:

    VirtualCoords getCoords(int16_t x, int16_t y);  // custom getCoords() method for specific pixel mapping

};

// custom getCoords() method for specific pixel mapping
inline VirtualCoords CustomPxBasePanel ::getCoords(int16_t x, int16_t y) {

  coords = VirtualMatrixPanel::getCoords(x, y); // call base class method to update coords for chaining approach

  if ( coords.x == -1 || coords.y == -1 ) { // Co-ordinates go from 0 to X-1 remember! width() and height() are out of range!
    return coords;
  }

  uint8_t pxbase = panelResX;  // pixel base
  // mapper for panels with 32 pixs height  (64x32 or 32x32)
  if (panelResY == 32)
  {
    if ((coords.y & 8) == 0)
    {
      coords.x += ((coords.x / pxbase) + 1) * pxbase;    // 1st, 3rd 'block' of 8 rows of pixels
    }
    else
    {
      coords.x += (coords.x / pxbase) * pxbase;          // 2nd, 4th 'block' of 8 rows of pixels
    }
    coords.y = (coords.y >> 4) * 8 + (coords.y & 0b00000111);
  }

  // mapper for panels with 16 pixs height  (32x16 1/4)
  else if (panelResY == 16)
  {
    if ((coords.y & 4) == 0)
    {
      // 1. Normal line, from left to right
      coords.x += ((coords.x / pxbase) + 1) * pxbase;     // 1st, 3rd 'block' of 4 rows of pixels
      //2. in case the line filled from right to left, use this (and comment 1st)
      //coords.x = ((coords.x / pxbase) + 1) * 2 * pxbase  - (coords.x % pxbase) - 1;
    }
    else
    {
      coords.x += (coords.x / pxbase) * pxbase;          // 2nd, 4th 'block' of 4 rows of pixels
    }
    coords.y = (coords.y >> 3) * 4 + (coords.y & 0b00000011);
  }
  return coords;
}

// Panel configuration
#define PANEL_RES_X 32 // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 16 // Number of pixels tall of each INDIVIDUAL panel module.

// Use a single panel for tests
#define NUM_ROWS 1 // Number of rows of chained INDIVIDUAL PANELS
#define NUM_COLS 1 // Number of INDIVIDUAL PANELS per ROW

// Chain settings, do not cnahge
#define SERPENT true
#define TOPDOWN false
#define VIRTUAL_MATRIX_CHAIN_TYPE CHAIN_BOTTOM_RIGHT_UP

// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;

// placeholder for the virtual display object
CustomPxBasePanel   *FourScanPanel = nullptr;

/******************************************************************************
   Setup!
 ******************************************************************************/
void setup()
{
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X * 2,            // DO NOT CHANGE THIS
    PANEL_RES_Y / 2,            // DO NOT CHANGE THIS
    NUM_ROWS * NUM_COLS         // DO NOT CHANGE THIS
    //,_pins            // Uncomment to enable custom pins
  );

  mxconfig.clkphase = false; // Change this if you see pixels showing up shifted wrongly by one column the left or right.

  // OK, now we can create our matrix object
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 75%
  dma_display->setBrightness8(40);    // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if ( not dma_display->begin() )
    Serial.println("****** !KABOOM! I2S memory allocation failed ***********");


  dma_display->clearScreen();
  delay(500);

  // create FourScanPanellay object based on our newly created dma_display object
  FourScanPanel = new CustomPxBasePanel ((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y,  VIRTUAL_MATRIX_CHAIN_TYPE);

}


void loop() {
  for (int i = 0; i < FourScanPanel->height(); i++)
  {
    for (int j = 0; j < FourScanPanel->width(); j++)
    {
      FourScanPanel->drawPixel(j, i, FourScanPanel->color565(255, 0, 0));
      delay(30);
    }
  }
  delay(2000);
  dma_display->clearScreen();
} // end loop