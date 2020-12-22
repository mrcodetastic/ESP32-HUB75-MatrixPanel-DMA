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

#ifndef PatternAttract_H

class PatternAttract : public Drawable {
private:
    const int count = 8;
    Attractor attractor;

public:
    PatternAttract() {
        name = (char *)"Attract";
    }

    void start() {
        int direction = random(0, 2);
        if (direction == 0)
            direction = -1;

        for (int i = 0; i < count; i++) {
            Boid boid = Boid(15, 31 - i);
            boid.mass = 1; // random(0.1, 2);
            boid.velocity.x = ((float) random(40, 50)) / 100.0;
            boid.velocity.x *= direction;
            boid.velocity.y = 0;
            boid.colorIndex = i * 32;
            boids[i] = boid;
            //dim = random(170, 250);
        }
    }

    unsigned int drawFrame() {
        // dim all pixels on the display
        uint8_t dim = beatsin8(2, 170, 250);
        effects.DimAll(dim);

        for (int i = 0; i < count; i++) {
            Boid boid = boids[i];

            PVector force = attractor.attract(boid);
            boid.applyForce(force);

            boid.update();
            effects.drawBackgroundFastLEDPixelCRGB(boid.location.x, boid.location.y, effects.ColorFromCurrentPalette(boid.colorIndex));

            boids[i] = boid;
        }

        effects.ShowFrame();
        return 0;
    }
};

#endif
