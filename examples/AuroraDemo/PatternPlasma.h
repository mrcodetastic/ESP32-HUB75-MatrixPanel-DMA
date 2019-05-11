/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from LedEffects Plasma by Robert Atkins: https://bitbucket.org/ratkins/ledeffects/src/26ed3c51912af6fac5f1304629c7b4ab7ac8ca4b/Plasma.cpp?at=default
 * Copyright (c) 2013 Robert Atkins
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

#ifndef PatternPlasma_H
#define PatternPlasma_H

class PatternPlasma : public Drawable {
private:
    int time = 0;
    int cycles = 0;

public:
    PatternPlasma() {
        name = (char *)"Plasma";
    }

    unsigned int drawFrame() {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                int16_t v = 0;
                uint8_t wibble = sin8(time);
                v += sin16(x * wibble * 2 + time);
                v += cos16(y * (128 - wibble) * 2 + time);
                v += sin16(y * x * cos8(-time) / 2);

                effects.Pixel(x, y, (v >> 8) + 127);
            }
        }

        time += 1;
        cycles++;

        if (cycles >= 2048) {
            time = 0;
            cycles = 0;
        }

        effects.ShowFrame();

        return 30;
    }
};

#endif
