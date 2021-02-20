// How to use this library with a FM6126 panel, thanks goes to:
// https://github.com/hzeller/rpi-rgb-led-matrix/issues/746

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>

////////////////////////////////////////////////////////////////////
// FM6126 support is still experimental

// Output resolution and panel chain length configuration
#define PANEL_RES_X 64 	   // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32 	   // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another


// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;

///////////////////////////////////////////////////////////////

// FastLED variables for pattern output
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void setup(){
	
	/*
		The configuration for MatrixPanel_I2S_DMA object is held in HUB75_I2S_CFG structure,
		All options has it's predefined default values. So we can create a new structure and redefine only the options we need

		Please refer to the '2_PatternPlasma.ino' example for detailed example of how to use the MatrixPanel_I2S_DMA configuration
		if you need to change the pin mappings etc.
	*/

	HUB75_I2S_CFG mxconfig(
				PANEL_RES_X,   // module width
				PANEL_RES_Y,   // module height
				PANEL_CHAIN    // Chain length
	);

	mxconfig.driver = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can set it here before creating MatrixPanel_I2S_DMA object

	// OK, now we can create our matrix object
	dma_display = new MatrixPanel_I2S_DMA(mxconfig);
	
    // If you experience ghosting, you will need to reduce the brightness level, not all RGB Matrix
    // Panels are the same - some seem to display ghosting artefacts at lower brightness levels.
    // In the setup() function do something like:

    // let's adjust default brightness to about 75%
    dma_display->setBrightness8(192);    // range is 0-255, 0 - 0%, 255 - 100%
	
    // Allocate memory and start DMA display
    if( not dma_display->begin() )
      Serial.println("****** !KABOOM! Insufficient memory - allocation failed ***********");	
	
    fps_timer = millis();
	
}

void loop(){
   for (int x = 0; x <  dma_display->width(); x++) {
        for (int y = 0; y <  dma_display->height(); y++) {
            int16_t v = 0;
            uint8_t wibble = sin8(time_counter);
            v += sin16(x * wibble * 3 + time_counter);
            v += cos16(y * (128 - wibble)  + time_counter);
            v += sin16(y * x * cos8(-time_counter) / 8);

            currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127); //, brightness, currentBlendType);
            dma_display->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
        }
    }

    ++time_counter;
    ++cycles;
    ++fps;

    if (cycles >= 1024) {
        time_counter = 0;
        cycles = 0;
        currentPalette = palettes[random(0,sizeof(palettes)/sizeof(palettes[0]))];
    }

    // print FPS rate every 5 seconds
    // Note: this is NOT a matrix refresh rate, it's the number of data frames being drawn to the DMA buffer per second
    if (fps_timer + 5000 < millis()){
      Serial.printf_P(PSTR("Effect fps: %d\n"), fps/5);
      fps_timer = millis();
      fps = 0;
    }
}