// How to use this library with a FM6126 panel, thanks goes to:
// https://github.com/hzeller/rpi-rgb-led-matrix/issues/746

#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#include <Arduino.h>
#include "xtensa/core-macros.h"
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif
#include "main.h"

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
#define CH_E -1 // assign to any available pin if using panels with 1/32 scan
#define CLK 16
#define LAT 4
#define OE 15
*/

// Configure for your panel(s) as appropriate!
#define PIN_E 32
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64         // Panel height of 64 will required PIN_E to be defined.

#ifdef VIRTUAL_PANE
 #define PANELS_NUMBER 4         // Number of chained panels, if just a single panel, obviously set to 1
#else
 #define PANELS_NUMBER 2         // Number of chained panels, if just a single panel, obviously set to 1
#endif

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH*PANE_HEIGHT

#ifdef VIRTUAL_PANE
 #define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
 #define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW
 #define PANEL_CHAIN NUM_ROWS*NUM_COLS    // total number of panels chained one to another
 // Change this to your needs, for details on VirtualPanel pls read the PDF!
 #define SERPENT true
 #define TOPDOWN false
#endif


#ifdef VIRTUAL_PANE
VirtualMatrixPanel *matrix = nullptr;
MatrixPanel_I2S_DMA *chain = nullptr;
#else
MatrixPanel_I2S_DMA *matrix = nullptr;
#endif
// patten change delay
#define PATTERN_DELAY 2000

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

// gradient buffer
CRGB *ledbuff;
//

unsigned long t1, t2, s1=0, s2=0, s3=0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

const char *str = "* ESP32 I2S DMA *";

void setup(){

  Serial.begin(BAUD_RATE);
  Serial.println("Starting pattern test...");

  // redefine pins if required
  //HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);

  mxconfig.gpio.e = PIN_E;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;   // for panels using FM6126A chips

#ifndef VIRTUAL_PANE
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(255);
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(255);
  // create VirtualDisplay object based on our newly created dma_display object
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
#endif

  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  // allocate buffer for some tests
  buffclear(ledbuff);
}

uint8_t wheelval = 0;
void loop(){

  Serial.printf("Cycle: %d\n", ++cycles);

#ifndef NO_GFX
  drawText(wheelval++);
#endif

  Serial.print("Estimating clearScreen() - ");
  ccount1 = XTHAL_GET_CCOUNT();
  matrix->clearScreen();
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  Serial.printf("%d ticks\n", ccount1);
  delay(PATTERN_DELAY);

/*
// Power supply tester
// slowly fills matrix with white, stressing PSU
  for (int y=0; y!=PANE_HEIGHT; ++y){
    for (int x=0; x!=PANE_WIDTH; ++x){
      matrix->drawPixelRGB888(x, y, 255,255,255);
      //matrix->drawPixelRGB888(x, y-1, 255,0,0);       // pls, be gentle :)
      delay(10);
    }
  }
  delay(5000);
*/

#ifndef VIRTUAL_PANE
  // simple solid colors
  Serial.println("Fill screen: RED");
  matrix->fillScreenRGB888(255, 0, 0);
  delay(PATTERN_DELAY);
  Serial.println("Fill screen: GREEN");
  matrix->fillScreenRGB888(0, 255, 0);
  delay(PATTERN_DELAY);
  Serial.println("Fill screen: BLUE");
  matrix->fillScreenRGB888(0, 0, 255);
  delay(PATTERN_DELAY);
#endif

  for (uint8_t i=5; i; --i){
    Serial.print("Estimating single drawPixelRGB888(r, g, b) ticks: ");
    color1 = random8();
    ccount1 = XTHAL_GET_CCOUNT();
    matrix->drawPixelRGB888(i,i, color1, color1, color1);
    ccount1 = XTHAL_GET_CCOUNT() - ccount1;
    Serial.printf("%d ticks\n", ccount1);
  }

// Clearing CRGB ledbuff
  Serial.print("Estimating ledbuff clear time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  buffclear(ledbuff);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n\n", t2, ccount1);

#ifndef VIRTUAL_PANE
  // Bare fillscreen(r, g, b)
  Serial.print("Estimating fillscreenRGB888(r, g, b) time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  matrix->fillScreenRGB888(64, 64, 64);   // white
  ccount2 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  s1+=t2;
  Serial.printf("%lu us, avg: %lu, ccnt: %d\n", t2, s1/cycles, ccount2);
  delay(PATTERN_DELAY);
#endif

  Serial.print("Estimating full-screen fillrate with looped drawPixelRGB888(): ");
  y = PANE_HEIGHT;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
        matrix->drawPixelRGB888( x, y, 0, 0, 0);
    } while(x);
  } while(y);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);



