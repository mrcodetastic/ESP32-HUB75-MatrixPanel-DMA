/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from Andrew: http://pastebin.com/f22bfe94d
 * which, in turn, was "Adapted from the Life example on the Processing.org site"
 *
 * Made much more colorful by J.B. Langston: https://github.com/jblang/aurora/commit/6db5a884e3df5d686445c4f6b669f1668841929b
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

#ifndef PatternLife_H
#define PatternLife_H

class Cell {
public:
  byte alive : 1;
  byte prev : 1;
  byte hue: 6;  
  byte brightness;
};

class PatternLife : public Drawable {
private:
    Cell world[VPANEL_W][VPANEL_H];
    unsigned int density = 50;
    int generation = 0;

    void randomFillWorld() {
        for (int i = 0; i < VPANEL_W; i++) {
            for (int j = 0; j < VPANEL_H; j++) {
                if (random(100) < density) {
                    world[i][j].alive = 1;
                    world[i][j].brightness = 255;
                }
                else {
                    world[i][j].alive = 0;
                    world[i][j].brightness = 0;
                }
                world[i][j].prev = world[i][j].alive;
                world[i][j].hue = 0;
            }
        }
    }

    int neighbours(int x, int y) {
        return (world[(x + 1) % VPANEL_W][y].prev) +
            (world[x][(y + 1) % VPANEL_H].prev) +
            (world[(x + VPANEL_W - 1) % VPANEL_W][y].prev) +
            (world[x][(y + VPANEL_H - 1) % VPANEL_H].prev) +
            (world[(x + 1) % VPANEL_W][(y + 1) % VPANEL_H].prev) +
            (world[(x + VPANEL_W - 1) % VPANEL_W][(y + 1) % VPANEL_H].prev) +
            (world[(x + VPANEL_W - 1) % VPANEL_W][(y + VPANEL_H - 1) % VPANEL_H].prev) +
            (world[(x + 1) % VPANEL_W][(y + VPANEL_H - 1) % VPANEL_H].prev);
    }

public:
    PatternLife() {
        name = (char *)"Life";
    }

    unsigned int drawFrame() {
        if (generation == 0) {
            effects.ClearFrame();

            randomFillWorld();
        }

        // Display current generation
        for (int i = 0; i < VPANEL_W; i++) {
            for (int j = 0; j < VPANEL_H; j++) {
                effects.leds[XY(i, j)] = effects.ColorFromCurrentPalette(world[i][j].hue * 4, world[i][j].brightness);
            }
        }

        // Birth and death cycle
        for (int x = 0; x < VPANEL_W; x++) {
            for (int y = 0; y < VPANEL_H; y++) {
                // Default is for cell to stay the same
                if (world[x][y].brightness > 0 && world[x][y].prev == 0)
                  world[x][y].brightness *= 0.9;
                int count = neighbours(x, y);
                if (count == 3 && world[x][y].prev == 0) {
                    // A new cell is born
                    world[x][y].alive = 1;
                    world[x][y].hue += 2;
                    world[x][y].brightness = 255;
                } else if ((count < 2 || count > 3) && world[x][y].prev == 1) {
                    // Cell dies
                    world[x][y].alive = 0;
                }
            }
        }

        // Copy next generation into place
        for (int x = 0; x < VPANEL_W; x++) {
            for (int y = 0; y < VPANEL_H; y++) {
                world[x][y].prev = world[x][y].alive;
            }
        }


        generation++;
        if (generation >= 256)
            generation = 0;

        effects.ShowFrame();
  
        return 60;
    }
};

#endif
