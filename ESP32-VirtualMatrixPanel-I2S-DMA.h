#ifndef _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA
#define _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA

/*******************************************************************
    Class contributed by Brian Lough, and expanded by Faptastic.

    Originally designed to allow CHAINING of panels together to create
    a 'bigger' display of panels. i.e. Chaining 4 panels into a 2x2
    grid.

    However, the function of this class has expanded now to also manage
    the output for 1/16 scan panels, as the core DMA library is designed
    ONLY FOR 1/16 scan matrix panels.

    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#ifndef NO_GFX
#include <Fonts/FreeSansBold12pt7b.h>
#endif

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
  NORMAL_ONE_SIXTEEN,
  ONE_EIGHT_32,
  ONE_EIGHT_16
};

#ifdef USE_GFX_ROOT
class VirtualMatrixPanel : public GFX
#elif !defined NO_GFX
class VirtualMatrixPanel : public Adafruit_GFX
#else
class VirtualMatrixPanel
#endif
{

public:
  int16_t virtualResX;
  int16_t virtualResY;

  int16_t vmodule_rows;
  int16_t vmodule_cols;

  int16_t panelResX;
  int16_t panelResY;

  int16_t dmaResX; // The width of the chain in pixels (as the DMA engine sees it)

  MatrixPanel_I2S_DMA *display;

  VirtualMatrixPanel(MatrixPanel_I2S_DMA &disp, int _vmodule_rows, int _vmodule_cols, int _panelResX, int _panelResY, bool serpentine_chain = true, bool top_down_chain = false)
#ifdef USE_GFX_ROOT
      : GFX(_vmodule_cols * _panelResX, _vmodule_rows * _panelResY)
#elif !defined NO_GFX
      : Adafruit_GFX(_vmodule_cols * _panelResX, _vmodule_rows * _panelResY)
#endif
  {
    this->display = &disp;

    panelResX = _panelResX;
    panelResY = _panelResY;

    vmodule_rows = _vmodule_rows;
    vmodule_cols = _vmodule_cols;

    virtualResX = vmodule_cols * _panelResX;
    virtualResY = vmodule_rows * _panelResY;

    dmaResX = panelResX * vmodule_rows * vmodule_cols;

    /* Virtual Display width() and height() will return a real-world value. For example:
     * Virtual Display width: 128
     * Virtual Display height: 64
     *
     * So, not values that at 0 to X-1
     */

    _s_chain_party = serpentine_chain; // serpentine, or 'S' chain?
    _chain_top_down = top_down_chain;

    coords.x = coords.y = -1; // By default use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
  }

  // equivalent methods of the matrix library so it can be just swapped out.
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
  virtual void fillScreen(uint16_t color); // overwrite adafruit implementation
  virtual void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);

  void clearScreen() { display->clearScreen(); }
  void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

#ifdef USE_GFX_ROOT
  // 24bpp FASTLED CRGB colour struct support
  void fillScreen(CRGB color);
  void drawPixel(int16_t x, int16_t y, CRGB color);
#endif

  uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return display->color444(r, g, b); }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
  uint16_t color333(uint8_t r, uint8_t g, uint8_t b) { return display->color333(r, g, b); }

  void flipDMABuffer() { display->flipDMABuffer(); }
  void drawDisplayTest();
  void setRotate(bool rotate);

  void setPhysicalPanelScanRate(PANEL_SCAN_RATE rate);

protected:
  virtual VirtualCoords getCoords(int16_t &x, int16_t &y);
  VirtualCoords coords;

  bool _s_chain_party = true;   // Are we chained? Ain't no party like a...
  bool _chain_top_down = false; // is the ESP at the top or bottom of the matrix of devices?
  bool _rotate = false;

  PANEL_SCAN_RATE _panelScanRate = NORMAL_ONE_SIXTEEN;

}; // end Class header

/**
 * Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
 * Updates the private class member variable 'coords', so no need to use the return value.
 * Not thread safe, but not a concern for ESP32 sketch anyway... I think.
 */
