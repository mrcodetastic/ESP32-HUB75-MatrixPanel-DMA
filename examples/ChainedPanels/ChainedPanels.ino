/*******************************************************************
    This is the PatternPlasma Demo adopted for use with multiple
    displays arranged in a non standard order

    What is a non standard order?
    
    When you connect multiple panels together, the library treats the
    multiple panels as one big panel arranged horizontally. Arranging
    the displays like this would be a standard order.

    [ 4 ][ 3 ][ 2 ][ 1 ]  (ESP32 is connected to 1)

    If you wanted to arrange the displays vertically, or in rows and 
    columns this example might be able to help.

    [ 4 ][ 3 ]
    [ 2 ][ 1 ]

    It creates a virtual screen that you draw to in the same way you would
    the matrix, but it will look after mapping it back to the displays.

    -----------
    Steps to use
    -----------

    1) In ESP32-RGB64x32MatrixPanel-I2S-DMA.h:

    - Set the MATRIX_HEIGHT to be the y resolution of the physical chained panels in a line (if the panels are 32 x 16, set it to be 16)
    - Set the MATRIX_WIDTH to be the sum of the x resolution of all the physical chained panels (i.e. If you have 4 x (32px w x 16px h) panels, 32x4 = 128) 

    2) In the sketch:
    
    - Set values for NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y. There are comments beside them
    explaining what they are in more detail.
    - Other than where the matrix is defined and matrix.begin in the setup, you should now be using the virtual display
    for everything (drawing pixels, writing text etc). You can do a find and replace of all calls if it's an existing sketch
    (just make sure you don't replace the definition and the matrix.begin)
    - If the sketch makes use of MATRIX_HEIGHT or MATRIX_WIDTH, these will need to be replaced with the width and height
    of your virtual screen. Either make new defines and use that, or you can use virtualDisp.width() or .height()

    Parts:
    ESP32 D1 Mini * - https://s.click.aliexpress.com/e/_dSi824B
    ESP32 I2S Matrix Shield (From my Tindie) = https://www.tindie.com/products/brianlough/esp32-i2s-matrix-shield/

 *  * = Affilate

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/


    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

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
 

/* Include FastLED, DMA Display (for module driving), and VirtualMatrixPanel
 * to enable easy painting & graphics for a chain of individual LED modules. 
 */

#define NUM_ROWS 2 // Number of rows panels in your overall display
#define NUM_COLS 1 // number of panels in each row

#define PANEL_RES_X 64 // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32 // Number of pixels tall of each INDIVIDUAL panel module.

/* Create physical module driver class AND virtual (chained) display class. */
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
RGB64x32MatrixPanel_I2S_DMA dma_display;
VirtualMatrixPanel          virtualDisp(dma_display, NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y);

/* FastLED Global Variables for Pattern */
#include <FastLED.h> 
int time_counter = 0;
int cycles = 0;
CRGBPalette16 currentPalette;
CRGB currentColor;

CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void setup() {
  
  Serial.begin(115200);
  
  Serial.println(""); Serial.println(""); Serial.println("");
  Serial.println("*****************************************************");
  Serial.println(" HELLO !");
  Serial.println("*****************************************************");

#ifdef USE_CUSTOM_PINS
  dma_display.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
#else
  dma_display.begin();
#endif
 
  // fill the screen with 'black'
  //dma_display.fillScreen(dma_display.color444(0, 0, 0));
  virtualDisp.fillScreen(virtualDisp.color444(0, 0, 0));

  // Set current FastLED palette
  currentPalette = RainbowColors_p;

  virtualDisp.drawRect(4, 4, PANEL_RES_X * NUM_COLS - 8, PANEL_RES_Y * NUM_ROWS - 8, virtualDisp.color565(255, 255, 255));
  delay(5000);

}


void loop() {
  

   for (int x = 0; x <  virtualDisp.width(); x++) {
            for (int y = 0; y <  virtualDisp.height(); y++) {
                int16_t v = 0;
                uint8_t wibble = sin8(time_counter);
                v += sin16(x * wibble * 3 + time_counter);
                v += cos16(y * (128 - wibble)  + time_counter);
                v += sin16(y * x * cos8(-time_counter) / 8);

				currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127); //, brightness, currentBlendType);
				virtualDisp.drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
				
            }
        }

        time_counter += 1;
        cycles++;

        if (cycles >= 2048) {
            time_counter = 0;
            cycles = 0;
        }
      

} // end loop