/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from FastLED Fire2012 example by Mark Kriegsman: https://github.com/FastLED/FastLED/blob/master/examples/Noise/Noise.ino
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

#ifndef PatternSimplexNoise_H
#define PatternSimplexNoise_H

class PatternSimplexNoise : public Drawable {
  public:
    PatternSimplexNoise() {
      name = (char *)"Noise";
    }

    void start() {
      // Initialize our coordinates to some random values
      noise_x = random16();
      noise_y = random16();
      noise_z = random16();
    }

    unsigned int drawFrame() {
#if FASTLED_VERSION >= 3001000
      // a new parameter set every 15 seconds
      EVERY_N_SECONDS(15) {
        noise_x = random16();
        noise_y = random16();
        noise_z = random16();
      }
#endif

      uint32_t speed = 100;

      effects.FillNoise();
      ShowNoiseLayer(0, 1, 0);

      // noise_x += speed;
      noise_y += speed;
      noise_z += speed;

      effects.ShowFrame();

      return 30;
    }

    // show just one layer
    void ShowNoiseLayer(byte layer, byte colorrepeat, byte colorshift) {
      for (uint16_t i = 0; i < VPANEL_W; i++) {
        for (uint16_t j = 0; j < VPANEL_H; j++) {
          uint8_t pixel = noise[i][j];

          // assign a color depending on the actual palette
          effects.leds[XY16(i, j)] = effects.ColorFromCurrentPalette(colorrepeat * (pixel + colorshift), pixel);
        }
      }
    }
};

#endif
