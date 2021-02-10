#include <Arduino.h>

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


/* -------------------------- Display Config Initialisation -------------------- */
//    Assume we have four 64x32 panels daizy-chained and ESP32 attached to the bottom right corner
#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

#define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
#define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW
#define PANEL_CHAIN NUM_ROWS*NUM_COLS    // total number of panels chained one to another

// Change this to your needs, for details on VirtualPanel pls see ChainedPanels example
#define SERPENT false
#define TOPDOWN false

// Virtual Panl dimensions - our combined panel would be a square 4x4 modules with a combined resolution of 128x128 pixels
#define VPANEL_W PANEL_RES_X*NUM_COLS // Kosso: All Pattern files have had the MATRIX_WIDTH and MATRIX_HEIGHT replaced by these.
#define VPANEL_H PANEL_RES_Y*NUM_ROWS //

// Kosso added: Button with debounce
#define BTN_PIN 0 // Pattern advance. Using EPS32 Boot button.
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// The palettes are set to change every 60 seconds. 

// Kosso added: Non-volatile memory to save last pattern index.
#include <Preferences.h>
Preferences preferences;
int lastPattern = 0;


/* -------------------------- Class Initialisation -------------------------- */
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#include <FastLED.h>  // Used for some mathematics calculations and effects.

// placeholder for the matrix object
MatrixPanel_I2S_DMA *matrix = nullptr;

// placeholder for the virtual display object
VirtualMatrixPanel  *virtualDisp = nullptr;

// Aurora related
#include "Effects.h"
Effects effects;

#include "Drawable.h"
#include "Playlist.h"
//#include "Geometry.h"
#include "Patterns.h"
Patterns patterns;

/* -------------------------- Some variables -------------------------- */
unsigned long ms_current  = 0;
unsigned long ms_previous = 0;
unsigned long ms_animation_max_duration = 20000; // 10 seconds
unsigned long next_frame = 0;

void listPatterns();

void setup()
{
  // Setup serial interface
  Serial.begin(115200);
  delay(250);

  // Added a button to manually advance the pattern index.
  pinMode(BTN_PIN, INPUT);
  // For saving last pattern index. TO reboot with same.
  preferences.begin("RGBMATRIX", false);
  lastPattern = preferences.getInt("lastPattern", 0);

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

  // let's adjust default brightness to about 75%
  matrix->setBrightness8(96);    // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if( not matrix->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

  // create VirtualDisplay object based on our newly created dma_display object
  virtualDisp = new VirtualMatrixPanel((*matrix), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, SERPENT, TOPDOWN);

  Serial.println("**************** Starting Aurora Effects Demo ****************");

  Serial.print("MATRIX_WIDTH: ");  Serial.println(PANEL_RES_X*PANEL_CHAIN);
  Serial.print("MATRIX_HEIGHT: "); Serial.println(PANEL_RES_Y);

#ifdef VPANEL_W
  Serial.println("VIRTUAL PANEL WIDTH " + String(VPANEL_W));
  Serial.println("VIRTUAL PANEL HEIGHT " + String(VPANEL_H));
#endif

   // setup the effects generator
  effects.Setup();

  delay(500);
  Serial.println("Effects being loaded: ");
  listPatterns();

  Serial.println("LastPattern index: " + String(lastPattern));
  
  patterns.setPattern(lastPattern); //   // simple noise
  patterns.start();     

  Serial.print("Starting with pattern: ");
  Serial.println(patterns.getCurrentPatternName());

  preferences.end();

}


void patternAdvance(){
    // Go to next pattern in the list (se Patterns.h)
    patterns.stop();
    patterns.moveRandom(1);
    //patterns.move(1);
    patterns.start();  
    // Select a random palette as well
    effects.RandomPalette();
    Serial.print("Changing pattern to:  ");
    Serial.println(patterns.getCurrentPatternName());
    //Serial.println(patterns.getPatternIndex());
    //lastPattern = patterns.getPatternIndex();
    // Save last index.
    preferences.begin("RGBMATRIX", false);
    preferences.putInt("lastPattern", lastPattern);
    preferences.end();
   
}

void loop()
{
    // Boot button Pattern advance with debounce
    int reading = digitalRead(BTN_PIN);
    if (reading != lastButtonState) {
      lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
        if (buttonState == LOW) {
          Serial.println("NEXT PATTERN ...");
          patternAdvance();
        }
      }
    }
    lastButtonState = reading;
    // end button debounce
  
    ms_current = millis();
      
    if ( (ms_current - ms_previous) > ms_animation_max_duration ) 
    {
       patternAdvance();
       // just auto-change the palette
       effects.RandomPalette();
       ms_previous = ms_current;
    }
 
    if ( next_frame < ms_current)
      next_frame = patterns.drawFrame() + ms_current; 
       
}


void listPatterns() {
  patterns.listPatterns();
}
