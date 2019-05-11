/* ------------------------- CUSTOM GPIO PIN MAPPING ------------------------- */
#define R1_PIN 18
#define G1_PIN 25
#define B1_PIN 5
#define R2_PIN 17
#define G2_PIN 26
#define B2_PIN 16
#define A_PIN 14
#define B_PIN 27 
#define C_PIN 12
#define D_PIN 4
#define E_PIN -1
#define LAT_PIN 13
#define OE_PIN 15
#define CLK_PIN 2

/* -------------------------- Display Config Initialisation -------------------- */

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 32

/* -------------------------- Class Initialisation -------------------------- */
#include <ESP32-RGB64x32MatrixPanel-I2S-DMA.h>
RGB64x32MatrixPanel_I2S_DMA matrix;

#include <FastLED.h>

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
unsigned long ms_animation_max_duration = 10000; // 10 seconds
unsigned long next_frame = 0;

void setup()
{
  // Setup serial interface
  Serial.begin(115200);
  delay(250);
  matrix.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix

  Serial.println("**************** Starting Aurora Effects Demo ****************");

   // setup the effects generator
  effects.Setup();

  delay(500);
  Serial.println("Effects being loaded: ");
  listPatterns();


  patterns.setPattern(0); //   // simple noise
  patterns.start();     

  Serial.print("Starting with pattern: ");
  Serial.println(patterns.getCurrentPatternName());

}

void loop()
{
    // menu.run(mainMenuItems, mainMenuItemCount);  
    ms_current = millis();

  
    if ( (ms_current - ms_previous) > ms_animation_max_duration ) 
    {
     //  patterns.moveRandom(1);

       patterns.stop();      
       patterns.move(1);
       patterns.start();  
 
       
       Serial.print("Changing pattern to:  ");
       Serial.println(patterns.getCurrentPatternName());
        
       ms_previous = ms_current;

       // Select a random palette as well
       //effects.RandomPalette();
    }
 
    if ( next_frame < ms_current)
      next_frame = patterns.drawFrame() + ms_current; 
       
}


void listPatterns() {
  patterns.listPatterns();
}
