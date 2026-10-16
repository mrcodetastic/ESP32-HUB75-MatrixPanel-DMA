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

#ifndef PatternRadar_H
#define PatternRadar_H

class PatternRadar : public Drawable {
  private:
    byte theta = 0;
    byte hueoffset = 0;
    unsigned long last_update_hue_ms = 0;

  public:
    PatternRadar() {
      name = (char *)"Radar";
    }

    unsigned int drawFrame() {
      effects.DimAll(254);

      const int squareSize = max(VPANEL_W, VPANEL_H);
      const int radiusLimit = squareSize / 2;
      const int centerX = VPANEL_W / 2;
      const int centerY = VPANEL_H / 2;
      for (int offset = 0; offset <= radiusLimit; offset++) {
        byte hue = 255 - (offset * 16 + hueoffset);
        CRGB color = effects.ColorFromCurrentPalette(hue);
        const int radius = radiusLimit - offset;
        const int x = centerX + (static_cast<int>(cos8(theta)) - 128) * radius / 127;
        const int y = centerY + (static_cast<int>(sin8(theta)) - 128) * radius / 127;
        // setPixel clips against the real panel bounds, leaving the radar square
        // centered on a rectangular display and naturally cropping its edges.
        effects.setPixel(x, y, color);
      }

      if (millis() - last_update_hue_ms > 25) {
        last_update_hue_ms = millis();
        theta += 2;
        hueoffset += 1;
      }

      effects.ShowFrame();
      return 0;
    }
};

#endif
