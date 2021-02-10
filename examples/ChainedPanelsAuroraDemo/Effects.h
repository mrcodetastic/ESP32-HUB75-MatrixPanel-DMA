
/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from "Funky Clouds" by Stefan Petrick: https://gist.github.com/anonymous/876f908333cd95315c35
 * Portions of this code are adapted from "NoiseSmearing" by Stefan Petrick: https://gist.github.com/StefanPetrick/9ee2f677dbff64e3ba7a
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

#ifndef Effects_H
#define Effects_H

/* ---------------------------- GLOBAL CONSTANTS ----------------------------- */

const int  MATRIX_CENTER_X = VPANEL_W / 2;
const int  MATRIX_CENTER_Y = VPANEL_H / 2;
// US vs GB, huh? :)
//const byte MATRIX_CENTRE_X = MATRIX_CENTER_X - 1;
//const byte MATRIX_CENTRE_Y = MATRIX_CENTER_Y - 1;
#define MATRIX_CENTRE_X MATRIX_CENTER_X
#define MATRIX_CENTRE_Y MATRIX_CENTER_Y


const uint16_t NUM_LEDS = (VPANEL_W * VPANEL_H) + 1; // one led spare to capture out of bounds

// forward declaration
uint16_t XY16( uint16_t x, uint16_t y);

/* Convert x,y co-ordinate to flat array index. 
 * x and y positions start from 0, so must not be >= 'real' panel width or height 
 * (i.e. 64 pixels or 32 pixels.).  Max value: VPANEL_W-1 etc.
 * Ugh... uint8_t - really??? this weak method can't cope with 256+ pixel matrixes :(
 */
uint16_t XY( uint8_t x, uint8_t y) 
{
  return XY16(x, y);
}

/**
 *  The one for 256+ matrixes
 *  otherwise this:
 *    for (uint8_t i = 0; i < VPANEL_W; i++) {}
 *  turns into an infinite loop
 */
uint16_t XY16( uint16_t x, uint16_t y) 
{
    if( x >= VPANEL_W) return 0;
    if( y >= VPANEL_H) return 0;

    return (y * VPANEL_W) + x + 1; // everything offset by one to capute out of bounds stuff - never displayed by ShowFrame()
}


