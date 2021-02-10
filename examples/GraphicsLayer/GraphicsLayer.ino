/*
 * An example that writes to a CRGB FastLED pixel buffer before being ultimately sent
 * to the DMA Display.
 * 
 * Faptastic 2020
 * 
 * Note: 
 *    * Layers use lots of RAM (3*WIDTH*HEIGHT bytes per layer to be precise), so use at your own risk. 
 *    * Make sure LAYER_WIDTH and LAYER_HEIGHT are correctly configured in Layer.h !!!
 */
 
//#define USE_CUSTOM_PINS // uncomment to use custom pins, then provide below

#define A_PIN  26
#define B_PIN  4
#define C_PIN  27
#define D_PIN  2
#define E_PIN  21 

#define R1_PIN   5
#define R2_PIN  19
#define G1_PIN  17
#define G2_PIN  16
#define B1_PIN  18
#define B2_PIN  25

#define CLK_PIN  14
#define LAT_PIN  15
#define OE_PIN  13


#include <FastLED.h>  // FastLED needs to be installed.
#include "Layer.h"    // Layer Library
#include "Fonts/FreeSansBold9pt7b.h" // include adafruit font

/* 
 * Below is an is the 'legacy' way of initialising the MatrixPanel_I2S_DMA class.
 * i.e. MATRIX_WIDTH and MATRIX_HEIGHT are modified by compile-time directives.
 * By default the library assumes a single 64x32 pixel panel is connected.
 *
 * Refer to the example '2_PatternPlasma' on the new / correct way to setup this library
 * for different resolutions / panel chain lengths within the sketch 'setup()'.
 * 
 */
MatrixPanel_I2S_DMA dma_display; // Create HUB75 DMA object

// Create FastLED based graphic 'layers'
Layer               bgLayer(dma_display);         // Create background Layer
Layer               textLayer(dma_display);     // Create foreground Layer


int time_counter = 0;
int cycles = 0;
CRGBPalette16 currentPalette;
CRGB currentColor;


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void setup() {
  
  Serial.begin(115200);
  
  Serial.println("*****************************************************");
  Serial.println(" HELLO !");
  Serial.println("*****************************************************");

#ifdef USE_CUSTOM_PINS
  dma_display.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
#else
  dma_display.begin();
#endif
 
  // fill the screen with 'black'
  dma_display.fillScreen(dma_display.color444(0, 0, 0));

  // Set current FastLED palette
  currentPalette = RainbowColors_p;

  // Allocate Background Layer
  bgLayer.init();
  bgLayer.clear();
  bgLayer.setTransparency(false);

  // Allocate Canvas Layer
  textLayer.init();
  textLayer.clear();  

  /* Step 1: Write some pixels to foreground Layer (use custom layer function)
   * Only need to do this once as we're not changing it ever again in this example.
   */
  textLayer.drawCentreText("COOL!", MIDDLE, &FreeSansBold9pt7b, CRGB(255,255,255));
  textLayer.autoCenterX(); // because I don't trust AdaFruit to perfectly place the contents in the middle

}

int LayerCompositor_mode = 0;
void loop() {
  
   for (int x = 0; x <  dma_display.width(); x++) {
            for (int y = 0; y <  dma_display.height(); y++) {
                int16_t v = 0;
                uint8_t wibble = sin8(time_counter);
                v += sin16(x * wibble * 3 + time_counter);
                v += cos16(y * (128 - wibble)  + time_counter);
                v += sin16(y * x * cos8(-time_counter) / 8);

        				currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127); //, brightness, currentBlendType);
        
                /* 
                 *  Step 2: Write to Background layer! Don't show it on the screen just yet.
                 *          Note: Layer class is designed for FastLED 'CRGB' data type.
                 */                
        				bgLayer.drawPixel(x, y, currentColor);
				
            }
        }

        time_counter += 1;
        cycles++;

        if (cycles >= 2048) {
            time_counter = 0;
            cycles = 0;
        }

        /* 
         *  Step 3: Merge foreground and background layers and send to the matrix panel!
         *  Use our special sauce LayerCompositor functions
         */
         switch (LayerCompositor_mode)
         {
          case 0:
            LayerCompositor::Siloette(dma_display, bgLayer, textLayer);
            break;

          case 1:
            LayerCompositor::Stack(dma_display, bgLayer, textLayer);
            break;

          case 2:
            LayerCompositor::Blend(dma_display, bgLayer, textLayer);
            break;          
         }

        EVERY_N_SECONDS(5) { // FastLED Macro
          LayerCompositor_mode++;
          dma_display.clearScreen();
          if (LayerCompositor_mode > 2) LayerCompositor_mode = 0;          
        }         
         
         //
         // LayerCompositor::Blend(dma_display, bgLayer, textLayer, 127);
        
} // end loop
