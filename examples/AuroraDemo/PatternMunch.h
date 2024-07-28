/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Munch pattern created by J.B. Langston: https://github.com/jblang/aurora/blob/master/PatternMunch.h
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

#ifndef PatternMunch_H
#define PatternMunch_H


class PatternMunch : public Drawable {
private:
    byte count = 0;
    byte dir = 1;
    byte flip = 0;
    byte generation = 0;

public:
    PatternMunch() {
        name = (char *)"Munch";
    }

    unsigned int drawFrame() {
       
        for (uint16_t x = 0; x < VPANEL_W; x++) {
            for (uint16_t y = 0; y < VPANEL_H; y++) {
                effects.leds[XY16(x, y)] = (x ^ y ^ flip) < count ? effects.ColorFromCurrentPalette(((x ^ y) << 2) + generation) : CRGB::Black;

                // The below is more pleasant
               // effects.leds[XY(x, y)] = effects.ColorFromCurrentPalette(((x ^ y) << 2) + generation) ;
            }
        }
        
        count += dir;
        
        if (count <= 0 || count >= VPANEL_W) {
          dir = -dir;
        }
        
        if (count <= 0) {
          if (flip == 0)
            flip = VPANEL_W-1;
          else
            flip = 0;
        }

        generation++;

        // show it ffs!
        effects.ShowFrame();
        return 60;
    }
};

#endif
