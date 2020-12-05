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

#ifndef Patterns_H
#define Patterns_H

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#include "Vector.h"
#include "Boid.h"
#include "Attractor.h"

/* 
 *  Note from mrfaptastic:
 *  
 *  Commented out patterns are due to the fact they either didn't work properly with a non-square display,
 *  or from my personal opinion, are crap. 
 */

#include "PatternTest.h"
//#include "PatternNoiseSmearing.h" // Doesn't seem to work, omitting.
#include "PatternSpiro.h"
#include "PatternRadar.h"
#include "PatternSwirl.h"
#include "PatternPendulumWave.h"
#include "PatternFlowField.h"
#include "PatternIncrementalDrift.h"
//#include "PatternIncrementalDrift2.h" // Doesn't seem to work, omitting.
#include "PatternMunch.h"
#include "PatternElectricMandala.h"
//#include "PatternSpin.h" // Doesn't seem to work, omitting.
#include "PatternSimplexNoise.h"
#include "PatternWave.h"
#include "PatternAttract.h"
//#include "PatternBounce.h" // Doesn't seem to work, omitting.
#include "PatternFlock.h"
#include "PatternInfinity.h"
#include "PatternPlasma.h"
#include "PatternSnake.h"
#include "PatternInvaders.h"
//#include "PatternCube.h" // Doesn't seem to work, omitting.
//#include "PatternFire.h" // Doesn't seem to work, omitting.
#include "PatternLife.h"
#include "PatternMaze.h"
//#include "PatternPulse.h" // Doesn't seem to work, omitting.
//#include "PatternSpark.h" // Doesn't seem to work, omitting.
#include "PatternSpiral.h"

class Patterns : public Playlist {
  private:
    PatternTest patternTest;
 //   PatternRainbowFlag rainbowFlag; // doesn't work
 //   PatternPaletteSmear paletteSmear;
 //   PatternMultipleStream multipleStream;   // doesn't work
 //   PatternMultipleStream2 multipleStream2; // doesn't work
 //   PatternMultipleStream3 multipleStream3; // doesn't work
 //   PatternMultipleStream4 multipleStream4; // doesn't work
 //   PatternMultipleStream5 multipleStream5; // doesn't work
 //   PatternMultipleStream8 multipleStream8; // doesn't work
    PatternSpiro spiro;
 //   PatternRadar radar;
    PatternSwirl swirl;
    PatternPendulumWave pendulumWave;
    PatternFlowField flowField;
    PatternIncrementalDrift incrementalDrift;
 //   PatternIncrementalDrift2 incrementalDrift2;
    PatternMunch munch;
    PatternElectricMandala electricMandala;
 //   PatternSpin spin;
    PatternSimplexNoise simplexNoise;
    PatternWave wave;
    PatternAttract attract;
 //   PatternBounce bounce;
    PatternFlock flock;
    PatternInfinity infinity;
    PatternPlasma plasma;
    PatternInvadersSmall invadersSmall;
 //   PatternInvadersMedium invadersMedium;
 //   PatternInvadersLarge invadersLarge;
    PatternSnake snake;
 //   PatternCube cube;
 //   PatternFire fire;
    PatternLife life;
    PatternMaze maze;
 //   PatternPulse pulse;
 //  PatternSpark spark;
    PatternSpiral spiral;

    int currentIndex = 0;
    Drawable* currentItem;

    int getCurrentIndex() {
      return currentIndex;
    }

    //const static int PATTERN_COUNT = 37;   

    const static int PATTERN_COUNT = 17;

    Drawable* shuffledItems[PATTERN_COUNT];

