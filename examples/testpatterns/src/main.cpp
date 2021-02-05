// How to use this library with a FM6126 panel, thanks goes to:
// https://github.com/hzeller/rpi-rgb-led-matrix/issues/746

/*
// IDF
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
*/
#include <Arduino.h>
#include "xtensa/core-macros.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

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

#define R1 25
#define G1 26
#define BL1 27
#define R2 14 // 21 SDA
#define G2 12 // 22 SDL
#define BL2 13
#define CH_A 23
#define CH_B 19
#define CH_C 5
#define CH_D 17
#define CH_E 32 // assign to any available pin if using two panels or 64x64 panels with 1/32 scan
#define CLK 16
#define LAT 4
#define OE 15


#define M_WIDTH 256
#define M_HEIGHT 64
#define PATTERN_DELAY 2000

#define NUM_LEDS M_WIDTH*M_HEIGHT

//#define USE_FASTLINES

MatrixPanel_I2S_DMA *matrix = nullptr;

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

//
CRGB *ledbuff;
//

unsigned long t1, t2, s1=0, s2=0, s3=0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

void setup(){

  Serial.begin(BAUD_RATE);
  Serial.println("Starting pattern test...");

  HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  HUB75_I2S_CFG mxconfig(64, 64, 4, _pins, HUB75_I2S_CFG::FM6126A);
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;

  matrix = new MatrixPanel_I2S_DMA(mxconfig);

  matrix->begin();
  matrix->setBrightness8(255);
  //matrix->setLatBlanking(2);

  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  // allocate buffer for some tests
  buffclear(ledbuff);

}


void loop(){

  Serial.printf("Cycle: %d\n", ++cycles);

  Serial.print("Estimating clearScreen() - ");
  ccount1 = XTHAL_GET_CCOUNT();
  matrix->clearScreen();
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  Serial.printf("%d ticks\n", ccount1);
  delay(PATTERN_DELAY);

/*
// Power supply tester
// slowly fills matrix with white, stressing PSU
  for (int y=0; y!=M_HEIGHT; ++y){
    for (int x=0; x!=M_WIDTH; ++x){
      matrix->drawPixelRGB888(x, y, 255,255,255);
      //matrix->drawPixelRGB888(x, y-1, 255,0,0);       // pls, be gentle :)
      delay(10);
    }
  }
  delay(5000);
*/


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


  Serial.print("Estimating full-screen fillrate with looped drawPixelRGB888(): ");
  y = M_HEIGHT;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    --y;
    uint16_t x = M_WIDTH;
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
    if (i%M_WIDTH==0)
      color3+=255/M_HEIGHT;

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

#ifdef USE_FASTLINES
  // Fillrate for fillRect() function
  Serial.print("Estimating fullscreen fillrate with fillRect() time: ");
  t1 = micros();
  matrix->fillRect(0, 0, M_WIDTH, M_HEIGHT, 0, 224, 0);
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
    }while(x < M_WIDTH);
    y+=8;
    toggle = !toggle;
    x = toggle ? 8 : 0;
  }while(y < M_HEIGHT);
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
    } while(++y != M_HEIGHT);
    x+=2;
  } while(x != M_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

#ifdef USE_FASTLINES
  Serial.println("Estimating V-lines with vlineDMA(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastVLine(x, y, M_HEIGHT, color1, color2, color3);
    x+=2;
  } while(x != M_WIDTH);
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
    matrix->fillRect(x, y, 1, M_HEIGHT, color1, color2, color3);
    x+=2;
  } while(x != M_WIDTH);
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
    } while(++x != M_WIDTH);
    y+=2;
  } while(y != M_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

#ifdef USE_FASTLINES
  Serial.println("Estimating H-lines with hlineDMA(): ");
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastHLine(x, y, M_WIDTH, color1, color2, color3);
    y+=2;
  } while(y != M_HEIGHT);
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
    matrix->fillRect(x, y, M_WIDTH, 1, color1, color2, color3);
    y+=2;
  } while(y != M_HEIGHT);
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
  uint16_t y = M_HEIGHT;
  do {
    --y;
    uint16_t x = M_WIDTH;
    do {
      --x;
        uint16_t _pixel = y * M_WIDTH + x;
        matrix->drawPixelRGB888( x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while(x);
  } while(y);
}
//

/**
 *  The one for 256+ matrixes
 *  otherwise this:
 *    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {}
 *  turns into an infinite loop
 */
uint16_t XY16( uint16_t x, uint16_t y)
{ 
  if (x<M_WIDTH && y < M_HEIGHT){
    return (y * M_WIDTH) + x; // everything offset by one to capute out of bounds stuff - never displayed by ShowFrame()
  } else {
    return 0;
  }
}
