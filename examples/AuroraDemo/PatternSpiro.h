/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
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

#ifndef PatternSpiro_H

class PatternSpiro : public Drawable {
  private:
    byte theta1 = 0;
    byte theta2 = 0;
    byte hueoffset = 0;

    uint8_t radiusx = MATRIX_WIDTH / 4;
    uint8_t radiusy = MATRIX_HEIGHT / 4;
    uint8_t minx = MATRIX_CENTER_X - radiusx;
    uint8_t maxx = MATRIX_CENTER_X + radiusx + 1;
    uint8_t miny = MATRIX_CENTER_Y - radiusy;
    uint8_t maxy = MATRIX_CENTER_Y + radiusy + 1;

    uint8_t spirocount = 1;
    uint8_t spirooffset = 256 / spirocount;
    boolean spiroincrement = true;

    boolean handledChange = false;

  public:
    PatternSpiro() {
      name = (char *)"Spiro";
    }

    void start(){
      effects.ClearFrame();
    };

    unsigned int drawFrame() {
      blur2d(effects.leds, MATRIX_WIDTH > 255 ? 255 : MATRIX_WIDTH, MATRIX_HEIGHT > 255 ? 255 : MATRIX_HEIGHT, 192);

      boolean change = false;
      
      for (int i = 0; i < spirocount; i++) {
        uint8_t x = mapsin8(theta1 + i * spirooffset, minx, maxx);
        uint8_t y = mapcos8(theta1 + i * spirooffset, miny, maxy);

        uint8_t x2 = mapsin8(theta2 + i * spirooffset, x - radiusx, x + radiusx);
        uint8_t y2 = mapcos8(theta2 + i * spirooffset, y - radiusy, y + radiusy);

        CRGB color = effects.ColorFromCurrentPalette(hueoffset + i * spirooffset, 128);
        effects.leds[XY(x2, y2)] += color;
        
        if((x2 == MATRIX_CENTER_X && y2 == MATRIX_CENTER_Y) ||
           (x2 == MATRIX_CENTRE_X && y2 == MATRIX_CENTRE_Y)) change = true;
      }

      theta2 += 1;

      EVERY_N_MILLIS(25) {
        theta1 += 1;
      }

      EVERY_N_MILLIS(100) {
        if (change && !handledChange) {
          handledChange = true;
          
          if (spirocount >= MATRIX_WIDTH || spirocount == 1) spiroincrement = !spiroincrement;

          if (spiroincrement) {
            if(spirocount >= 4)
              spirocount *= 2;
            else
              spirocount += 1;
          }
          else {
            if(spirocount > 4)
              spirocount /= 2;
            else
              spirocount -= 1;
          }

          spirooffset = 256 / spirocount;
        }
        
        if(!change) handledChange = false;
      }

      EVERY_N_MILLIS(33) {
        hueoffset += 1;
      }

      effects.ShowFrame();
      return 0;
    }
};

#endif
