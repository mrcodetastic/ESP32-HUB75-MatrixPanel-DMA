/**
 * Experimental layer class to do play with pixel in an off-screen buffer before painting to the DMA 
 *
 * Requires FastLED
 *
 * Faptastic 2020-2021
 **/

#include "FastLED_Pixel_Buffer.h"

/**
 *  The one for 256+ matrices
 *  otherwise this:
 *    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {}
 *  turns into an infinite loop
 */
inline uint16_t VirtualMatrixPanel_FastLED_Pixel_Buffer::XY16( uint16_t x, uint16_t y) {

    if (x >= virtualResX) return 0;
    if (y >= virtualResY) return 0;
    
    return (y * virtualResX) + x + 1; // everything offset by one to compute out of bounds stuff - never displayed by ShowFrame()
}

// For adafruit
void VirtualMatrixPanel_FastLED_Pixel_Buffer::drawPixel(int16_t x, int16_t y, uint16_t color) {

  //Serial.println("calling our drawpixel!");

    // 565 color conversion
    uint8_t r = ((((color >> 11)  & 0x1F) * 527) + 23) >> 6;
    uint8_t g = ((((color >> 5)   & 0x3F) * 259) + 33) >> 6;
    uint8_t b = (((color & 0x1F)  * 527) + 23) >> 6;

    this->drawPixel(x, y, CRGB(r,g,b));
}

void VirtualMatrixPanel_FastLED_Pixel_Buffer::drawPixel(int16_t x, int16_t y, int r, int g, int b) {
    this->drawPixel(x, y, CRGB(r,g,b));
}

// We actually just draw to ourselves... to our buffer
void VirtualMatrixPanel_FastLED_Pixel_Buffer::drawPixel(int16_t x, int16_t y, CRGB color) 
{
  //Serial.printf("updated x y : %d %d", x, y);
    buffer[XY16(x,y)] = color;
}

CRGB VirtualMatrixPanel_FastLED_Pixel_Buffer::getPixel(int16_t x, int16_t y) 
{
    return buffer[XY16(x,y)];
}

/**
 * Dim all the pixels on the layer.
 */
void VirtualMatrixPanel_FastLED_Pixel_Buffer::dimAll(byte value) {   

        //Serial.println("performing dimall");
        // nscale8 max value is 255, or it'll flip back to 0 
        // (documentation is wrong when it says x/256), it's actually x/255
        /*
        for (int y = 0; y < LAYER_HEIGHT; y++) {
            for (int x = 0; x < LAYER_WIDTH; x++) {
                pixels->data[y][x].nscale8(value);
        }}
        */
        dimRect(0,0, virtualResX, virtualResY, value);
}

/**
 * Dim all the pixels in a rectangular option of the layer the layer.
 */
void VirtualMatrixPanel_FastLED_Pixel_Buffer::dimRect(int16_t x, int16_t y, int16_t w, int16_t h, byte value) {
        for (int16_t i = x; i < x + w; i++)
        {
            for (int16_t j = y; j < y + h; j++)
            {
                buffer[XY16(i,j)].nscale8(value);
            }
        }
}

void VirtualMatrixPanel_FastLED_Pixel_Buffer::clear() {
    memset(buffer, CRGB(0,0,0), (virtualResX * virtualResY) );
}

/**
 * Actually Send the CRGB FastLED buffer to the DMA engine / Physical Panels!
 * Do this via the underlying 'VirtualMatrixPanel' that does all the pixel-remapping for 
 * all sorts of chained panels, and panel scan types.
 */
void VirtualMatrixPanel_FastLED_Pixel_Buffer::show() { 

    //Serial.println("Doing Show");
    
    CRGB _pixel = 0;
    for (int16_t y = 0; y < virtualResY; y++) {
        for (int16_t x = 0; x < virtualResX; x++)
        {
            //VirtualMatrixPanel::getCoords(x, y); // call to base to update coords for chaining approach           
            _pixel = buffer[XY16(x,y)];
            drawPixelRGB888( x, y, _pixel.r, _pixel.g, _pixel.b); // call VirtualMatrixPanel::drawPixelRGB888(...)
            //drawPixelRGB888( x, y, 0, 0, 128); // call VirtualMatrixPanel::drawPixelRGB888(...)
        } // end loop to copy fast led to the dma matrix
    }

} // show

/**
 * Cleanup should we delete this buffer class. Unlikely during runtime.
 */
VirtualMatrixPanel_FastLED_Pixel_Buffer::~VirtualMatrixPanel_FastLED_Pixel_Buffer(void)
{
  delete(buffer);
}