inline VirtualCoords VirtualMatrixPanel::getCoords(int16_t &x, int16_t &y)
{
  // Serial.println("Called Base.");
  coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer

  // Do we want to rotate?
  if (_rotate)
  {
    int16_t temp_x = x;
    x = y;
    y = virtualResY - 1 - temp_x;
  }

  if (x < 0 || x >= virtualResX || y < 0 || y >= virtualResY)
  { // Co-ordinates go from 0 to X-1 remember! otherwise they are out of range!
    // Serial.printf("VirtualMatrixPanel::getCoords(): Invalid virtual display coordinate. x,y: %d, %d\r\n", x, y);
    return coords;
  }

  // Stupidity check
  if ((vmodule_rows == 1) && (vmodule_cols == 1)) // single panel...
  {
    coords.x = x;
    coords.y = y;
  }
  else
  {
    uint8_t row = (y / panelResY) + 1;                         // a non indexed 0 row number
    if ((_s_chain_party && !_chain_top_down && (row % 2 == 0)) // serpentine vertically stacked chain starting from bottom row (i.e. ESP closest to ground), upwards
        ||
        (_s_chain_party && _chain_top_down && (row % 2 != 0)) // serpentine vertically stacked chain starting from the sky downwards
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
      coords.x = x + ((y / panelResY) * (virtualResX));
      coords.y = y % panelResY;
    }
  }

  // Reverse co-ordinates if panel chain from ESP starts from the TOP RIGHT
  if (_chain_top_down)
  {
    /*
    const HUB75_I2S_CFG _cfg = this->display->getCfg();
    coords.x = (_cfg.mx_width * _cfg.chain_length - 1) - coords.x;
    coords.y = (_cfg.mx_height-1) - coords.y;
    */
    coords.x = (dmaResX - 1) - coords.x;
    coords.y = (panelResY - 1) - coords.y;
  }

  /* START: Pixel remapping AGAIN to convert 1/16 SCAN output that the
   *        the underlying hardware library is designed for (because
   *        there's only 2 x RGB pins... and convert this to 1/8 or something
   */
  if (_panelScanRate == ONE_EIGHT_32)
  {
    /* Convert Real World 'VirtualMatrixPanel' co-ordinates (i.e. Real World pixel you're looking at
       on the panel or chain of panels, per the chaining configuration) to a 1/8 panels
       double 'stretched' and 'squished' coordinates which is what needs to be sent from the
       DMA buffer.

       Note: Look at the One_Eight_1_8_ScanPanel code and you'll see that the DMA buffer is setup
       as if the panel is 2 * W and 0.5 * H !
    */

    /*
      Serial.print("VirtualMatrixPanel Mapping ("); Serial.print(x, DEC); Serial.print(","); Serial.print(y, DEC); Serial.print(") ");
      // to
      Serial.print("to ("); Serial.print(coords.x, DEC);  Serial.print(",");  Serial.print(coords.y, DEC);   Serial.println(") ");
     */
    if ((y & 8) == 0)
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
    coords.y = (y >> 4) * 8 + (y & 0b00000111);

    /*
     Serial.print("OneEightScanPanel Mapping ("); Serial.print(x, DEC); Serial.print(","); Serial.print(y, DEC); Serial.print(") ");
     // to
     Serial.print("to ("); Serial.print(coords.x, DEC);  Serial.print(",");  Serial.print(coords.y, DEC);   Serial.println(") ");
    */
  }
  else if (_panelScanRate == ONE_EIGHT_16)
  {
    if ((y & 8) == 0)
    {
      coords.x += (panelResX >> 2) * (((coords.x & 0xFFF0) >> 4) + 1); // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }
    else
    {
      coords.x += (panelResX >> 2) * (((coords.x & 0xFFF0) >> 4)); // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }

    if (y < 32)
      coords.y = (y >> 4) * 8 + (y & 0b00000111);
    else
    {
      coords.y = ((y - 32) >> 4) * 8 + (y & 0b00000111);
      coords.x += 256;
    }
  }

  // Serial.print("Mapping to x: "); Serial.print(coords.x, DEC);  Serial.print(", y: "); Serial.println(coords.y, DEC);
  return coords;
}

inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{ // adafruit virtual void override
  getCoords(x, y);
  this->display->drawPixel(coords.x, coords.y, color);
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
  getCoords(x, y);
  this->display->drawPixelRGB888(coords.x, coords.y, r, g, b);
}

#ifdef USE_GFX_ROOT
// Support for CRGB values provided via FastLED
inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, CRGB color)
{
  getCoords(x, y);
  this->display->drawPixel(coords.x, coords.y, color);
}

inline void VirtualMatrixPanel::fillScreen(CRGB color)
{
  this->display->fillScreen(color);
}
#endif

inline void VirtualMatrixPanel::setRotate(bool rotate)
{
  _rotate = rotate;

#ifndef NO_GFX
  // We don't support rotation by degrees.
  if (rotate)
  {
    setRotation(1);
  }
  else
  {
    setRotation(0);
  }
#endif
}

inline void VirtualMatrixPanel::setPhysicalPanelScanRate(PANEL_SCAN_RATE rate)
{
  _panelScanRate = rate;
}

#ifndef NO_GFX
inline void VirtualMatrixPanel::drawDisplayTest()
{
  this->display->setFont(&FreeSansBold12pt7b);
  this->display->setTextColor(this->display->color565(255, 255, 0));
  this->display->setTextSize(1);

  for (int panel = 0; panel < vmodule_cols * vmodule_rows; panel++)
  {
    int top_left_x = (panel == 0) ? 0 : (panel * panelResX);
    this->display->drawRect(top_left_x, 0, panelResX, panelResY, this->display->color565(0, 255, 0));
    this->display->setCursor(panel * panelResX, panelResY - 3);
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
