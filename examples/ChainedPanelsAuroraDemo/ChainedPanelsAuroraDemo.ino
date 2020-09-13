/* ------------------------- CUSTOM GPIO PIN MAPPING ------------------------- */
// Kosso's ESP32 Matrix PCB pins
#define R1_PIN 2
#define G1_PIN 15
#define B1_PIN 4
#define R2_PIN 16
#define G2_PIN 13
#define B2_PIN 17
#define A_PIN 5
#define B_PIN 18 
#define C_PIN 19
#define D_PIN 27
#define E_PIN -1
#define LAT_PIN 21
#define OE_PIN 23
#define CLK_PIN 22


/* -------------------------- Display Config Initialisation -------------------- */
#define MATRIX_WIDTH 128  // Overall matrix dimensions if laid out end-to-end.
#define MATRIX_HEIGHT 32

#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

#define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
#define NUM_COLS 1 // Number of INDIVIDUAL PANELS per ROW

// Virtual Panl dimensions
#define VPANEL_W 64 // Kosso: All Pattern files have had the MATRIX_WIDTH and MATRIX_HEIGHT replaced by these.
#define VPANEL_H 64 //

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
//#include <ESP32-RGB64x32MatrixPanel-I2S-DMA.h>
//RGB64x32MatrixPanel_I2S_DMA matrix;

#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
RGB64x32MatrixPanel_I2S_DMA matrix;
// Added support for 'Chained' virtual panel configurations.
VirtualMatrixPanel    virtualDisp(matrix, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, true);

#include <FastLED.h>  // Used for some mathematics calculations and effects.

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
unsigned long ms_animation_max_duration = 60000; // 10 seconds
unsigned long next_frame = 0;

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
  
  matrix.setPanelBrightness(30);
  matrix.setMinRefreshRate(200);
  
    
  matrix.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
  
  virtualDisp.fillScreen(virtualDisp.color444(0, 0, 0));
  
  Serial.println("**************** Starting Aurora Effects Demo ****************");

  Serial.println("MATRIX_WIDTH " + String(MATRIX_WIDTH));
  Serial.println("MATRIX_HEIGHT " + String(MATRIX_HEIGHT));

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
    patterns.move(1);
    patterns.start();  
    // Select a random palette as well
    effects.RandomPalette();
    Serial.print("Changing pattern to:  ");
    Serial.println(patterns.getCurrentPatternName());
    Serial.println(patterns.getPatternIndex());
    lastPattern = patterns.getPatternIndex();
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
       // patternAdvance();
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
