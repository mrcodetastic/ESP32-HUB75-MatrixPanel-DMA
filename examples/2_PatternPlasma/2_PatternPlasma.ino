/*
 * Portions of this code are adapted from Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from LedEffects Plasma by Robert Atkins: https://bitbucket.org/ratkins/ledeffects/src/26ed3c51912af6fac5f1304629c7b4ab7ac8ca4b/Plasma.cpp?at=default
 * Copyright (c) 2013 Robert Atkins
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
 

// HUB75E pinout
// R1 | G1
// B1 | GND
// R2 | G2
// B2 | E
//  A | B
//  C | D
// CLK| LAT
// OE | GND

/*  Default library pin configuration for the reference
  you can redefine only ones you need later on object creation

#define R1 25
#define G1 26
#define BL1 27
#define R2 14
#define G2 12
#define BL2 13
#define CH_A 23
#define CH_B 19
#define CH_C 5
#define CH_D 17
#define CH_E -1 // assign to any available pin if using two panels or 64x64 panels with 1/32 scan
#define CLK 16
#define LAT 4
#define OE 15

*/


#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// https://github.com/mrcodetastic/GFX_Lite/tree/main
#include <GFX_Lite.h>

#include <math.h>

// Configure for your panel(s) as appropriate!
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32  	// Panel height of 64 will required PIN_E to be defined.
#define PANELS_NUMBER 1 	// Number of chained panels, if just a single panel, obviously set to 1
#define PIN_E 32

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT


// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;


uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];
uint8_t paletteShift = 0;
uint8_t paletteNumber = 2;


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void setup() {
  
  Serial.begin(115200);
  
  Serial.println(F("*****************************************************"));
  Serial.println(F("*        ESP32-HUB75-MatrixPanel-I2S-DMA DEMO       *"));
  Serial.println(F("*****************************************************"));

  /*
    The configuration for MatrixPanel_I2S_DMA object is held in HUB75_I2S_CFG structure,
    pls refer to the lib header file for full details.
    All options has it's predefined default values. So we can create a new structure and redefine only the options we need 

    // those are the defaults
    mxconfig.mx_width = 64;                   // physical width of a single matrix panel module (in pixels, usually it is always 64 ;) )
    mxconfig.mx_height = 32;                  // physical height of a single matrix panel module (in pixels, usually almost always it is either 32 or 64)
    mxconfig.chain_length = 1;                // number of chained panels regardless of the topology, default 1 - a single matrix module
    mxconfig.gpio.r1 = R1;                    // pin mappings
    mxconfig.gpio.g1 = G1;
    mxconfig.gpio.b1 = B1;                    // etc
    mxconfig.driver = HUB75_I2S_CFG::SHIFT;   // shift reg driver, default is plain shift register
    mxconfig.double_buff = false;             // use double buffer (twice amount of RAM required)
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;// I2S clock speed, better leave as-is unless you want to experiment
  */

  /*
    For example we have two 64x64 panels chained, so we need to customize our setup like this

  */
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  mxconfig.gpio.e = PIN_E;                // we MUST assign pin e to some free pin on a board to drive 64 pix height panels with 1/32 scan
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can change that

  /*
    //Another way of creating config structure
    //Custom pin mapping for all pins
    HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
    HUB75_I2S_CFG mxconfig(
                            64,   // width
                            64,   // height
                             4,   // chain length
                         _pins,   // pin mapping
      HUB75_I2S_CFG::FM6126A      // driver chip
    );

  */


  // OK, now we can create our matrix object
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 75%
  dma_display->setBrightness8(255);    // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
 
  // well, hope we are OK, let's draw some colors first :)
  Serial.println("Fill screen: RED");
  dma_display->fillScreenRGB888(192, 0, 0);
  delay(1000);

  Serial.println("Fill screen: GREEN");
  dma_display->fillScreenRGB888(0, 192, 0);
  delay(1000);

  Serial.println("Fill screen: BLUE");
  dma_display->fillScreenRGB888(0, 0, 192);
  delay(1000);

  Serial.println("Fill screen: Neutral White");
  dma_display->fillScreenRGB888(64, 64, 64);
  delay(1000);

  Serial.println("Fill screen: black");
  dma_display->fillScreenRGB888(0, 0, 0);
  delay(1000);


  // Set current FastLED palette
  currentPalette = RainbowColors_p;
  Serial.println("Starting plasma effect...");
  fps_timer = millis();
}

void loop() {

    // Use the configured display dimensions rather than the example's panel
    // constants, so chained panels and different panel heights use the full area.
    const int16_t width = dma_display->width();
    const int16_t height = dma_display->height();
    const int16_t minDimension = min(width, height);

    // Coordinates are aspect-correct and normalized to the short screen edge.
    // This keeps the rings round and their size consistent on 64x32, 64x64,
    // 128x64, and other display sizes.
    const int16_t driftX = (int32_t)sin16((uint16_t)(time_counter * 18)) * 28 / 32767;
    const int16_t driftY = (int32_t)cos16((uint16_t)(time_counter * 18)) * 20 / 32767;
    const int16_t phase = time_counter;

    for (int16_t y = 0; y < height; ++y) {
        const int16_t yCoord = ((2 * y - (height - 1)) * 128) / minDimension;
        for (int16_t x = 0; x < width; ++x) {
            const int16_t xCoord = ((2 * x - (width - 1)) * 128) / minDimension;
            const int16_t px = xCoord + driftX;
            const int16_t py = yCoord + driftY;

            // Several differently directed waves create an organic plasma field;
            // the radial term supplies the moving rings instead of a single
            // product of x and y that tends to pin the pattern to the origin.
            const int16_t waveX = sin16((uint16_t)(px * 600 + phase * 6));
            const int16_t waveY = sin16((uint16_t)(py * 600 - phase * 5));
            const int16_t waveDiagonal = sin16((uint16_t)((px + py) * 360 + phase * 4));
            const uint16_t radius = (uint16_t)sqrtf((float)px * px + (float)py * py);
            const int16_t waveRings = sin16((uint16_t)(radius * 1100 - phase * 8));

            const int32_t field = (int32_t)waveX + waveY + waveDiagonal + waveRings;
            const uint8_t colorIndex = (uint8_t)(((field + 131072) >> 10) + paletteShift);
            currentColor = ColorFromPalette(currentPalette, colorIndex);
            dma_display->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
        }
    }

    ++time_counter;
    ++cycles;
    ++fps;

    // Slowly move the whole palette through its colors independently of the
    // spatial waves, so the plasma keeps evolving even when the field repeats.
    if ((cycles % 3) == 0) {
      ++paletteShift;
    }

    if (cycles >= 2026) {
        cycles = 0;
        paletteNumber = (paletteNumber + 1) % (sizeof(palettes) / sizeof(palettes[0]));
        currentPalette = palettes[paletteNumber];
    }

    // print FPS rate every 5 seconds
    // Note: this is NOT a matrix refresh rate, it's the number of data frames being drawn to the DMA buffer per second
    if (fps_timer + 5000 < millis()){
      Serial.printf_P(PSTR("Effect fps: %d\n"), fps/5);
      fps_timer = millis();
      fps = 0;
    }
} // end loop
