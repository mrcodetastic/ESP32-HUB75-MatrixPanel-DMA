
/*
  Patch class for 32x16 RGB Matrix panels

  reimplement all functions which use x,y coordinates
  
*/

#ifndef ESP_HUB75_32x16MatrixPanel
#define ESP_HUB75_32x16MatrixPanel

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include "glcdfont.c"

struct VirtualCoords {
  int16_t x;
  int16_t y;
};

#define VP_WIDTH 32
#define VP_HEIGHT 16
#define DEFAULT_FONT_W 5
#define DEFAULT_FONT_H 7
#define PIXEL_SPACE 1       // space between chars in a string


/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)


/* --- end macros --- */


class QuarterScanMatrixPanel : public Adafruit_GFX
{

  public:

    MatrixPanel_I2S_DMA *display;

    QuarterScanMatrixPanel(MatrixPanel_I2S_DMA &disp)  : Adafruit_GFX(64, 32)
    {
      this->display = &disp;
      size_x = size_y = 1 ;
      wrap = false;
      cursor_x = cursor_y = 0;
      dir = 1;
      loop = true;

    }

    VirtualCoords getCoords(int16_t x, int16_t y);
    int16_t getVirtualX(int16_t x) {
      VirtualCoords coords = getCoords(x, 0);
      return coords.x;
    }
    int16_t getVirtualY(int16_t y) {
      VirtualCoords coords = getCoords(0,y);
      return coords.y;
    }
//    int16_t getVirtualY(int16_t y) {return getCoords(0,y).y;}
    /** extende function to draw lines/rects/... **/
    virtual uint8_t width() {return VP_WIDTH;};
    virtual uint8_t height() {return VP_HEIGHT;};

    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    virtual void drawHLine(int16_t x0, int16_t y0, int16_t w, uint16_t color);
    virtual void drawVLine(int16_t x0, int16_t y0, int16_t h, uint16_t color);
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    virtual void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);
    virtual void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y);
    virtual void scrollChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint16_t dir, uint16_t speed);
    virtual void drawString(int16_t x, int16_t y, unsigned char* c, uint16_t color, uint16_t bg);
    virtual size_t write(unsigned char c);   // write a character on current cursor postion
    virtual size_t write(const char *str);  // write a character array (string) on curreont cursor postion

    virtual void setTextWrap(bool w);
    virtual void setCursor (int16_t x, int16_t y);
    void setTextFGColor(uint16_t color) {textFGColor = color;};
    void setTextBGColor(uint16_t color) {textBGColor = color;};
    void setTextSize(uint8_t x, uint8_t y) {size_x = x; size_y = y;}; // magnification, default = 1

    void setScrollDir(uint8_t d = 1) { dir = (d != 1) ? 0 : 1;};    // set scroll dir default = 1
    void setScroolLoop (bool b = true) { loop = b;} ;               // scroll text in a loop, default true
    void scrollText(const char *str, uint16_t speed, uint16_t pixels);
    /**------------------------------------------**/

    // equivalent methods of the matrix library so it can be just swapped out.
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void fillScreen(uint16_t color); // overwrite adafruit implementation
    void clearScreen() { fillScreen(0); }
    //void drawPixelRGB565(int16_t x, int16_t y, uint16_t color);
    void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    //void drawPixelRGB24(int16_t x, int16_t y, RGB24 color);
    void drawIcon (int *ico, int16_t x, int16_t y, int16_t module_cols, int16_t module_rows);

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
	void showDMABuffer() { display->showDMABuffer(); }

    void drawDisplayTest();

  protected:
    int16_t cursor_x, cursor_y;   // Cursor position
    uint8_t size_x, size_y;       // Font size Multiplikator default = 1 => 5x7 Font (5widht,7Height)
    uint16_t textFGColor, textBGColor;
    bool wrap ;           //  < If set, 'wrap' text at right edge of display 
    uint8_t dir ;   // used for scrolling text direction
    bool loop ;           // used for scrolling text in a loop

  private:
    VirtualCoords coords;

}; // end Class header


