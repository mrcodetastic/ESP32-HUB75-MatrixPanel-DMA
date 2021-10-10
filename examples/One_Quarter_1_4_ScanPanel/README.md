# Using this library with 32x16 1/4 Scan Panels

## Problem
ESP32-HUB75-MatrixPanel-I2S-DMA library will not display output correctly with 1/4 scan panels such [as this](https://de.aliexpress.com/item/33017477904.html?spm=a2g0o.detail.1000023.16.1fedd556Yw52Zi&algo_pvid=4329f1c0-04d2-43d9-bdfd-7d4ee95e6b40&algo_expid=4329f1c0-04d2-43d9-bdfd-7d4ee95e6b40-52&btsid=9a8bf2b5-334b-45ea-a849-063d7461362e&ws_ab_test=searchweb0_0,searchweb201602_10,searchweb201603_60%5BAliExpress%2016x32%5D).

## Solution
It is possible to connect 1/4 scan panels to this library and 'trick' the output to work correctly on these panels by way of adjusting the pixel co-ordinates that are 'sent' to the ESP32-HUB75-MatrixPanel-I2S-DMA library (in this example, it is the 'dmaOutput' class).

Creation of a 'QuarterScanMatrixPanel.h' class which sends an adjusted x,y co-ordinates to the underlying ESP32-HUB75-MatrixPanel-I2S-DMA library's drawPixel routine, to trick the output to look pixel perfect. Refer to the 'getCoords' function within 'QuarterScanMatrixPanel.h'

## Limitations

* Only one font (glcd - standard font) is implemented. This font can't be resized.

## New functions (and adapted function) in this QuarterScanMatrixPanel class
### drawLine
`void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)`

Parameters should be self explained. x0/y0 upper left corner, x1/y1 lower right corner

### drawHLine
`void drawHLine(int16_t x0, int16_t y0, int16_t w, uint16_t color)`

Draw a fast horizontal line with length `w`. Starting at `x0/y0` 

### drawVLine
`void drawVLine(int16_t x0, int16_t y0, int16_t h, uint16_t color)`

Draw a fast vertical line with length `h` Starting at `x0/y0` 

### fillRect
`void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)`

Draw a rectangle starting at `x/y` with width `w` and height `h`in `color`

### drawChar (5x7) Standard font
`drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)`

Draw a char at position x/y in `color` with a background color `bg`
`size` is ignored. 

### writeChar (5x7)
`size_t write(unsigned char c)`

Write a char at current cursor position with current foreground and background color.

### writeString (5x7)
`size_t write(const char *c)`

Write a string at current cursor position with current foreground and background color.
You have to use `setCursor(x,y)` and `setTextFGColor() / setTextBGColor()`

### drawString (5x7)
`void drawString(int16_t x, int16_t y, unsigned char* c, uint16_t color, uint16_t bg)`

Draw String at postion x/y wit foreground `color` and background `bg`
Example: `display.drawString(0,5,"**Welcome**",display.color565(0,60,255));`

### void setScrollDir(uint8_t d = 1)
Set scrolling direction 0=left to right, 1= right to left (default)

### scrollText
`void scrollText(const char *str, uint16_t speed, uint16_t pixels)`

Scroll text `str` into `setScrollDir`. Speed indicates how fast in ms per pixel, pixels are the number pixes which should be scrolled, if not set or 0, than pixels is calculates by size of `*str`

### drawPixel(int16_t x, int16_t y, uint16_t color)
Draw a pixel at x/y in `color`. This function is the atomic function for all above drawing functions

### clearScreen() (all pixels off (black))
Same as `fillScreen(0)`


## Example videos:
https://user-images.githubusercontent.com/949032/104838449-4aae5600-58bb-11eb-802f-a358b49a9315.mp4

https://user-images.githubusercontent.com/949032/104366906-5647f880-551a-11eb-9792-a6f8276629e6.mp4

