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

#ifndef PatternSpin_H



class PatternSpin : public Drawable {
public:
    PatternSpin() {
        name = (char *)"Spin";
    }

    float degrees = 0;
    float radius = 16;

    float speedStart = 1;
    float velocityStart = 0.6;

    float maxSpeed = 30;

    float speed = speedStart;
    float velocity = velocityStart;

    void start() {
        speed = speedStart;
        velocity = velocityStart;
        degrees = 0;
    }

    unsigned int drawFrame() {
        effects.DimAll(190); effects.ShowFrame();

        CRGB color = effects.ColorFromCurrentPalette(speed * 8);

        // start position
        int x;
        int y;

        // target position
        float targetDegrees = degrees + speed;
        float targetRadians = radians(targetDegrees);
        int targetX = (int) (MATRIX_CENTER_X + radius * cos(targetRadians));
        int targetY = (int) (MATRIX_CENTER_Y - radius * sin(targetRadians));

        float tempDegrees = degrees;

        do{
            float radians = radians(tempDegrees);
            x = (int) (MATRIX_CENTER_X + radius * cos(radians));
            y = (int) (MATRIX_CENTER_Y - radius * sin(radians));

            effects.drawBackgroundFastLEDPixelCRGB(x, y, color);
            effects.drawBackgroundFastLEDPixelCRGB(y, x, color);

            tempDegrees += 1;
            if (tempDegrees >= 360)
                tempDegrees = 0;
        } while (x != targetX || y != targetY);

        degrees += speed;

        // add velocity to the particle each pass around the accelerator
        if (degrees >= 360) {
            degrees = 0;
            speed += velocity;
            if (speed <= speedStart) {
                speed = speedStart;
                velocity *= -1;
            }
            else if (speed > maxSpeed){
                speed = maxSpeed - velocity;
                velocity *= -1;
            }
        }

        return 0;
    }
};

#endif