/***************************************************************************************

  @brief scroll text from right to left or vice versa on current cursor position
  please note, this function is not interruptable.

  @param *c pointer to \0 terminated string
  @param pixels number of pixels to scroll, if 0, than scroll complete text
  @param speed velocity of scrolling in ms
***************************************************************************************/
void QuarterScanMatrixPanel::scrollText(const char *str,uint16_t speed,  uint16_t pixels = 0) {
  // first we put all columns of every char inside str into a big array of lines
  // than we move through this arry and draw line per line and move this line
  // one position to dir
  const uint8_t xSize = 6;
  uint16_t len = strlen(str);
  uint8_t array[len * xSize];         // size of array number of chars * width of char
  //uint16_t lenArray = sizeof(array)/sizeof(array[0]);
  uint16_t aPtr = 0;
  //
  // generate array
  char c = *str;
  // Serial.printf("size *str (%d), size array: (%d) \n", len, lenArray);

  while (c) {
    // Serial.printf("** %c ** \n", c);
    // read font line per line. A line is a column inside a char
    for (int8_t i = 0; i < 5; i++) {
      uint8_t line = pgm_read_byte(&font[c * 5 + i]); 
      array[aPtr++] = line;
      // Serial.printf("%d - Line " PRINTF_BINARY_PATTERN_INT8 "\n", i, PRINTF_BYTE_TO_BINARY_INT8(line) );
    }
    str++;
    c = *str;
    array[aPtr++] = 0x00;     // line with 0 (space between chars)

  }
  array[aPtr++] = 0x00;     // line with 0 (space between chars)
/*
  Serial.printf("---------------------------- \n");
  for (aPtr=0; aPtr < (len*xSize); aPtr++) {
    Serial.printf("%d - Line " PRINTF_BINARY_PATTERN_INT8 "\n", aPtr, PRINTF_BYTE_TO_BINARY_INT8(array[aPtr]) );
  }
*/

  int16_t x,y,lastX, p;
  lastX = (dir) ? VP_WIDTH : 0;
  x = cursor_x;
  y = cursor_y;
  Serial.printf("X: %d, Y: %d \n", x,y);
  p=0;
  pixels = (pixels) ? pixels : len * xSize;

  while (p <= pixels) {
    // remove last pixel positions
    fillRect(x,y,5,7,textBGColor);
    // set new pixel position
    x = (dir) ? lastX - p : lastX + p - pixels;
    // iterator through our array
    for (uint8_t i=0; i < (len*xSize); i++) {
      uint8_t line = array[i];
      //Serial.printf("%d:%d : " PRINTF_BINARY_PATTERN_INT8 "\n", x, i, PRINTF_BYTE_TO_BINARY_INT8(line) );
      // read line and shift from right to left
      // start with bit 0 (top of char) to 7(bottom)
      for (uint8_t j=0; j < 8; j++, line>>=1) {
        if (line & 1) {
          // got 1, if x + i outside panel ignore pixel
          if (x + i >= 0 && x + i < VP_WIDTH) {
            drawPixel(x + i, y + j, textFGColor);
          }
        }
        else {
          // got 0
          if (x + i >= 0 && x + i < VP_WIDTH) {
            drawPixel(x + i, y + j, textBGColor);
          }
        } // if
      } // for j
    } // for i
    p++;
    delay(speed);

  } // while 
}

inline size_t QuarterScanMatrixPanel::write(const char *str) {
  uint8_t x, y;
  x=cursor_x;
  y=cursor_y;
  char c = *str;
  while (c) {
    //Serial.printf("%c ", c);
    write(c);
    str++;
    c = *str;  
    x = x + ((DEFAULT_FONT_W + PIXEL_SPACE) * size_x);
    setCursor(x,y);
  }
  Serial.printf("\n");
  return 1;
}

inline size_t QuarterScanMatrixPanel::write(unsigned char c) {
  Serial.printf("\twrite(%d, %d, %c)\n", cursor_x, cursor_y, c);
  drawChar(cursor_x, cursor_y, c, textFGColor, textBGColor, size_x, size_y);
  return 1;
}

void QuarterScanMatrixPanel::setTextWrap(bool w) {this->display->setTextWrap(w);}
void QuarterScanMatrixPanel::setCursor(int16_t x, int16_t y) {
  cursor_x = x;
  cursor_y = y;
}


/* - new for 16x32 panels - */
inline void QuarterScanMatrixPanel::drawLine(int16_t x, int16_t y, int16_t x1, int16_t y1, uint16_t color)
{
  int16_t a,b;
  for (a=x; a <= x1; a++) {
    for (b=y; b <= y1; b++) {
      drawPixel(a,b,color);
    }
  }
}

inline void QuarterScanMatrixPanel::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  drawLine(x,y,x+w,y, color);
}

inline void QuarterScanMatrixPanel::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  drawLine(x,y,x,y+h, color);
}

inline void QuarterScanMatrixPanel::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) 
{
  for (int16_t i = x; i < x + w; i++) {
    drawVLine(i, y, h, color);
  } 
}

void QuarterScanMatrixPanel::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
  drawChar(x,y,c,color, bg, size, size);
}