// created random color gradient in ledbuff
  uint8_t color1 = 0;
  uint8_t color2 = random8();
  uint8_t color3 = 0;

  for (uint16_t i = 0; i<NUM_LEDS; ++i){
    ledbuff[i].r=color1++;
    ledbuff[i].g=color2;
    if (i%PANE_WIDTH==0)
      color3+=255/PANE_HEIGHT;

    ledbuff[i].b=color3;
  }
//

//
  Serial.print("Estimating ledbuff-to-matrix fillrate with drawPixelRGB888(), time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  mxfill(ledbuff);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  s2+=t2;
  Serial.printf("%lu us, avg: %lu, %d ticks:\n", t2, s2/cycles, ccount1);
  delay(PATTERN_DELAY);
//

#ifndef NO_FAST_FUNCTIONS
  // Fillrate for fillRect() function
  Serial.print("Estimating fullscreen fillrate with fillRect() time: ");
  t1 = micros();
  matrix->fillRect(0, 0, PANE_WIDTH, PANE_HEIGHT, 0, 224, 0);
  t2 = micros()-t1;
  Serial.printf("%lu us\n", t2);
  delay(PATTERN_DELAY);


  Serial.print("Chessboard with fillRect(): ");  // шахматка
  matrix->fillScreen(0);
  x =0, y = 0;
  color1 = random8();
  color2 = random8();
  color3 = random8();
  bool toggle=0;
  t1 = micros();
  do {
    do{
      matrix->fillRect(x, y, 8, 8, color1, color2, color3);
      x+=16;
    }while(x < PANE_WIDTH);
    y+=8;
    toggle = !toggle;
    x = toggle ? 8 : 0;
  }while(y < PANE_HEIGHT);
  t2 = micros()-t1;
  Serial.printf("%lu us\n", t2);
  delay(PATTERN_DELAY);
#endif

// ======== V-Lines ==========
  Serial.println("Estimating V-lines with drawPixelRGB888(): ");  //
  matrix->fillScreen(0);
  color1 = random8();
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    y=0;
    do{
      matrix->drawPixelRGB888(x, y, color1, color2, color3);
    } while(++y != PANE_HEIGHT);
    x+=2;
  } while(x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

#ifndef NO_FAST_FUNCTIONS
  Serial.println("Estimating V-lines with vlineDMA(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastVLine(x, y, PANE_HEIGHT, color1, color2, color3);
    x+=2;
  } while(x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating V-lines with fillRect(): ");  //
  matrix->fillScreen(0);
  color1 = random8();
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->fillRect(x, y, 1, PANE_HEIGHT, color1, color2, color3);
    x+=2;
  } while(x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);
#endif



// ======== H-Lines ==========
  Serial.println("Estimating H-lines with drawPixelRGB888(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    x=0;
    do{
      matrix->drawPixelRGB888(x, y, color1, color2, color3);
    } while(++x != PANE_WIDTH);
    y+=2;
  } while(y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

#ifndef NO_FAST_FUNCTIONS
  Serial.println("Estimating H-lines with hlineDMA(): ");
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastHLine(x, y, PANE_WIDTH, color1, color2, color3);
    y+=2;
  } while(y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating H-lines with fillRect(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->fillRect(x, y, PANE_WIDTH, 1, color1, color2, color3);
    y+=2;
  } while(y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);
#endif




  Serial.println("\n====\n");

  // take a rest for a while
  delay(10000);
}


void buffclear(CRGB *buf){
  memset(buf, 0x00, NUM_LEDS * sizeof(CRGB)); // flush buffer to black  
}

void IRAM_ATTR mxfill(CRGB *leds){
  uint16_t y = PANE_HEIGHT;
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
        uint16_t _pixel = y * PANE_WIDTH + x;
        matrix->drawPixelRGB888( x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while(x);
  } while(y);
}
//

/**
 *  The one for 256+ matrices
 *  otherwise this:
 *    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {}
 *  turns into an infinite loop
 */
uint16_t XY16( uint16_t x, uint16_t y)
{ 
  if (x<PANE_WIDTH && y < PANE_HEIGHT){
    return (y * PANE_WIDTH) + x;
  } else {
    return 0;
  }
}

#ifdef NO_GFX
void drawText(int colorWheelOffset){}
#else
void drawText(int colorWheelOffset){
  // draw some text
  matrix->setTextSize(1);     // size 1 == 8 pixels high
  matrix->setTextWrap(false); // Don't wrap at end of line - will do ourselves

  matrix->setCursor(5, 5);    // start at top left, with 5,5 pixel of spacing
  uint8_t w = 0;

  for (w=0; w<strlen(str); w++) {
    matrix->setTextColor(colorWheel((w*32)+colorWheelOffset));
    matrix->print(str[w]);
  }
}
#endif

uint16_t colorWheel(uint8_t pos) {
  if(pos < 85) {
    return matrix->color565(pos * 3, 255 - pos * 3, 0);
  } else if(pos < 170) {
    pos -= 85;
    return matrix->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return matrix->color565(0, pos * 3, 255 - pos * 3);
  }
}
