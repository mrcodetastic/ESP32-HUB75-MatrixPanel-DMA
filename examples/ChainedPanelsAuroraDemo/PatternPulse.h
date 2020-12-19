/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Based at least in part on someone else's work that I can no longer find.
 * Please let me know if you recognize any of this code!
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

#ifndef PatternPulse_H
#define PatternPulse_H

class PatternPulse : public Drawable {
  private:
    int hue;
    int centerX = 0;
    int centerY = 0;
    int step = -1;
    int maxSteps = 16;
    float fadeRate = 0.8;
    int diff;

  public:
    PatternPulse() {
      name = (char *)"Pulse";
    }

    unsigned int drawFrame() {
      effects.DimAll(235);

      if (step == -1) {
        centerX = random(32);
        centerY = random(32);
        hue = random(256); // 170;
        step = 0;
      }

      if (step == 0) {
        matrix.drawCircle(centerX, centerY, step, effects.ColorFromCurrentPalette(hue));
        step++;
      }
      else {
        if (step < maxSteps) {
          // initial pulse
          matrix.drawCircle(centerX, centerY, step, effects.ColorFromCurrentPalette(hue, pow(fadeRate, step - 2) * 255));

          // secondary pulse
          if (step > 3) {
            matrix.drawCircle(centerX, centerY, step - 3, effects.ColorFromCurrentPalette(hue, pow(fadeRate, step - 2) * 255));
          }
          step++;
        }
        else {
          step = -1;
        }
      }

      effects.standardNoiseSmearing();

      effects.ShowFrame();

      return 30;
    }
};

#endif