inline void QuarterScanMatrixPanel::scrollChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint16_t dir, uint16_t speed){

  if ((x >= VP_WIDTH) ||
    (y >= VP_HEIGHT) ||
    ((x + 6 * size_x-1) < 0) ||
    ((y + 8 * size_y-1) <0))
    return; 
  setTextWrap(true);  
  // text wrap is only for the right end of the panel, to scroll soft out of the left of panel
  // algorithm should wrap the character from left to right


  // loop s = scroll-loop, scrolls char 5 pixels into dir
  uint8_t lastX = x;
  for (int8_t s = 0; s < 6; s++) {
    // loop i : width of a character 
    Serial.printf("X:%d ", x);

    // clear current position
    fillRect(x,y,5,7,0);
    x = lastX - s;
    for (int8_t i = 0; i < 5; i++) {
      // first line is the firste vertical part of a character and 8bits long
      // last bit is everytime 0
      // we read 5 lines with 8 bit (5x7 char + 8bit with zeros)
      // Example : char A (90deg cw)
      // 01111100
      // 00010010
      // 00010001
      // 00010010
      // 01111100
      uint8_t line = pgm_read_byte(&font[c * 5 + i]); 
      // shift from right to left bit per bit  
      // loop j = height of a character
      // loop through a colunm of currenc character
      Serial.printf("i:%d ", i);
      // ignore all pixels outside panel
      if (x+i >= VP_WIDTH) continue;

      for (int8_t j=0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          Serial.printf
          (" ON %d", x+i);
          // we read 1
          if (x >= 0) {
            drawPixel(x+i, y+j, color);
          }
          else if (x+i >= 0) {
            drawPixel(x+i, y+j, color);
          }
        }
        else if (bg != color) {
          // we read 0
          Serial.printf(" OFF %d", x+i);

          if (x >= 0) {
            drawPixel(x+i, y+j, bg);
          }
          else if (x+i >= 0) {
            drawPixel(x+i, y+j, bg);
          }        
        }
      }
    }
    Serial.printf("\n");
    delay(speed);
  }
}


inline void QuarterScanMatrixPanel::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y)
{
  //Serial.printf("unmapped : drawChar(%d, %d, %c) \n",x, y, c);

  // note: remapping to 16x32 coordinats is done inside drawPixel() or fillRect

  if ((x >= VP_WIDTH) ||
      (y >= VP_HEIGHT) ||
      ((x + 6 * size_x-1) < 0) ||
      ((y + 8 * size_y-1) <0))
      return;
  //Serial.printf("Font-Array : %d \n", sizeof(font));
  for (int8_t i = 0; i < 5; i++) {
      uint8_t line = pgm_read_byte(&font[c * 5 + i]);
      //Serial.printf("%d - Line " PRINTF_BINARY_PATTERN_INT8 "\n", i, PRINTF_BYTE_TO_BINARY_INT8(line) );
      for (int8_t j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          if (size_x == 1 && size_y == 1)
            //Serial.printf("");
            drawPixel(x + i, y + j, color);
          else
            // remark: it's important to call function with orgininal coordinates for x/y
            fillRect(x + i * size_x, y + j * size_y, size_x, size_y,
                          color);
        } else if (bg != color) {
          if (size_x == 1 && size_y == 1)
            drawPixel(x + i, y + j, bg);
          else
            // remark: it's important to call function with orgininal coordinates for x/y
            fillRect(x + i * size_x, y + j * size_y, size_x, size_y, bg);
        }
      }
    }

}

inline void QuarterScanMatrixPanel::drawString(int16_t x, int16_t y, unsigned char* c, uint16_t color, uint16_t bg) {

}

inline VirtualCoords QuarterScanMatrixPanel::getCoords(int16_t x, int16_t y)
{
  const int y_remap[] = { 0,1,8,9,4,5,12,13,16,17,24,25,22,23,30,31 };
  if (y > VP_HEIGHT) 
    y = VP_HEIGHT;
  if (x > VP_WIDTH)
    x = VP_WIDTH;
  coords.x = x + VP_WIDTH;
  coords.y = y_remap[y];
  return coords;  
}


/* -------------------------*/

inline void QuarterScanMatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixel(coords.x, coords.y, color);
}

inline void QuarterScanMatrixPanel::fillScreen(uint16_t color)  // adafruit virtual void override
{
  // No need to map this.
  this->display->fillScreen(color);
}
/*
inline void QuarterScanMatrixPanel::drawPixelRGB565(int16_t x, int16_t y, uint16_t color)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixelRGB565( coords.x, coords.y, color);
}
*/


inline void QuarterScanMatrixPanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixelRGB888( coords.x, coords.y, r, g, b);
}
/*
inline void QuarterScanMatrixPanel::drawPixelRGB24(int16_t x, int16_t y, RGB24 color)
{
  VirtualCoords coords = getCoords(x, y);
  this->display->drawPixelRGB24(coords.x, coords.y, color);
}
*/


// need to recreate this one, as it wouldnt work to just map where it starts.
inline void QuarterScanMatrixPanel::drawIcon (int *ico, int16_t x, int16_t y, int16_t module_cols, int16_t module_rows) { }

#endif