uint8_t beatcos8(accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255, uint32_t timebase = 0, uint8_t phase_offset = 0)
{
  uint8_t beat = beat8(beats_per_minute, timebase);
  uint8_t beatcos = cos8(beat + phase_offset);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatcos, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

uint8_t mapsin8(uint8_t theta, uint8_t lowest = 0, uint8_t highest = 255) {
  uint8_t beatsin = sin8(theta);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatsin, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

uint8_t mapcos8(uint8_t theta, uint8_t lowest = 0, uint8_t highest = 255) {
  uint8_t beatcos = cos8(theta);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatcos, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

// Array of temperature readings at each simulation cell
//byte heat[NUM_LEDS];    // none of the currently enabled effects uses this

uint32_t noise_x;
uint32_t noise_y;
uint32_t noise_z;
uint32_t noise_scale_x;
uint32_t noise_scale_y;

//uint8_t noise[VPANEL_W][VPANEL_H];
uint8_t **noise = nullptr;  // we will allocate mem later
uint8_t noisesmoothing;

class Effects {
public:
  CRGB *leds;

  Effects(){
    // we do dynamic allocation for leds buffer, otherwise esp32 toolchain can't link static arrays of such a big size for 256+ matrixes
    leds = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));

    // allocate mem for noise effect
    // (there should be some guards for malloc errors eventually)
    noise = (uint8_t **)malloc(VPANEL_W * sizeof(uint8_t *));
    for (int i = 0; i < VPANEL_W; ++i) {
      noise[i] = (uint8_t *)malloc(VPANEL_H * sizeof(uint8_t));
    }

    ClearFrame();
  }
  ~Effects(){
    free(leds);
    for (int i = 0; i < VPANEL_W; ++i) {
      free(noise[i]);
    }
    free(noise);
  }

  /* The only 'framebuffer' we have is what is contained in the leds and leds2 variables.
   * We don't store what the color a particular pixel might be, other than when it's turned
   * into raw electrical signal output gobbly-gook (i.e. the DMA matrix buffer), but this * is not reversible.
   * 
   * As such, any time these effects want to write a pixel color, we first have to update
   * the leds or leds2 array, and THEN write it to the RGB panel. This enables us to 'look up' the array to see what a pixel color was previously, each drawFrame().
   */
  void drawBackgroundFastLEDPixelCRGB(int16_t x, int16_t y, CRGB color)
  {
	  leds[XY(x, y)] = color;
	  //matrix.drawPixelRGB888(x, y, color.r, color.g, color.b); 
  }

  // write one pixel with the specified color from the current palette to coordinates
  void Pixel(int x, int y, uint8_t colorIndex) {
    leds[XY(x, y)] = ColorFromCurrentPalette(colorIndex);
    //matrix.drawPixelRGB888(x, y, temp.r, temp.g, temp.b); // now draw it?
  }
  
 void PrepareFrame() {
   // leds = (CRGB*) backgroundLayer.backBuffer();
  }

  void ShowFrame() {
    //#if (FASTLED_VERSION >= 3001000)
    //      nblendPaletteTowardPalette(currentPalette, targetPalette, 24);
    //#else
    currentPalette = targetPalette;
    //#endif

  //  backgroundLayer.swapBuffers();
   // leds = (CRGB*) backgroundLayer.backBuffer();
   // LEDS.countFPS();

	for (int y=0; y<VPANEL_H; ++y){
  	    for (int x=0; x<VPANEL_W; ++x){
		//Serial.printf("Flushing x, y coord %d, %d\n", x, y);
    		uint16_t _pixel = XY16(x,y);
    		virtualDisp->drawPixelRGB888( x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
	    } // end loop to copy fast led to the dma matrix
	}
  }

  // scale the brightness of the screenbuffer down
  void DimAll(byte value)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i].nscale8(value);
    }
  }  

  void ClearFrame()
  {
      memset(leds, 0x00, NUM_LEDS * sizeof(CRGB)); // flush
  }
  
	 
  
