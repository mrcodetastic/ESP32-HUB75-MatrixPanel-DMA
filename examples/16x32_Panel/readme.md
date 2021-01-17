# 16x32 virtual panel
Only one font (glcd - standard font) is implemented. This font can't be resized.

## Installation
Please insert the file `ESP-32_HUB75_32x16MatrixPanel-I2S-DMA.h` in the ESP-32_HUB75 library folder.
See demo file how to work with this virtual panel

## Problem
Current HUB75 64x32 library didn't work with a 16x32 panel without patches. 

## Solution
Creation of a virtual 16x32 panel which inherit all stuff from the 64x32 lib. Technically a 16x32 panel is a 1/4 of the bit 64x32 panel.
In this case the virtual panel use a mapping table for x/y coordinates and reimplement all neccessary drawing function because every function which work with coordinates must remap x & y.

Additonally this 16x32 panel has some simple drawing & scrolling mechanisms.

# new functions (and adapted function)
## drawLine
`void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)`

Parameters should be self explained. x0/y0 upper left corner, x1/y1 lower right corner

## drawHLine
`void drawHLine(int16_t x0, int16_t y0, int16_t w, uint16_t color)`

Draw a fast horizontal line with length `w`. Starting at `x0/y0` 

## drawVLine
`void drawVLine(int16_t x0, int16_t y0, int16_t h, uint16_t color)`

Draw a fast vertical line with length `h` Starting at `x0/y0` 

## fillRect
`void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)`

Draw a rectangle starting at `x/y` with width `w` and height `h`in `color`

## drawChar (5x7) Standard font
`drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)`

Draw a char at position x/y in `color` with a background color `bg`
`size` is ignored. 

## writeChar (5x7)
`size_t write(unsigned char c)`

Write a char at current cursor position with current foreground and background color.

## writeString (5x7)
`size_t write(const char *c)`

Write a string at current cursor position with current foreground and background color.
You have to use `setCursor(x,y)` and `setTextFGColor() / setTextBGColor()`

## drawString (5x7)
`void drawString(int16_t x, int16_t y, unsigned char* c, uint16_t color, uint16_t bg)`

Draw String at postion x/y wit foreground `color` and background `bg`
Example: `display.drawString(0,5,"**Welcome**",display.color565(0,60,255));`

## void setScrollDir(uint8_t d = 1)
Set scrolling direction 0=left to right, 1= right to left (default)

## scrollText
`void scrollText(const char *str, uint16_t speed, uint16_t pixels)`

Scroll text `str` into `setScrollDir`. Speed indicates how fast in ms per pixel, pixels are the number pixes which should be scrolled, if not set or 0, than pixels is calculates by size of `*str`

## drawPixel(int16_t x, int16_t y, uint16_t color)
Draw a pixel at x/y in `color`. This function is the atomic function for all above drawing functions

## clearScreen() (all pixels off (black))
Same as `fillScreen(0)`

