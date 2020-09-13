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
 *  
 *  Kosso: I have removed the crappy ones and added a less crappy (and working!) Fire demo ;) 
 *  
 */
#include "PaletteFireKoz.h" // Added by Kosso
#include "PatternFireKoz.h" // Added by Kosso
#include "PatternSpiro.h"
#include "PatternRadar.h"
#include "PatternSwirl.h"
#include "PatternPendulumWave.h"
#include "PatternFlowField.h"
#include "PatternIncrementalDrift.h"
#include "PatternMunch.h"
#include "PatternElectricMandala.h"
#include "PatternSimplexNoise.h"
#include "PatternWave.h"
#include "PatternAttract.h"
#include "PatternFlock.h"
#include "PatternInfinity.h"
#include "PatternPlasma.h"
#include "PatternSnake.h"
#include "PatternFire.h" // Not very good.
#include "PatternLife.h"
#include "PatternMaze.h"
#include "PatternSpiral.h"


class Patterns : public Playlist {
  private:
    PatternFireKoz fireKoz;
    PatternSpiro spiro;
    PatternSwirl swirl;
    PatternPendulumWave pendulumWave;
    PatternFlowField flowField;
    PatternIncrementalDrift incrementalDrift;
    PatternMunch munch;
    PatternElectricMandala electricMandala;
    PatternSimplexNoise simplexNoise;
    PatternWave wave;
    PatternAttract attract;
    PatternFlock flock;
    PatternPlasma plasma;
    PatternSnake snake;
    PatternFire fire;
    PatternLife life;
    PatternMaze maze;
    PatternSpiral spiral;

    int currentIndex = 0;
    Drawable* currentItem;

    int getCurrentIndex() {
      return currentIndex;
    }

    const static int PATTERN_COUNT = 18; 

    Drawable* shuffledItems[PATTERN_COUNT];

    Drawable* items[PATTERN_COUNT] = {
      &fireKoz, // added by Kosso
      &spiro,           
      &life,            
      &flowField,
      &pendulumWave,    
      &incrementalDrift,
      &munch,           
      &electricMandala, 
      &simplexNoise,    
      &wave,            
      &attract,         
      &swirl,  
      &flock, 
      &plasma, 
      &snake, 
      &fire, 
      &maze,
      &spiral, 
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

    int getPatternIndex()
    {
      return currentIndex;
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