/*
  void CircleStream(uint8_t value) {
    DimAll(value); ShowFrame();

    for (uint8_t offset = 0; offset < MATRIX_CENTER_X; offset++) {
      boolean hasprev = false;
      uint16_t prevxy = 0;

      for (uint8_t theta = 0; theta < 255; theta++) {
        uint8_t x = mapcos8(theta, offset, (VPANEL_W - 1) - offset);
        uint8_t y = mapsin8(theta, offset, (VPANEL_H - 1) - offset);

        uint16_t xy = XY(x, y);

        if (hasprev) {
          leds[prevxy] += leds[xy];
        }

        prevxy = xy;
        hasprev = true;
      }
    }

    for (uint8_t x = 0; x < VPANEL_W; x++) {
      for (uint8_t y = 0; y < VPANEL_H; y++) {
        uint16_t xy = XY(x, y);
        leds[xy] = leds2[xy];
        leds[xy].nscale8(value);
        leds2[xy].nscale8(value);
      }
    }
  }
*/

  // palettes
  static const int paletteCount = 10;
  int paletteIndex = -1;
  TBlendType currentBlendType = LINEARBLEND;
  CRGBPalette16 currentPalette;
  CRGBPalette16 targetPalette;
  char* currentPaletteName;

  static const int HeatColorsPaletteIndex = 6;
  static const int RandomPaletteIndex = 9;

  void Setup() {
    currentPalette = RainbowColors_p;
    loadPalette(0);
    NoiseVariablesSetup();
  }

  void CyclePalette(int offset = 1) {
    loadPalette(paletteIndex + offset);
  }

  void RandomPalette() {
    loadPalette(RandomPaletteIndex);
  }

  void loadPalette(int index) {
    paletteIndex = index;

    if (paletteIndex >= paletteCount)
      paletteIndex = 0;
    else if (paletteIndex < 0)
      paletteIndex = paletteCount - 1;

    switch (paletteIndex) {
      case 0:
        targetPalette = RainbowColors_p;
        currentPaletteName = (char *)"Rainbow";
        break;
        //case 1:
        //  targetPalette = RainbowStripeColors_p;
        //  currentPaletteName = (char *)"RainbowStripe";
        //  break;
      case 1:
        targetPalette = OceanColors_p;
        currentPaletteName = (char *)"Ocean";
        break;
      case 2:
        targetPalette = CloudColors_p;
        currentPaletteName = (char *)"Cloud";
        break;
      case 3:
        targetPalette = ForestColors_p;
        currentPaletteName = (char *)"Forest";
        break;
      case 4:
        targetPalette = PartyColors_p;
        currentPaletteName = (char *)"Party";
        break;
      case 5:
        setupGrayscalePalette();
        currentPaletteName = (char *)"Grey";
        break;
      case HeatColorsPaletteIndex:
        targetPalette = HeatColors_p;
        currentPaletteName = (char *)"Heat";
        break;
      case 7:
        targetPalette = LavaColors_p;
        currentPaletteName = (char *)"Lava";
        break;
      case 8:
        setupIcePalette();
        currentPaletteName = (char *)"Ice";
        break;
      case RandomPaletteIndex:
        loadPalette(random(0, paletteCount - 1));
        paletteIndex = RandomPaletteIndex;
        currentPaletteName = (char *)"Random";
        break;
    }
  }

  void setPalette(String paletteName) {
    if (paletteName == "Rainbow")
      loadPalette(0);
    //else if (paletteName == "RainbowStripe")
    //  loadPalette(1);
    else if (paletteName == "Ocean")
      loadPalette(1);
    else if (paletteName == "Cloud")
      loadPalette(2);
    else if (paletteName == "Forest")
      loadPalette(3);
    else if (paletteName == "Party")
      loadPalette(4);
    else if (paletteName == "Grayscale")
      loadPalette(5);
    else if (paletteName == "Heat")
      loadPalette(6);
    else if (paletteName == "Lava")
      loadPalette(7);
    else if (paletteName == "Ice")
      loadPalette(8);
    else if (paletteName == "Random")
      RandomPalette();
  }

  void listPalettes() {
    Serial.println(F("{"));
    Serial.print(F("  \"count\": "));
    Serial.print(paletteCount);
    Serial.println(",");
    Serial.println(F("  \"results\": ["));

    String paletteNames [] = {
      "Rainbow",
      // "RainbowStripe",
      "Ocean",
      "Cloud",
      "Forest",
      "Party",
      "Grayscale",
      "Heat",
      "Lava",
      "Ice",
      "Random"
    };

    for (int i = 0; i < paletteCount; i++) {
      Serial.print(F("    \""));
      Serial.print(paletteNames[i]);
      if (i == paletteCount - 1)
        Serial.println(F("\""));
      else
        Serial.println(F("\","));
    }

    Serial.println("  ]");
    Serial.println("}");
  }

  void setupGrayscalePalette() {
    targetPalette = CRGBPalette16(CRGB::Black, CRGB::White);
  }

  void setupIcePalette() {
    targetPalette = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);
  }

  // Oscillators and Emitters

  // the oscillators: linear ramps 0-255
  byte osci[6];

  // sin8(osci) swinging between 0 to VPANEL_W - 1
  byte p[6];

  // set the speeds (and by that ratios) of the oscillators here
  void MoveOscillators() {
    osci[0] = osci[0] + 5;
    osci[1] = osci[1] + 2;
    osci[2] = osci[2] + 3;
    osci[3] = osci[3] + 4;
    osci[4] = osci[4] + 1;
    if (osci[4] % 2 == 0)
      osci[5] = osci[5] + 1; // .5
    for (int i = 0; i < 4; i++) {
      p[i] = map8(sin8(osci[i]), 0, VPANEL_W - 1); //why? to keep the result in the range of 0-VPANEL_W (matrix size)
    }
  }

 
  // All the caleidoscope functions work directly within the screenbuffer (leds array).
  // Draw whatever you like in the area x(0-15) and y (0-15) and then copy it arround.

  // rotates the first 16x16 quadrant 3 times onto a 32x32 (+90 degrees rotation for each one)
  void Caleidoscope1() {
    for (int x = 0; x < MATRIX_CENTER_X; x++) {
      for (int y = 0; y < MATRIX_CENTER_Y; y++) {
        leds[XY16(VPANEL_W - 1 - x, y)] = leds[XY16(x, y)];
        leds[XY16(VPANEL_W - 1 - x, VPANEL_H - 1 - y)] = leds[XY16(x, y)];
        leds[XY16(x, VPANEL_H - 1 - y)] = leds[XY16(x, y)];
      }
    }
  }


  // mirror the first 16x16 quadrant 3 times onto a 32x32
  void Caleidoscope2() {
    for (int x = 0; x < MATRIX_CENTER_X; x++) {
      for (int y = 0; y < MATRIX_CENTER_Y; y++) {
        leds[XY16(VPANEL_W - 1 - x, y)] = leds[XY16(y, x)];
        leds[XY16(x, VPANEL_H - 1 - y)] = leds[XY16(y, x)];
        leds[XY16(VPANEL_W - 1 - x, VPANEL_H - 1 - y)] = leds[XY16(x, y)];
      }
    }
  }

  // copy one diagonal triangle into the other one within a 16x16
  void Caleidoscope3() {
    for (int x = 0; x <= MATRIX_CENTRE_X && x < VPANEL_H; x++) {
      for (int y = 0; y <= x && y<VPANEL_H; y++) {
        leds[XY16(x, y)] = leds[XY16(y, x)];
      }
    }
  }

  // copy one diagonal triangle into the other one within a 16x16 (90 degrees rotated compared to Caleidoscope3)
  void Caleidoscope4() {
    for (int x = 0; x <= MATRIX_CENTRE_X; x++) {
      for (int y = 0; y <= MATRIX_CENTRE_Y - x; y++) {
        leds[XY16(MATRIX_CENTRE_Y - y, MATRIX_CENTRE_X - x)] = leds[XY16(x, y)];
      }
    }
  }

  // copy one diagonal triangle into the other one within a 8x8
  void Caleidoscope5() {
    for (int x = 0; x < VPANEL_W / 4; x++) {
      for (int y = 0; y <= x && y<=VPANEL_H; y++) {
        leds[XY16(x, y)] = leds[XY16(y, x)];
      }
    }

    for (int x = VPANEL_W / 4; x < VPANEL_W / 2; x++) {
      for (int y = VPANEL_H / 4; y >= 0; y--) {
        leds[XY16(x, y)] = leds[XY16(y, x)];
      }
    }
  }

  void Caleidoscope6() {
    for (int x = 1; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 7)] = leds[XY16(x, 0)];
    } //a
    for (int x = 2; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 6)] = leds[XY16(x, 1)];
    } //b
    for (int x = 3; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 5)] = leds[XY16(x, 2)];
    } //c
    for (int x = 4; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 4)] = leds[XY16(x, 3)];
    } //d
    for (int x = 5; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 3)] = leds[XY16(x, 4)];
    } //e
    for (int x = 6; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 2)] = leds[XY16(x, 5)];
    } //f
    for (int x = 7; x < MATRIX_CENTER_X; x++) {
      leds[XY16(7 - x, 1)] = leds[XY16(x, 6)];
    } //g
  }

  // create a square twister to the left or counter-clockwise
  // x and y for center, r for radius
  void SpiralStream(int x, int y, int r, byte dimm) {
    for (int d = r; d >= 0; d--) { // from the outside to the inside
      for (int i = x - d; i <= x + d; i++) {
        leds[XY16(i, y - d)] += leds[XY16(i + 1, y - d)]; // lowest row to the right
        leds[XY16(i, y - d)].nscale8(dimm);
      }
      for (int i = y - d; i <= y + d; i++) {
        leds[XY16(x + d, i)] += leds[XY16(x + d, i + 1)]; // right colum up
        leds[XY16(x + d, i)].nscale8(dimm);
      }
      for (int i = x + d; i >= x - d; i--) {
        leds[XY16(i, y + d)] += leds[XY16(i - 1, y + d)]; // upper row to the left
        leds[XY16(i, y + d)].nscale8(dimm);
      }
      for (int i = y + d; i >= y - d; i--) {
        leds[XY16(x - d, i)] += leds[XY16(x - d, i - 1)]; // left colum down
        leds[XY16(x - d, i)].nscale8(dimm);
      }
    }
  }

  // expand everything within a circle
  void Expand(int centerX, int centerY, int radius, byte dimm) {
    if (radius == 0)
      return;

    int currentRadius = radius;

    while (currentRadius > 0) {
      int a = radius, b = 0;
      int radiusError = 1 - a;

      int nextRadius = currentRadius - 1;
      int nextA = nextRadius - 1, nextB = 0;
      int nextRadiusError = 1 - nextA;

      while (a >= b)
      {
        // move them out one pixel on the radius
        leds[XY16(a + centerX, b + centerY)] = leds[XY16(nextA + centerX, nextB + centerY)];
        leds[XY16(b + centerX, a + centerY)] = leds[XY16(nextB + centerX, nextA + centerY)];
        leds[XY16(-a + centerX, b + centerY)] = leds[XY16(-nextA + centerX, nextB + centerY)];
        leds[XY16(-b + centerX, a + centerY)] = leds[XY16(-nextB + centerX, nextA + centerY)];
        leds[XY16(-a + centerX, -b + centerY)] = leds[XY16(-nextA + centerX, -nextB + centerY)];
        leds[XY16(-b + centerX, -a + centerY)] = leds[XY16(-nextB + centerX, -nextA + centerY)];
        leds[XY16(a + centerX, -b + centerY)] = leds[XY16(nextA + centerX, -nextB + centerY)];
        leds[XY16(b + centerX, -a + centerY)] = leds[XY16(nextB + centerX, -nextA + centerY)];

        // dim them
        leds[XY16(a + centerX, b + centerY)].nscale8(dimm);
        leds[XY16(b + centerX, a + centerY)].nscale8(dimm);
        leds[XY16(-a + centerX, b + centerY)].nscale8(dimm);
        leds[XY16(-b + centerX, a + centerY)].nscale8(dimm);
        leds[XY16(-a + centerX, -b + centerY)].nscale8(dimm);
        leds[XY16(-b + centerX, -a + centerY)].nscale8(dimm);
        leds[XY16(a + centerX, -b + centerY)].nscale8(dimm);
        leds[XY16(b + centerX, -a + centerY)].nscale8(dimm);

        b++;
        if (radiusError < 0)
          radiusError += 2 * b + 1;
        else
        {
          a--;
          radiusError += 2 * (b - a + 1);
        }

        nextB++;
        if (nextRadiusError < 0)
          nextRadiusError += 2 * nextB + 1;
        else
        {
          nextA--;
          nextRadiusError += 2 * (nextB - nextA + 1);
        }
      }

      currentRadius--;
    }
  }

  // give it a linear tail to the right
  void StreamRight(byte scale, int fromX = 0, int toX = VPANEL_W, int fromY = 0, int toY = VPANEL_H)
  {
    for (int x = fromX + 1; x < toX; x++) {
      for (int y = fromY; y < toY; y++) {
        leds[XY16(x, y)] += leds[XY16(x - 1, y)];
        leds[XY16(x, y)].nscale8(scale);
      }
    }
    for (int y = fromY; y < toY; y++)
      leds[XY16(0, y)].nscale8(scale);
  }

  // give it a linear tail to the left
  void StreamLeft(byte scale, int fromX = VPANEL_W, int toX = 0, int fromY = 0, int toY = VPANEL_H)
  {
    for (int x = toX; x < fromX; x++) {
      for (int y = fromY; y < toY; y++) {
        leds[XY16(x, y)] += leds[XY16(x + 1, y)];
        leds[XY16(x, y)].nscale8(scale);
      }
    }
    for (int y = fromY; y < toY; y++)
      leds[XY16(0, y)].nscale8(scale);
  }

  // give it a linear tail downwards
  void StreamDown(byte scale)
  {
    for (int x = 0; x < VPANEL_W; x++) {
      for (int y = 1; y < VPANEL_H; y++) {
        leds[XY16(x, y)] += leds[XY16(x, y - 1)];
        leds[XY16(x, y)].nscale8(scale);
      }
    }
    for (int x = 0; x < VPANEL_W; x++)
      leds[XY16(x, 0)].nscale8(scale);
  }

  // give it a linear tail upwards
  void StreamUp(byte scale)
  {
    for (int x = 0; x < VPANEL_W; x++) {
      for (int y = VPANEL_H - 2; y >= 0; y--) {
        leds[XY16(x, y)] += leds[XY16(x, y + 1)];
        leds[XY16(x, y)].nscale8(scale);
      }
    }
    for (int x = 0; x < VPANEL_W; x++)
      leds[XY16(x, VPANEL_H - 1)].nscale8(scale);
  }

  // give it a linear tail up and to the left
  void StreamUpAndLeft(byte scale)
  {
    for (int x = 0; x < VPANEL_W - 1; x++) {
      for (int y = VPANEL_H - 2; y >= 0; y--) {
        leds[XY16(x, y)] += leds[XY16(x + 1, y + 1)];
        leds[XY16(x, y)].nscale8(scale);
      }
    }
    for (int x = 0; x < VPANEL_W; x++)
      leds[XY16(x, VPANEL_H - 1)].nscale8(scale);
    for (int y = 0; y < VPANEL_H; y++)
      leds[XY16(VPANEL_W - 1, y)].nscale8(scale);
  }

  // give it a linear tail up and to the right
  void StreamUpAndRight(byte scale)
  {
    for (int x = 0; x < VPANEL_W - 1; x++) {
      for (int y = VPANEL_H - 2; y >= 0; y--) {
        leds[XY16(x + 1, y)] += leds[XY16(x, y + 1)];
        leds[XY16(x, y)].nscale8(scale);
      }
    }
    // fade the bottom row
    for (int x = 0; x < VPANEL_W; x++)
      leds[XY16(x, VPANEL_H - 1)].nscale8(scale);

    // fade the right column
    for (int y = 0; y < VPANEL_H; y++)
      leds[XY16(VPANEL_W - 1, y)].nscale8(scale);
  }

  // just move everything one line down
  void MoveDown() {
    for (int y = VPANEL_H - 1; y > 0; y--) {
      for (int x = 0; x < VPANEL_W; x++) {
        leds[XY16(x, y)] = leds[XY16(x, y - 1)];
      }
    }
  }

  // just move everything one line down
  void VerticalMoveFrom(int start, int end) {
    for (int y = end; y > start; y--) {
      for (int x = 0; x < VPANEL_W; x++) {
        leds[XY16(x, y)] = leds[XY16(x, y - 1)];
      }
    }
  }

  // copy the rectangle defined with 2 points x0, y0, x1, y1
  // to the rectangle beginning at x2, x3
  void Copy(byte x0, byte y0, byte x1, byte y1, byte x2, byte y2) {
    for (int y = y0; y < y1 + 1; y++) {
      for (int x = x0; x < x1 + 1; x++) {
        leds[XY16(x + x2 - x0, y + y2 - y0)] = leds[XY16(x, y)];
      }
    }
  }

  // rotate + copy triangle (MATRIX_CENTER_X*MATRIX_CENTER_X)
  void RotateTriangle() {
    for (int x = 1; x < MATRIX_CENTER_X; x++) {
      for (int y = 0; y < x; y++) {
        leds[XY16(x, 7 - y)] = leds[XY16(7 - x, y)];
      }
    }
  }

  // mirror + copy triangle (MATRIX_CENTER_X*MATRIX_CENTER_X)
  void MirrorTriangle() {
    for (int x = 1; x < MATRIX_CENTER_X; x++) {
      for (int y = 0; y < x; y++) {
        leds[XY16(7 - y, x)] = leds[XY16(7 - x, y)];
      }
    }
  }

  // draw static rainbow triangle pattern (MATRIX_CENTER_XxWIDTH / 2)
  // (just for debugging)
  void RainbowTriangle() {
    for (int i = 0; i < MATRIX_CENTER_X; i++) {
      for (int j = 0; j <= i; j++) {
        Pixel(7 - i, j, i * j * 4);
      }
    }
  }

  void BresenhamLine(int x0, int y0, int x1, int y1, byte colorIndex)
  {
    BresenhamLine(x0, y0, x1, y1, ColorFromCurrentPalette(colorIndex));
  }

  void BresenhamLine(int x0, int y0, int x1, int y1, CRGB color)
  {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
      leds[XY16(x0, y0)] += color;
      if (x0 == x1 && y0 == y1) break;
      e2 = 2 * err;
      if (e2 > dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }
  }

  // write one pixel with the specified color from the current palette to coordinates
  /*
  void Pixel(int x, int y, uint8_t colorIndex) {
    leds[XY(x, y)] = ColorFromCurrentPalette(colorIndex);
    matrix.drawBackgroundPixelRGB888(x,y, leds[XY(x, y)]); // now draw it?
  }
  */

  CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
    return ColorFromPalette(currentPalette, index, brightness, currentBlendType);
  }

  CRGB HsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
    CHSV hsv = CHSV(h, s, v);
    CRGB rgb;
    hsv2rgb_spectrum(hsv, rgb);
    return rgb;
  }

  void NoiseVariablesSetup() {
    noisesmoothing = 200;

    noise_x = random16();
    noise_y = random16();
    noise_z = random16();
    noise_scale_x = 6000;
    noise_scale_y = 6000;
  }

  void FillNoise() {
    for (uint16_t i = 0; i < VPANEL_W; i++) {
      uint32_t ioffset = noise_scale_x * (i - MATRIX_CENTRE_Y);

      for (uint16_t j = 0; j < VPANEL_H; j++) {
        uint32_t joffset = noise_scale_y * (j - MATRIX_CENTRE_Y);

        byte data = inoise16(noise_x + ioffset, noise_y + joffset, noise_z) >> 8;

        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8(olddata, noisesmoothing) + scale8(data, 256 - noisesmoothing);
        data = newdata;

        noise[i][j] = data;
      }
    }
  }

  // non leds2 memory version.
  void MoveX(byte delta) 
  {

    CRGB tmp = 0;

    for (int y = 0; y < VPANEL_H; y++) 
    {
 
      // Shift Left: https://codedost.com/c/arraypointers-in-c/c-program-shift-elements-array-left-direction/
      // Computationally heavier but doesn't need an entire leds2 array

      tmp = leds[XY16(0, y)];      
      for (int m = 0; m < delta; m++)
      {
        // Do this delta time for each row... computationally expensive potentially.
        for(int x = 0; x < VPANEL_W; x++)
        {
            leds[XY16(x, y)] = leds [XY16(x+1, y)];
        }

        leds[XY16(VPANEL_W-1, y)] = tmp;
      }
   

      /*
      // Shift
      for (int x = 0; x < VPANEL_W - delta; x++) {
        leds2[XY(x, y)] = leds[XY(x + delta, y)];
      }

      // Wrap around
      for (int x = VPANEL_W - delta; x < VPANEL_W; x++) {
        leds2[XY(x, y)] = leds[XY(x + delta - VPANEL_W, y)];
      }
      */
    } // end row loop

    /*
    // write back to leds
    for (uint8_t y = 0; y < VPANEL_H; y++) {
      for (uint8_t x = 0; x < VPANEL_W; x++) {
        leds[XY(x, y)] = leds2[XY(x, y)];
      }
    }
    */
  }

  void MoveY(byte delta)
  {
    
    CRGB tmp = 0; 
    for (int x = 0; x < VPANEL_W; x++) 
    {
      tmp = leds[XY16(x, 0)];      
      for (int m = 0; m < delta; m++) // moves
      {
        // Do this delta time for each row... computationally expensive potentially.
        for(int y = 0; y < VPANEL_H; y++)
        {
            leds[XY16(x, y)] = leds [XY16(x, y+1)];
        }

        leds[XY16(x, VPANEL_H-1)] = tmp;
      }
    } // end column loop
  } /// MoveY

  
};

#endif
