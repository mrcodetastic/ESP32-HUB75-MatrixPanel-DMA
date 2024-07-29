/*
        _    _   _ ____   ___  ____      _    
       / \  | | | |  _ \ / _ \|  _ \    / \   
      / _ \ | | | | |_) | | | | |_) |  / _ \  
     / ___ \| |_| |  _ <| |_| |  _ <  / ___ \ 
    /_/   \_\\___/|_| \_\\___/|_| \_\/_/   \_\
                                              
           ____  _____ __  __  ___  
          |  _ \| ____|  \/  |/ _ \ 
          | | | |  _| | |\/| | | | |
          | |_| | |___| |  | | |_| |
          |____/|_____|_|  |_|\___/ 

 Description:
 * This demonstrates a combination of the following libraries two:
    - "ESP32-HUB75-MatrixPanel-DMA" to send pixel data to the physical panels in combination with its 
       in-built "VirtualMatrix" class which used to create a virtual display of chained panels, so the
       graphical effects of the Aurora demonstration can be shown on a 'bigger' grid of physical panels
       acting as one big display.

    - "GFX_Lite" to provide a simple graphics library for drawing on the virtual display.
       GFX_Lite is a fork of AdaFruitGFX and FastLED library combined together, with a focus on simplicity and ease of use.

 Instructions:
  * Use the serial input to advance through the patterns, or to toggle auto advance. Sending 'n' will advance to the next
    pattern, 'p' will go to the previous pattern. Sending 'a' will toggle auto advance on and off.

*/

#define USE_GFX_LITE 1
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

/***************************************************************************************************************************/

// Step 1) Provide the size of each individual physical panel LED Matrix panel that is chained (or not) together
#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

// Step 2) Provide details of the physical panel chaining that is in place.
#define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
#define NUM_COLS 1 // Number of INDIVIDUAL PANELS per ROW
#define PANEL_CHAIN NUM_ROWS*NUM_COLS    // total number of panels chained one to another

// Step 3) How are the panels chained together?
#define PANEL_CHAIN_TYPE CHAIN_TOP_RIGHT_DOWN

// Refer to: https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/examples/VirtualMatrixPanel
//      and: https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/blob/master/doc/VirtualMatrixPanel.pdf

// Virtual Panel dimensions - our combined panel would be a square 4x4 modules with a combined resolution of 128x128 pixels
#define VPANEL_W PANEL_RES_X*NUM_COLS // Kosso: All Pattern files have had the MATRIX_WIDTH and MATRIX_HEIGHT replaced by these.
#define VPANEL_H PANEL_RES_Y*NUM_ROWS //

/***************************************************************************************************************************/

// The palettes are set to change every 60 seconds. 
int lastPattern = 0;


// placeholder for the matrix object
MatrixPanel_I2S_DMA *matrix = nullptr;

// placeholder for the virtual display object
VirtualMatrixPanel  *virtualDisp = nullptr;


#include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
EffectsLayer effects(VPANEL_W, VPANEL_H);

#include "Drawable.hpp"
#include "Geometry.hpp"

#include "Patterns.hpp"
Patterns patterns;

/* -------------------------- Some variables -------------------------- */
unsigned long ms_current  = 0;
unsigned long ms_previous = 0;
unsigned long ms_previous_palette = 0;
unsigned long ms_animation_max_duration = 30000; // 10 seconds
unsigned long next_frame = 0;

void listPatterns();

void setup()
{
  // Setup serial interface
  Serial.begin(115200);
  delay(250);


  // Configure your matrix setup here
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);

  // custom pin mapping (if required)
  //HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  //mxconfig.gpio = _pins;

  // in case that we use panels based on FM6126A chip, we can change that
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // FM6126A panels could be cloked at 20MHz with no visual artefacts
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;

  // OK, now we can create our matrix object
  matrix = new MatrixPanel_I2S_DMA(mxconfig);

  // Allocate memory and start DMA display
  if( not matrix->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

  // let's adjust default brightness to about 75%
  matrix->setBrightness8(192);    // range is 0-255, 0 - 0%, 255 - 100%

  // create VirtualDisplay object based on our newly created dma_display object
  virtualDisp = new VirtualMatrixPanel((*matrix), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN_TYPE);

  Serial.println("**************** Starting Aurora Effects Demo ****************");

  Serial.print("MATRIX_WIDTH: ");  Serial.println(PANEL_RES_X*PANEL_CHAIN);
  Serial.print("MATRIX_HEIGHT: "); Serial.println(PANEL_RES_Y);

#ifdef VPANEL_W
  Serial.println("VIRTUAL PANEL WIDTH " + String(VPANEL_W));
  Serial.println("VIRTUAL PANEL HEIGHT " + String(VPANEL_H));
#endif


  listPatterns();

  patterns.setPattern(0);
  patterns.start();     

  ms_previous = millis();

  Serial.print("Starting with pattern: ");
  Serial.println(patterns.getCurrentPatternName());

}


bool   autoAdvance = true;
char   incomingByte    = 0;
void handleSerialRead()
{
    if (Serial.available() > 0) {

        // read the incoming byte:
        incomingByte = Serial.read();

        if (incomingByte == 'n') {
            Serial.println("Going to next pattern");
            patterns.move(1);
        }

        if (incomingByte == 'p') {
            Serial.println("Going to previous pattern");
            patterns.move(-1);
        }

        if (incomingByte == 'a') {
            autoAdvance = !autoAdvance;

            if (autoAdvance)
              Serial.println("Auto pattern advance is ON");
            else
              Serial.println("Auto pattern advance is OFF");
        } 

        ms_previous = millis();
    }
} // end handleSerialRead


void loop()
{

  handleSerialRead();

    ms_current = millis();

    if (ms_current - ms_previous_palette > 10000) // change colour palette evert 10 seconds
    {
      effects.RandomPalette();
      ms_previous_palette = ms_current;
    }

    if ( ((ms_current - ms_previous) > ms_animation_max_duration) && autoAdvance) 
    {

       patterns.move(1);

       ms_previous = ms_current;
    }
 
    if ( next_frame < ms_current)
      next_frame = patterns.drawFrame() + ms_current; 
       
}


void listPatterns() {
  patterns.listPatterns();
}
