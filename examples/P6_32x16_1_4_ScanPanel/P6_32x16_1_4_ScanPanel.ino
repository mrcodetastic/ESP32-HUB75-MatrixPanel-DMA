/*************************************************************************
 * Contributor: https://github.com/mrRobot62  
 *
 * Description: 
 * 
 * The underlying implementation of the ESP32-HUB75-MatrixPanel-I2S-DMA only
 * supports output to 1/16 or 1/32 scan panels (two scan parallel scan lines)
 * this is fixed and cannot be changed.
 *
 * However, it is possible to connect 1/4 scan panels to this same library and
 * 'trick' the output to work correctly on these panels by way of adjusting the
 * pixel co-ordinates that are 'sent' to the ESP32-HUB75-MatrixPanel-I2S-DMA
 * library (in this example, it is the 'dmaOutput' class).
 * 
 * This is done by way of the 'QuarterScanMatrixPanel.h' class that sends
 * adjusted x,y co-ordinates to the underlying ESP32-HUB75-MatrixPanel-I2S-DMA 
 * library's drawPixel routine.
 *
 * Refer to the 'getCoords' function within 'QuarterScanMatrixPanel.h'
 *
 **************************************************************************/

// uncomment to use custom pins, then provide below
#define USE_CUSTOM_PINS 

/* Pin 1,3,5,7,9,11,13,15 */
#define R1_PIN  25
#define B1_PIN  27
#define R2_PIN  14
#define B2_PIN  13
#define A_PIN  23
#define C_PIN  5
#define CLK_PIN  16
#define OE_PIN  15

/* Pin 2,6,10,12,14 */
#define G1_PIN  26
#define G2_PIN  12
#define B_PIN  19
#define D_PIN  17
#define LAT_PIN  4
#define E_PIN  -1 // required for 1/32 scan panels

#include "QuarterScanMatrixPanel.h" // Virtual Display to re-map co-ordinates such that they draw correctly on a32x16 1/4 Scan panel 
#include <Wire.h>

/* 
 * Below is an is the 'legacy' way of initialising the MatrixPanel_I2S_DMA class.
 * i.e. MATRIX_WIDTH and MATRIX_HEIGHT are modified by compile-time directives.
 * By default the library assumes a single 64x32 pixel panel is connected.
 *
 * Refer to the example '2_PatternPlasma' on the new / correct way to setup this library
 * for different resolutions / panel chain lengths within the sketch 'setup()'.
 * 
 */
MatrixPanel_I2S_DMA dmaOutput;

// Create virtual 1/2 to 1/4 scan pixel co-ordinate mapping class.
QuarterScanMatrixPanel display(dmaOutput);

#include <FastLED.h>

int time_counter = 0;
int cycles = 0;

CRGBPalette16 currentPalette;
CRGB currentColor;


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

typedef struct Matrix {
  uint8_t x;
  uint8_t y;
} Matrix;

Matrix matrix;


void testSimpleChars(uint16_t timeout) {

  /** drawChar() **/
  Serial.println("draw chars with drawChar()");
  display.fillScreen(display.color444(0,0,0));

  uint16_t myFGColor = display.color565(180,0,0);
  uint16_t myBGColor = display.color565(0,50,0);
  display.fillScreen(display.color444(0,0,0));
  display.drawChar(0,0,'X',myFGColor, myFGColor,1);
  display.drawChar(16,1,'Y',myFGColor, myBGColor,1);
  display.drawChar(3,9,'Z',myFGColor, myFGColor,1);
  display.drawChar(16,9,'4',display.color565(0,220,0), myBGColor,1);
  delay(timeout);

}

void testSimpleCharString(uint16_t timeout) {
  uint8_t x,y,w,h;
  w = 6; h=8;
  x = 0; y=0;
  display.fillScreen(display.color444(0,0,0));
  display.setTextFGColor(display.color565(0,60,180));
  display.setCursor(x,y); display.write('L');
  display.setCursor(x+w,y); display.write('u');
  display.setCursor(x+(2*w),y); display.write('n');
  display.setCursor(x+(3*w),y); display.write('a');
  display.setTextFGColor(display.color565(180,60,140));
  display.setCursor(x+(4*w),y); display.write('X');

  delay(timeout);

}

void testTextString(uint16_t timeout) {
  display.fillScreen(display.color444(0,0,0));
  display.setTextFGColor(display.color565(0,60,255));

  display.setCursor(0,5);
  display.write("HURRA");
  delay(timeout);
}

void testWrapChar(const char c, uint16_t speed, uint16_t timeout) {
  display.setTextWrap(true);
  for (uint8_t i = 32; i > 0; i--) {
    display.fillScreen(display.color444(0,0,0));
    display.setCursor(i, 5);
    display.write(c);
    delay(speed);
  }
  delay(timeout);
}

void testScrollingChar(const char c, uint16_t speed, uint16_t timeout) {
  Serial.println("Scrolling Char");
  uint16_t myFGColor = display.color565(180,0,0);
  uint16_t myBGColor = display.color565(60,120,0);
  display.fillScreen(display.color444(0,0,0));
  display.setTextWrap(true);
  // from right to left with wrap
  display.scrollChar(31,5,c, myFGColor, myFGColor, 1, speed);
  // left out with wrap
  delay(500);
  display.scrollChar(0,5,c, myBGColor, myBGColor, 1, speed);

  delay(timeout);
}

void testScrollingText(const char *str, uint16_t speed, uint16_t timeout) {
  Serial.println("Scrolling Text as loop");
  // pre config
  uint16_t red = display.color565(255,0,100);
  uint16_t blue100 = display.color565(0,0,100);
  uint16_t black = display.color565(0,0,0);
  uint16_t green = display.color565(0,255,0);
  uint16_t green150 = display.color565(0,150,0);

  display.fillScreen(display.color565(0,0,0));
  display.setCursor(31,5);
  display.setScrollDir(1);

  /** black background **/
  display.setTextFGColor(green150);
  display.scrollText("** Welcome **", speed);
  display.fillScreen(black);
  delay(timeout / 2) ;

  /** scrolling with colored background */
  display.fillRect(0,4,VP_WIDTH,8,blue100);
  // scrolling, using default pixels size = length of string (not used parameter pixels)
  display.setTextFGColor(red);
  display.setTextBGColor(blue100);
  display.scrollText(str, speed);
  delay(timeout / 2) ;

  // same as above but now from left to right
  display.setScrollDir(0);
  display.setTextFGColor(blue100);
  display.setTextBGColor(red);
  display.fillRect(0,4,VP_WIDTH,8,red);
  display.scrollText(str, speed, 0);

  delay(timeout);
  display.fillScreen(black);
  display.setTextFGColor(red);


}

void setup() {
  
  Serial.begin(115200);
  delay(500);
  Serial.println("*****************************************************");
  Serial.println(" dmaOutput 32x16 !");
  Serial.println("*****************************************************");

#ifdef USE_CUSTOM_PINS
  dmaOutput.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
#else
  display.begin(true); // init buffers
#endif
 
  // fill the screen with 'black'
  display.fillScreen(display.color444(0, 0, 0));

  // Set current FastLED palette
  currentPalette = RainbowColors_p;
    // display.fillScreen(display.color565(0, 0, 0));  
}

void loop() {
  display.fillScreen(display.color444(0, 0, 0));
  //testSimpleChars(1500);
  //testSimpleCharString (1500); 
  testTextString(2000);
  // length = 16 bytes without \0
  //testWrapChar('A', 250, 1500);
  //testScrollingChar('X', 250, 2000);
  testScrollingText("Scrolling 16x32", 100, 2000);
} // end loop