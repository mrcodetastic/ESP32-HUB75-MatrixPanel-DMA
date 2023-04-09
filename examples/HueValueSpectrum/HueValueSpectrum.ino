#define PANEL_RES_X 64    // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;

void setup() {

  Serial.begin(112500);


  HUB75_I2S_CFG::i2s_pins _pins={
    25, //R1_PIN, 
    26, //G1_PIN, 
    27, //B1_PIN, 
    14, //R2_PIN, 
    12, //G2_PIN, 
    13, //B2_PIN, 
    23, //A_PIN, 
    19, //B_PIN, 
    5, //C_PIN, 
    17, //D_PIN, 
    18, //E_PIN,
    4, //LAT_PIN, 
    15, //OE_PIN, 
    16, //CLK_PIN
  };
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X, // Module width
    PANEL_RES_Y, // Module height
    PANEL_CHAIN //, // chain length
    //_pins // pin mapping -- uncomment if providing own custom pin mapping as per above.
  );
  //mxconfig.clkphase = false;
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->clearScreen();
}

void loop() {
  // Canvas loop
  float t = (float) ((millis()%4000)/4000.f);
  float tt = (float) ((millis()%16000)/16000.f);

  for(int x = 0; x < (PANEL_RES_X*PANEL_CHAIN); x++)
  {
    // calculate the overal shade
    float f = (((sin(tt-(float)x/PANEL_RES_Y/32.)*2.f*PI)+1)/2)*255;
    // calculate hue spectrum into rgb
    float r = max(min(cosf(2.f*PI*(t+((float)x/PANEL_RES_Y+0.f)/3.f))+0.5f,1.f),0.f);
    float g = max(min(cosf(2.f*PI*(t+((float)x/PANEL_RES_Y+1.f)/3.f))+0.5f,1.f),0.f);
    float b = max(min(cosf(2.f*PI*(t+((float)x/PANEL_RES_Y+2.f)/3.f))+0.5f,1.f),0.f);

    // iterate pixels for every row
    for(int y = 0; y < PANEL_RES_Y; y++){
      if(y*2 < PANEL_RES_Y){
        // top-middle part of screen, transition of value
        float t = (2.f*y+1)/PANEL_RES_Y;
        dma_display->drawPixelRGB888(x,y,
          (r*t)*f,
          (g*t)*f,
          (b*t)*f
        );
      }else{
        // middle to bottom of screen, transition of saturation
        float t = (2.f*(PANEL_RES_Y-y)-1)/PANEL_RES_Y;
        dma_display->drawPixelRGB888(x,y,
          (r*t+1-t)*f,
          (g*t+1-t)*f,
          (b*t+1-t)*f
        );
      }
    }
  }
}
