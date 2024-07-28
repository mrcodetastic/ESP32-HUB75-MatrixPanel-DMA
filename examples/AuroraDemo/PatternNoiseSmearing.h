/*
* Aurora: https://github.com/pixelmatix/aurora
* Copyright (c) 2014 Jason Coon
*
* Portions of this code are adapted from "Noise Smearing" by Stefan Petrick: https://gist.githubusercontent.com/embedded-creations/5cd47d83cb0e04f4574d/raw/ebf6a82b4755d55cfba3bf6598f7b19047f89daf/NoiseSmearing.ino
* Copyright (c) 2014 Stefan Petrick
* http://www.stefan-petrick.de/wordpress_beta
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

#ifndef PatternNoiseSmearing_H
#define PatternNoiseSmearing_H

byte patternNoiseSmearingHue = 0;

class PatternMultipleStream : public Drawable {
public:
  PatternMultipleStream() {
    name = (char *)"MultipleStream";
  }

  // this pattern draws two points to the screen based on sin/cos if a counter
  // (comment out NoiseSmearWithRadius to see pattern of pixels)
  // these pixels are smeared by a large radius, giving a lot of movement
  // the image is dimmed before each drawing to not saturate the screen with color
  // the smear has an offset so the pixels usually have a trail leading toward the upper left
  unsigned int drawFrame() {
    static unsigned long counter = 0;
#if 0
    // this counter lets put delays between each frame and still get the same animation
    counter++;
#else
    // this counter updates in real time and can't be slowed down for debugging
    counter = millis() / 10;
#endif

    byte x1 = 4 + sin8(counter * 2) / 10;
    byte x2 = 8 + sin8(counter * 2) / 16;
    byte y2 = 8 + cos8((counter * 2) / 3) / 16;

    effects.leds[XY(x1, x2)] = effects.ColorFromCurrentPalette(patternNoiseSmearingHue);
    effects.leds[XY(x2, y2)] = effects.ColorFromCurrentPalette(patternNoiseSmearingHue + 128);

    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(8);
    effects.MoveFractionalNoiseX();

    effects.MoveY(8);
    effects.MoveFractionalNoiseY();

    patternNoiseSmearingHue++;

    return 0;
  }
};

class PatternMultipleStream2 : public Drawable {
public:
  PatternMultipleStream2() {
    name = (char *)"MultipleStream2";
  }

  unsigned int drawFrame() {
    effects.DimAll(230); effects.ShowFrame();

    byte xx = 4 + sin8(millis() / 9) / 10;
    byte yy = 4 + cos8(millis() / 10) / 10;
    effects.leds[XY(xx, yy)] += effects.ColorFromCurrentPalette(patternNoiseSmearingHue);

    xx = 8 + sin8(millis() / 10) / 16;
    yy = 8 + cos8(millis() / 7) / 16;
    effects.leds[XY(xx, yy)] += effects.ColorFromCurrentPalette(patternNoiseSmearingHue + 80);

    effects.leds[XY(15, 15)] += effects.ColorFromCurrentPalette(patternNoiseSmearingHue + 160);

    noise_x += 1000;
    noise_y += 1000;
    noise_z += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(3);
    effects.MoveFractionalNoiseY(4);

    effects.MoveY(3);
    effects.MoveFractionalNoiseX(4);

    patternNoiseSmearingHue++;

    return 0;
  }
};

class PatternMultipleStream3 : public Drawable {
public:
  PatternMultipleStream3() {
    name = (char *)"MultipleStream3";
  }

  unsigned int drawFrame() {
    //CLS();
    effects.DimAll(235); effects.ShowFrame();

    for (uint8_t i = 3; i < 32; i = i + 4) {
      effects.leds[XY(i, 15)] += effects.ColorFromCurrentPalette(i * 8);
    }

    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_z += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(3);
    effects.MoveFractionalNoiseY(4);

    effects.MoveY(3);
    effects.MoveFractionalNoiseX(4);

        effects.ShowFrame();

    return 1;
  }
};

class PatternMultipleStream4 : public Drawable {
public:
  PatternMultipleStream4() {
    name = (char *)"MultipleStream4";
  }

  unsigned int drawFrame() {

    //CLS();
    effects.DimAll(235); effects.ShowFrame();

    effects.leds[XY(15, 15)] += effects.ColorFromCurrentPalette(patternNoiseSmearingHue);


    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(8);
    effects.MoveFractionalNoiseX();

    effects.MoveY(8);
    effects.MoveFractionalNoiseY();

    patternNoiseSmearingHue++;

    return 0;
  }
};

class PatternMultipleStream5 : public Drawable {
public:
  PatternMultipleStream5() {
    name = (char *)"MultipleStream5";
  }

  unsigned int drawFrame() {

    //CLS();
    effects.DimAll(235); effects.ShowFrame();


    for (uint8_t i = 3; i < 32; i = i + 4) {
      effects.leds[XY(i, 31)] += effects.ColorFromCurrentPalette(i * 8);
    }

    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_z += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(3);
    effects.MoveFractionalNoiseY(4);

    effects.MoveY(4);
    effects.MoveFractionalNoiseX(4);

    return 0;
  }
};

class PatternMultipleStream8 : public Drawable {
public:
  PatternMultipleStream8() {
    name = (char *)"MultipleStream8";
  }

  unsigned int drawFrame() {
    effects.DimAll(230); effects.ShowFrame();

    // draw grid of rainbow dots on top of the dimmed image
    for (uint8_t y = 1; y < 32; y = y + 6) {
      for (uint8_t x = 1; x < 32; x = x + 6) {

        effects.leds[XY(x, y)] += effects.ColorFromCurrentPalette((x * y) / 4);
      }
    }

    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_z += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(3);
    effects.MoveFractionalNoiseX(4);

    effects.MoveY(3);
    effects.MoveFractionalNoiseY(4);

    return 0;
  }
};

class PatternPaletteSmear : public Drawable {
public:
  PatternPaletteSmear() {
    name = (char *)"PaletteSmear";
  }

  unsigned int drawFrame() {

    effects.DimAll(170); effects.ShowFrame();
   
    // draw a rainbow color palette
    for (uint8_t y = 0; y < VPANEL_H; y++) {
      for (uint8_t x = 0; x < VPANEL_W; x++) {
        effects.leds[XY(x, y)] += effects.ColorFromCurrentPalette(x * 8, y * 8 + 7);
      }
    }
 
  
    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
 
    effects.FillNoise();

    effects.MoveX(3);
    //effects.MoveFractionalNoiseY(4);

    effects.MoveY(3);
    effects.MoveFractionalNoiseX(4);
    effects.ShowFrame();

    return 0;
  }
};

class PatternRainbowFlag : public Drawable {
public:
  PatternRainbowFlag() {
    name = (char *)"RainbowFlag";
  }

  unsigned int drawFrame() {
    effects.DimAll(10); effects.ShowFrame();

    CRGB rainbow[7] = {
      CRGB::Red,
      CRGB::Orange,
      CRGB::Yellow,
      CRGB::Green,
      CRGB::Blue,
      CRGB::Violet
    };

    uint8_t y = 2;

    for (uint8_t c = 0; c < 6; c++) {
      for (uint8_t j = 0; j < 5; j++) {
        for (uint8_t x = 0; x < VPANEL_W; x++) {
          effects.leds[XY(x, y)] += rainbow[c];
        }

        y++;
        if (y >= VPANEL_H)
          break;
      }
    }

    // Noise
    noise_x += 1000;
    noise_y += 1000;
    noise_scale_x = 4000;
    noise_scale_y = 4000;
    effects.FillNoise();

    effects.MoveX(3);
    effects.MoveFractionalNoiseY(4);

    effects.MoveY(3);
    effects.MoveFractionalNoiseX(4);

    return 0;
  }
};
#endif
