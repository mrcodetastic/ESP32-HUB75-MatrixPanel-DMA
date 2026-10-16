#ifndef JuliaSet_H
#define JuliaSet_H

// Codetastic 2024

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
  for (int it = 0; it<mxIT; it++) {
    float zzx = zx * zx;
    float zzy = zy * zy;
    // is the point is escaped?
    if(zzx+zzy>=bailOut){
      if(it>0){
        // calculate smooth coloring
        return it * itmult;
      }
    };
    // z -> z*z + c
    zy = 2.f*zx*zy+ay;
    zx = zzx-zzy+ax;
  }
  return 0;
}

class PatternJuliaSet : public Drawable {

  private:

    float sint[256]; // precalculated sin table, for performance reasons

  public:
    PatternJuliaSet() {
      name = (char *)"Julia Set";
    }

    void start() {

      for(int i=0;i<256;i++){
        sint[i] = sinf(i/256.f*2.f*PI);
      }      
    }


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
      effects.setPixel(x,y,CRGB(r,g,b));

    }

    unsigned int drawFrame() {
      uint32_t lastMicros = micros();
      double t = (double)lastMicros/8000000;
      double k = sin(t*3.212/2)*sin(t*3.212/2)/16+1;
      float cosk = (k-cos(t))/2;
      float xoff = (cos(t)*cosk+k/2-0.25);
      float yoff = (sin(t)*cosk         );
      const float pixelScale = 4.0f / max(VPANEL_W, VPANEL_H);
      for(uint16_t y=0;y<VPANEL_H;y++){
        for(uint16_t x=0;x<VPANEL_W;x++){
          const float zx = (static_cast<int>(x) - (VPANEL_W - 1) / 2.0f) * pixelScale;
          const float zy = (static_cast<int>(y) - (VPANEL_H - 1) / 2.0f) * pixelScale;
          uint32_t itcount = iteratefloat(xoff, yoff, zx, zy, 64);
          uint32_t itcolor = itcount ? itcount / 16 + t * 1024 : 0;
          drawPixelPalette(x,y,itcolor);
        }
      }

      blur2d(effects.leds, VPANEL_W, VPANEL_H, 64);      

      effects.ShowFrame();
      return 0;      
    }


};

#endif
