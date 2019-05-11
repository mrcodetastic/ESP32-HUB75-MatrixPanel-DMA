/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from FastLED Fire2012 example by Mark Kriegsman: https://github.com/FastLED/FastLED/tree/master/examples/Fire2012WithPalette
 * Copyright (c) 2013 FastLED
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

#ifndef PatternFire_H
#define PatternFire_H

#ifndef Effects_H
#include "Effects.h"
#endif

class PatternFire : public Drawable {
  private:

  public:
    PatternFire() {
      name = (char *)"Fire";
    }

    // There are two main parameters you can play with to control the look and
    // feel of your fire: COOLING (used in step 1 above), and SPARKING (used
    // in step 3 above).
    //
    // cooling: How much does the air cool as it rises?
    // Less cooling = taller flames.  More cooling = shorter flames.
    // Default 55, suggested range 20-100
    int cooling = 100;

    // sparking: What chance (out of 255) is there that a new spark will be lit?
    // Higher chance = more roaring fire.  Lower chance = more flickery fire.
    // Default 120, suggested range 50-200.
    unsigned int sparking = 100;

    unsigned int drawFrame() {
      // Add entropy to random number generator; we use a lot of it.
      random16_add_entropy( random16());

      effects.DimAll(235); 

      for (int x = 0; x < MATRIX_WIDTH; x++) {
        // Step 1.  Cool down every cell a little
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
          int xy = XY(x, y);
          heat[xy] = qsub8(heat[xy], random8(0, ((cooling * 10) / MATRIX_HEIGHT) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
          heat[XY(x, y)] = (heat[XY(x, y + 1)] + heat[XY(x, y + 2)] + heat[XY(x, y + 2)]) / 3;
        }

        // Step 2.  Randomly ignite new 'sparks' of heat
        if (random8() < sparking) {
          // int x = (p[0] + p[1] + p[2]) / 3;

          int xy = XY(x, MATRIX_HEIGHT - 1);
          heat[xy] = qadd8(heat[xy], random8(160, 255));
        }

        // Step 4.  Map from heat cells to LED colors
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
          int xy = XY(x, y);
          byte colorIndex = heat[xy];

          // Recommend that you use values 0-240 rather than
          // the usual 0-255, as the last 15 colors will be
          // 'wrapping around' from the hot end to the cold end,
          // which looks wrong.
          colorIndex = scale8(colorIndex, 200);

          // override color 0 to ensure a black background?
          if (colorIndex != 0)
            //                    effects.leds[xy] = CRGB::Black;
            //                else
            effects.leds[xy] = effects.ColorFromCurrentPalette(colorIndex);
        }
      }

      // Noise
      noise_x += 1000;
      noise_y += 1000;
      noise_z += 1000;
      noise_scale_x = 4000;
      noise_scale_y = 4000;
      effects.FillNoise();

      effects.MoveX(2);
      effects.MoveFractionalNoiseX(2);


      effects.ShowFrame();

      return 15;
    }
};

#endif
