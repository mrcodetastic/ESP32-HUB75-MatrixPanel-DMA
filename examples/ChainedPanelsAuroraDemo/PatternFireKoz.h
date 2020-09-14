/*
   Aurora: https://github.com/pixelmatix/aurora
   Copyright (c) 2014 Jason Coon

   Added by @Kosso. Cobbled together from various places which I can't remember. I'll update this when I track it down.
   Requires PaletteFireKoz.h

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
   the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
   FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef PatternFireKoz_H
#define PatternFireKoz_H

class PatternFireKoz : public Drawable {
  private:

    const int FIRE_HEIGHT = 800;

    int Bit = 0, NBit = 1;
    float fire_c;
    // might not need this buffer here... there some led buffers set up in Effects.h
    uint8_t fireBuffer[VPANEL_W][VPANEL_H][2];

  public:
    PatternFireKoz() {
      name = (char *)"FireKoz";
    }

    unsigned int drawFrame() {

      for (int x = 1; x < VPANEL_W - 1; x++)
      {
        fireBuffer[x][VPANEL_H - 2][Bit] = random(0, FIRE_HEIGHT);
        if (random(0, 100) > 80)
        {
          fireBuffer[x][VPANEL_H - 2][Bit] = 0;
          fireBuffer[x][VPANEL_H - 3][Bit] = 0;
        }
      }
      for (int y = 1; y < VPANEL_H - 1; y++)
      {
        for (int x = 1; x < VPANEL_W - 1; x++)
        {
          fire_c = (fireBuffer[x - 1][y][Bit] +
                    fireBuffer[x + 1][y][Bit] +
                    fireBuffer[x][y - 1][Bit] +
                    fireBuffer[x][y + 1][Bit] +
                    fireBuffer[x][y][Bit]) /
                   5.0;

          fire_c = (fireBuffer[x - 1][y][Bit] +
                    fireBuffer[x + 1][y][Bit] +
                    fireBuffer[x][y - 1][Bit] +
                    fireBuffer[x][y + 1][Bit] +
                    fireBuffer[x][y][Bit]) /
                   5.0;

          if (fire_c > (FIRE_HEIGHT / 2) && fire_c < FIRE_HEIGHT) {
            fire_c -= 0.2;
          } else if (fire_c > (FIRE_HEIGHT / 4) && fire_c < (FIRE_HEIGHT / 2)) {
            fire_c -= 0.4;
          } else if (fire_c <= (FIRE_HEIGHT / 8)) {
            fire_c -= 0.7;
          } else {
            fire_c -= 1;
          }
          if (fire_c < 0)
            fire_c = 0;
          if (fire_c >= FIRE_HEIGHT + 1)
            fire_c = FIRE_HEIGHT - 1;
          fireBuffer[x][y - 1][NBit] = fire_c;
          int index = (int)fire_c * 3;
          if (fire_c == 0)
          {
            effects.drawBackgroundFastLEDPixelCRGB(x, y, CRGB(0, 0, 0));
          }
          else
          {
            effects.drawBackgroundFastLEDPixelCRGB(x, y, CRGB(palette_fire[index], palette_fire[index + 1], palette_fire[index + 2]));
          }
        }
      }
      //display.drawRect(0, 0, VPANEL_W, VPANEL_H, display.color565(25, 25, 25));

      NBit = Bit;
      Bit = 1 - Bit;

      effects.ShowFrame();

      return 30; // no idea what this is for...
    }
};

#endif
