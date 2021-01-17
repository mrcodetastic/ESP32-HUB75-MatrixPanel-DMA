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
 
#define USE_CUSTOM_PINS // uncomment to use custom pins, then provide below

/*
*/

/*
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
 
*/
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



#include <ESP-32_HUB75_32x16MatrixPanel-I2S-DMA.h>
#include <Wire.h>

MatrixPanel_I2S_DMA matrixDisplay;
QuarterScanMatrixPanel display(matrixDisplay);

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
  Serial.println(" MatrixDisplay 32x16 !");
  Serial.println("*****************************************************");

#ifdef USE_CUSTOM_PINS
  matrixDisplay.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN );  // setup the LED matrix
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