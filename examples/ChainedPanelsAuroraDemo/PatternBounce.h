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

#ifndef PatternBounce_H

class PatternBounce : public Drawable {
private:
    static const int count = 32;
    PVector gravity = PVector(0, 0.0125);

public:
    PatternBounce() {
        name = (char *)"Bounce";
    }

    void start() {
        unsigned int colorWidth = 256 / count;
        for (int i = 0; i < count; i++) {
            Boid boid = Boid(i, 0);
            boid.velocity.x = 0;
            boid.velocity.y = i * -0.01;
            boid.colorIndex = colorWidth * i;
            boid.maxforce = 10;
            boid.maxspeed = 10;
            boids[i] = boid;
        }
    }

    unsigned int drawFrame() {
        // dim all pixels on the display
        effects.DimAll(170); effects.ShowFrame();

        for (int i = 0; i < count; i++) {
            Boid boid = boids[i];

            boid.applyForce(gravity);

            boid.update();

            effects.drawBackgroundFastLEDPixelCRGB(boid.location.x, boid.location.y, effects.ColorFromCurrentPalette(boid.colorIndex));

            if (boid.location.y >= VPANEL_H - 1) {
                boid.location.y = VPANEL_H - 1;
                boid.velocity.y *= -1.0;
            }

            boids[i] = boid;
        }

        return 15;
    }
};

#endif
