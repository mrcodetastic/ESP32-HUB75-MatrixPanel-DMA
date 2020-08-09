#ifndef _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA
#define _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA

/*******************************************************************
    Class contributed by Brian Lough, and expanded by Faptastic.

    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

#include "ESP32-RGB64x32MatrixPanel-I2S-DMA.h"
#include <Fonts/FreeSansBold12pt7b.h>


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

    int16_t panelResX;
    int16_t panelResY;

    RGB64x32MatrixPanel_I2S_DMA *display;

#ifdef USE_GFX_ROOT
    VirtualMatrixPanel(RGB64x32MatrixPanel_I2S_DMA &disp, int vmodule_rows, int vmodule_cols, int panelX, int panelY, bool serpentine_chain = true, bool top_down_chain = false)
      : GFX(vmodule_cols*panelX, vmodule_rows*panelY)
#else
    VirtualMatrixPanel(RGB64x32MatrixPanel_I2S_DMA &disp, int vmodule_rows, int vmodule_cols, int panelX, int panelY, bool serpentine_chain = true, bool top_down_chain = false )
      : Adafruit_GFX(vmodule_cols*panelX, vmodule_rows*panelY)
#endif            
    {
      this->display = &disp;

      panelResX = panelX;
      panelResY = panelY;

      module_rows = vmodule_rows;
      module_cols = vmodule_cols;

      virtualResY = vmodule_rows*panelY;
      virtualResX = vmodule_cols*panelX;      

      _s_chain_party = serpentine_chain; // serpentine, or 'S' chain?
      _chain_top_down= top_down_chain;

    }

    VirtualCoords getCoords(int16_t x, int16_t y);

    // equivalent methods of the matrix library so it can be just swapped out.
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color); // overwrite adafruit implementation
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

    void drawDisplayTest();

  private:
    VirtualCoords coords;
    bool _s_chain_party  = true; // Are we chained? Ain't no party like a... 
    bool _chain_top_down = false; // is the ESP at the top or bottom of the matrix of devices?

}; // end Class header

inline VirtualCoords VirtualMatrixPanel::getCoords(int16_t x, int16_t y) {

  coords.x = coords.y = 0;

  if (x < 0 || x > module_cols*panelResX || y < 0 || y > module_rows*panelResY ) {
    //Serial.printf("VirtualMatrixPanel::getCoords(): Invalid virtual display coordinate. x,y: %d, %d\r\n", x, y);
    return coords;
  }

    uint8_t row = (y / panelResY) + 1; //a non indexed 0 row number
    if(   ( _s_chain_party && !_chain_top_down && (row % 2 == 0) )  // serpentine vertically stacked chain starting from bottom row (i.e. ESP closest to ground), upwards
          ||
          ( _s_chain_party && _chain_top_down  && (row % 2 != 0) )  // serpentine vertically stacked chain starting from the sky downwards
    )
    {
      // First portion gets you to the correct offset for the row you need
      // Second portion inverts the x on the row
      coords.x = (y / panelResY) * (module_cols * panelResX) + (virtualResX - 1 - x);

      // inverts the y the row
      coords.y = panelResY - 1 - (y % panelResY);
    } 
    else 
    {
      coords.x = x + (y / panelResY) * (module_cols * panelResX) ;
      coords.y = y % panelResY;
    }

    // Reverse co-ordinates if panel chain from ESP starts from the TOP RIGHT
    if (_chain_top_down)
    {
      coords.x = (this->display->width()-1) - coords.x;
      coords.y = (this->display->height()-1) - coords.y;
    }

  //Serial.print("Mapping to x: "); Serial.print(coords.x, DEC);  Serial.print(", y: "); Serial.println(coords.y, DEC);  

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

inline void VirtualMatrixPanel::drawDisplayTest()
{  
   this->display->setFont(&FreeSansBold12pt7b);
   this->display->setTextColor(this->display->color565(255, 255, 0));
   this->display->setTextSize(1); 

  for ( int panel = 0; panel < module_cols*module_rows; panel++ ) {
    int top_left_x = (panel == 0) ? 0:(panel*panelResX);
    this->display->drawRect( top_left_x, 0, panelResX, panelResY, this->display->color565( 0, 255, 0));
    this->display->setCursor(panel*panelResX, panelResY-3); 
    this->display->print((module_cols*module_rows)-panel);    
  } 

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