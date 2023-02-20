#define PANEL_RES_X 128    // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 64     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another
#define USE_FLOATHACK      // To boost float performance, comment if this doesn't work. 

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;

// inspired by
// https://en.wikipedia.org/wiki/Fast_inverse_square_root
#ifdef USE_FLOATHACK
// cast float as int32_t
  int32_t intfloat(float n){ return *(int32_t *)&n; }
// cast int32_t as float
  float floatint(int32_t n){ return *(float *)&n; }
// fast approx sqrt(x)
  float floatsqrt(float n){ return floatint(0x1fbb4000+(intfloat(n)>>1)); }
// fast approx 1/x
  float floatinv(float n){ return floatint(0x7f000000-intfloat(n)); }
// fast approx log2(x)
  float floatlog2(float n){ return (float)((intfloat(n)<<1)-0x7f000000)*5.9604645e-08f; }
#else
  float floatinv(float n){ return 1.f/n;}
  float floatsqrt(float n){ return std::sqrt(n); }
  float floatlog2(float n){ return std::log2f(n); }
#endif

////////////////////////////////////////
// Escape time mandelbrot set function,
// with arbitrary start point zx, zy
// and arbitrary seed point ax, ay
//
// For julia set
// zx = pos_x,  zy = pos_y;
// ax = seed_x, ay = seed_y;
//
// For mandelbrot set
// zx = 0, zy = 0;
// ax = pos_x,  ay = pos_y;
//
const float  bailOut = 4;     // Escape radius
const int32_t itmult = 1<<10; // Color speed
//
// https://en.wikipedia.org/wiki/Mandelbrot_set
int32_t iteratefloat(float ax, float ay, float zx, float zy, uint16_t mxIT) {
  float zzl = 0;
  for (int it = 0; it<mxIT; it++) {
    float zzx = zx * zx;
    float zzy = zy * zy;
    // is the point is escaped?
    if(zzx+zzy>=bailOut){
      if(it>0){
        // calculate smooth coloring
        float zza = floatlog2(zzl);
        float zzb = floatlog2(zzx+zzy);
        float zzc = floatlog2(bailOut);
        float zzd = (zzc-zza)*floatinv(zzb-zza);
        return it*itmult+zzd*itmult;
      }
    };
    // z -> z*z + c
    zy = 2.f*zx*zy+ay;
    zx = zzx-zzy+ax;
    zzl = zzx+zzy;
  }
  return 0;
}

float sint[256]; // precalculated sin table, for performance reasons

// Palette color taken from:
// https://editor.p5js.org/Kouzerumatsukite/sketches/DwTiq9D01
// color palette originally made by piano_miles, written in p5js
// hsv2rgb(IT, cos(4096*it)/2+0.5, 1-sin(2048*it)/2-0.5)
void drawPixelPalette(int x, int y, uint32_t m){
  float r = 0.f, g = 0.f, b = 0.f;
  if(m){
    char  n =         m>> 4                               ;
    float l =abs(sint[m>> 2&255]         )*255.f          ;
    float s =   (sint[m    &255]+     1.f)*0.5f           ;
    r = (max(min(sint[n    &255]+0.5f,1.f),0.f)*s+(1-s))*l;
    g = (max(min(sint[n+ 85&255]+0.5f,1.f),0.f)*s+(1-s))*l;
    b = (max(min(sint[n+170&255]+0.5f,1.f),0.f)*s+(1-s))*l;
  }
  dma_display->drawPixelRGB888(x,y,r,g,b);
}

void drawCanvas() {
  uint32_t lastMicros = micros();
  double t = (double)lastMicros/8000000;
  double k = sin(t*3.212/2)*sin(t*3.212/2)/16+1;
  float cosk = (k-cos(t))/2;
  float xoff = (cos(t)*cosk+k/2-0.25);
  float yoff = (sin(t)*cosk         );
  for(uint8_t y=0;y<PANEL_RES_Y;y++){
    for(uint8_t x=0;x<PANEL_RES_X;x++){
      uint32_t itcount = iteratefloat(xoff,yoff,((x-64)+1)/64.f,(y)/64.f,64);
      uint32_t itcolor = itcount?floatsqrt(itcount)*4+t*1024:0;
      drawPixelPalette(x,y,itcolor);
    }
  }
}

volatile int frameCounts=0;
void Task1code(void *parameter){
  while(true){
    drawCanvas();
    delay(1);
    frameCounts++;
  }
}

void setup() {
  HUB75_I2S_CFG::i2s_pins _pins={
//  R1, G1, B1, R2, G2, B2,  A,  B,  C,  D,  E,LAT, OE,CLK, 
    25, 26, 27, 14, 12, 13, 23, 19,  5, 17, 18,  4, 15, 16,
  };
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X, // Module width
    PANEL_RES_Y, // Module height
    PANEL_CHAIN, // chain length
    _pins
  );

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->clearScreen();
  dma_display->setBrightness(64);
  setCpuFrequencyMhz(240);

  for(int i=0;i<256;i++){
    sint[i] = sinf(i/256.f*2.f*PI);
  }

  xTaskCreatePinnedToCore(\
      Task1code, /* Function to implement the task */\
      "Task1", /* Name of the task */\
      10000,  /* Stack size in words */\
      NULL,  /* Task input parameter */\
      4,  /* Priority of the task */\
      NULL,  /* Task handle. */\
      0); /* Core where the task should run */

  Serial.begin(115200);
}

uint64_t lastMillis=0;
void loop() {
  if(millis()-lastMillis>=1000){
    // log frame rate to serial
    Serial.print("fps: ");
    Serial.println(frameCounts);
    lastMillis += 1000;
    frameCounts=0;
  }
}
