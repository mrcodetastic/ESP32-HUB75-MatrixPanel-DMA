#ifndef _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA
#define _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA

/*******************************************************************
    Contributed by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

#include "ESP32-RGB64x32MatrixPanel-I2S-DMA.h"

struct VirtualCoords {
  int16_t x;
  int16_t y;
};

 
#ifdef USE_GFX_ROOT
class VirtualMatrixPanel : public GFX
#else	
class VirtualMatrixPanel : public Adafruit_GFX
#endif	
{

  public:
    int16_t virtualResX;
    int16_t virtualResY;

    int16_t module_rows;
    int16_t module_cols;

    int16_t screenResX;
    int16_t screenResY;

    RGB64x32MatrixPanel_I2S_DMA *display;

#ifdef USE_GFX_ROOT
    VirtualMatrixPanel(RGB64x32MatrixPanel_I2S_DMA &disp, int vmodule_rows, int vmodule_cols, int screenX, int screenY)
      : GFX(vmodule_cols*screenX, vmodule_rows*screenY)
#else
    VirtualMatrixPanel(RGB64x32MatrixPanel_I2S_DMA &disp, int vmodule_rows, int vmodule_cols, int screenX, int screenY)
      : Adafruit_GFX(vmodule_cols*screenX, vmodule_rows*screenY)
#endif            
    {
      this->display = &disp;
      module_rows = vmodule_rows;
      module_cols = vmodule_cols;
      screenResX = screenX;
      screenResY = screenY;

      virtualResX = vmodule_rows*screenY;
      virtualResY = vmodule_cols*screenX;
    }

    VirtualCoords getCoords(int16_t x, int16_t y);

    // equivalent methods of the matrix library so it can be just swapped out.
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color);                        // overwrite adafruit implementation
    void clearScreen() {
      fillScreen(0);
    }
    void drawPixelRGB565(int16_t x, int16_t y, uint16_t color);
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void drawPixelRGB24(int16_t x, int16_t y, rgb_24 color);
    void drawIcon (int *ico, int16_t x, int16_t y, int16_t module_cols, int16_t module_rows);

    uint16_t color444(uint8_t r, uint8_t g, uint8_t b) {
      return display->color444(r, g, b);
    }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
      return display->color565(r, g, b);
    }
    uint16_t Color333(uint8_t r, uint8_t g, uint8_t b) {
      return display->Color333(r, g, b);
    }

  private:
    VirtualCoords coords;

}; // end Class header

inline VirtualCoords VirtualMatrixPanel::getCoords(int16_t x, int16_t y) {
  int16_t xOffset = (y / screenResY) * (module_cols * screenResX);
  coords.x = x + xOffset;
  coords.y = y % screenResY;
  return coords;
}

inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixel(coords.x, coords.y, color);
}

inline void VirtualMatrixPanel::fillScreen(uint16_t color)  // adafruit virtual void override
{
  // No need to map this.
  this->display->fillScreen(color);
}

inline void VirtualMatrixPanel::drawPixelRGB565(int16_t x, int16_t y, uint16_t color)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixelRGB565( coords.x, coords.y, color);
}


inline void VirtualMatrixPanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixelRGB888( coords.x, coords.y, r, g, b);
}

inline void VirtualMatrixPanel::drawPixelRGB24(int16_t x, int16_t y, rgb_24 color)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixelRGB24(coords.x, coords.y, color);
}

// need to recreate this one, as it wouldnt work to just map where it starts.
inline void VirtualMatrixPanel::drawIcon (int *ico, int16_t x, int16_t y, int16_t module_cols, int16_t module_rows) {
  int i, j;
  for (i = 0; i < module_rows; i++) {
    for (j = 0; j < module_cols; j++) {
      // This is a call to this libraries version of drawPixel
      // which will map each pixel, which is what we want.
      drawPixelRGB565 (x + j, y + i, ico[i * module_cols + j]);
    }
  }
}

#endif