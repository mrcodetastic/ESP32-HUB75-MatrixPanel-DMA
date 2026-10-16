/*
*
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

#ifndef PatternIncrementalDrift2_H
#define PatternIncrementalDrift2_H

class PatternIncrementalDrift2 : public Drawable {
  public:
    PatternIncrementalDrift2() {
      name = (char *)"Incremental Drift Rose";
    }

    unsigned int drawFrame() {
      uint8_t dim = beatsin8(2, 170, 250);
      effects.DimAll(dim);

      const int maxRadius = (min(VPANEL_W, VPANEL_H) - 1) / 2;
      for (int i = 1; i <= maxRadius; ++i)
      {
        CRGB color = effects.ColorFromCurrentPalette((i - 1) * 240 / max(1, maxRadius));
        const uint8_t phase = beatsin8(max(1, 2 * i));
        const uint16_t x = i + (static_cast<uint32_t>(sin8(phase)) * (VPANEL_W - 1 - 2 * i)) / 255;
        const uint16_t y = i + (static_cast<uint32_t>(cos8(phase)) * (VPANEL_H - 1 - 2 * i)) / 255;

        effects.setPixel(x, y, color);
      }

      effects.ShowFrame();
      return 0;
    }
};

#endif
