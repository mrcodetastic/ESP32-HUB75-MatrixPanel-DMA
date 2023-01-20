#define PANEL_RES_X 64    // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

#define NO_CIE1931
#define PIXEL_COLOUR_DEPTH_BITS      8
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN -1 // required for 1/32 scan panels, esp for 64x64 or 128x64 displays
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

uint32_t lastMillis;

void setup() {
  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X, // Module width
    PANEL_RES_Y, // Module height
    1, // chain length
    _pins // pin mapping
  );
  //mxconfig.gpio.e = 18;
  //mxconfig.clkphase = false;
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->clearScreen();
}

void loop() {
  // Canvas loop
  float t = (float)(millis()%4000)/4000.f;
  for(int x = 0; x < PANEL_RES_X; x++){
    float r =max(min(cosf(2.f*PI*(t+(float)x/PANEL_RES_X+0.f/3.f))+0.5f,1.f),0.f);
    float g =max(min(cosf(2.f*PI*(t+(float)x/PANEL_RES_X+1.f/3.f))+0.5f,1.f),0.f);
    float b =max(min(cosf(2.f*PI*(t+(float)x/PANEL_RES_X+2.f/3.f))+0.5f,1.f),0.f);
    for(int y = 0; y < PANEL_RES_Y; y++)
      if(y*2 < PANEL_RES_Y){
        float t = (2.f*y)/PANEL_RES_Y;
        dma_display->drawPixelRGB888(x,y,
          (r*t)*255,
          (g*t)*255,
          (b*t)*255
        );
      }else{
        float t = (2.f*(PANEL_RES_Y-y))/PANEL_RES_Y;
        dma_display->drawPixelRGB888(x,y,
          (r*t+1-t)*255,
          (g*t+1-t)*255,
          (b*t+1-t)*255
        );
      }
        
  }
}