    Drawable* items[PATTERN_COUNT] = {
   //   &patternTest,     // ok 
      &spiro,           // cool
   //   &paletteSmear,  // fail
   //   &multipleStream, // fail
   //   &multipleStream8,// fail
   //   &multipleStream5,// fail
   //   &multipleStream3,// fail
   //    &radar, // fail
   //   &multipleStream4, // fail
   //   &multipleStream2, // fail
      &life, // ok
      &flowField,
      &pendulumWave, //11 ok

      &incrementalDrift, //12 ok
   //   &incrementalDrift2, // 13 fail
      &munch, // 14 ok
      &electricMandala, // 15 ok
   //   &spin, // 16 ok but repeditivev
      &simplexNoise, // 17 - cool!
   //   &wave, // 18 ok (can't work with 256+ matrix due to uint8_t vars)
   //   &rainbowFlag, //20 // fail
      &attract, // 21 ok
      &swirl, // 22
   //   &bounce, // boncing line crap
      &flock, // works
      &infinity, // works
      &plasma, // works
      &invadersSmall, // works ish
   //   &invadersMedium, // fail
   //   &invadersLarge, // fail
      &snake, // ok
   //   &cube, // works ish
   //   &fire, // ok ish
      &maze, // ok
    //  &pulse,// fail
    //  &spark, // same as fire
      &spiral, // ok
    };

  public:
    Patterns() {
      // add the items to the shuffledItems array
      for (int a = 0; a < PATTERN_COUNT; a++) {
        shuffledItems[a] = items[a];
      }

      shuffleItems();

      this->currentItem = items[0];
      this->currentItem->start();
    }

    char* Drawable::name = (char *)"Patterns";

    void stop() {
      if (currentItem)
        currentItem->stop();
    }

    void start() {
      if (currentItem)
        currentItem->start();
    }

    void move(int step) {
      currentIndex += step;

      if (currentIndex >= PATTERN_COUNT) currentIndex = 0;
      else if (currentIndex < 0) currentIndex = PATTERN_COUNT - 1;

      if (effects.paletteIndex == effects.RandomPaletteIndex)
        effects.RandomPalette();

      moveTo(currentIndex);

      //if (!isTimeAvailable && currentItem == &analogClock)
     //   move(step);
    }

    void moveRandom(int step) {
      currentIndex += step;

      if (currentIndex >= PATTERN_COUNT) currentIndex = 0;
      else if (currentIndex < 0) currentIndex = PATTERN_COUNT - 1;

      if (effects.paletteIndex == effects.RandomPaletteIndex)
        effects.RandomPalette();

      if (currentItem)
        currentItem->stop();

      currentItem = shuffledItems[currentIndex];

      if (currentItem)
        currentItem->start();

     // if (!isTimeAvailable && currentItem == &analogClock)
     //   moveRandom(step);
    }

    void shuffleItems() {
      for (int a = 0; a < PATTERN_COUNT; a++)
      {
        int r = random(a, PATTERN_COUNT);
        Drawable* temp = shuffledItems[a];
        shuffledItems[a] = shuffledItems[r];
        shuffledItems[r] = temp;
      }
    }


    unsigned int drawFrame() {
      return currentItem->drawFrame();
    }

    void listPatterns() {
      Serial.println(F("{"));
      Serial.print(F("  \"count\": "));
      Serial.print(PATTERN_COUNT);
      Serial.println(",");
      Serial.println(F("  \"results\": ["));

      for (int i = 0; i < PATTERN_COUNT; i++) {
        Serial.print(F("    \""));
        Serial.print(i, DEC);
        Serial.print(F(": "));
        Serial.print(items[i]->name);
        if (i == PATTERN_COUNT - 1)
          Serial.println(F("\""));
        else
          Serial.println(F("\","));
      }

      Serial.println("  ]");
      Serial.println("}");
    }

    char * getCurrentPatternName()
    {
      return currentItem->name;      
    }

    void moveTo(int index) {
      if (currentItem)
        currentItem->stop();

      currentIndex = index;

      currentItem = items[currentIndex];

      if (currentItem)
        currentItem->start();
    }    

    bool setPattern(String name) {
      for (int i = 0; i < PATTERN_COUNT; i++) {
        if (name.compareTo(items[i]->name) == 0) {
          moveTo(i);
          return true;
        }
      }

      return false;
    }


    bool setPattern(int index) {
      if (index >= PATTERN_COUNT || index < 0)
        return false;

      moveTo(index);

      return true;
    }
};

#endif
