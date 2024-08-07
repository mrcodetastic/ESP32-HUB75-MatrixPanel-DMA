/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Original Copyright (c) 2014 Jason Coon
 * 
 * Modified by Codetastic 2024
 * 
*/

#ifndef Patterns_H
#define Patterns_H

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#include "Vector2.hpp"
#include "Boid.hpp"
#include "Attractor.hpp"

/* 
 *  Note from mrfaptastic:
 *  
 *  Commented out patterns are due to the fact they either didn't work properly with a non-square display,
 *  or from my personal opinion, are crap. 
 */
#include "PatternStarfield.hpp" // new 2024
#include "PatternAttract.hpp"
#include "PatternBounce.hpp" // Doesn't seem to work, omitting.
#include "PatternCube.hpp" // Doesn't seem to work, omitting.
#include "PatternElectricMandala.hpp"
#include "PatternFireKoz.hpp" // Doesn't seem to work, omitting.
#include "PatternFlock.hpp"
#include "PatternFlowField.hpp"
#include "PatternIncrementalDrift.hpp"
#include "PatternIncrementalDrift2.hpp" // Doesn't seem to work, omitting.
#include "PatternInfinity.hpp"
#include "PatternMaze.hpp" // ??
#include "PatternMunch.hpp"
//#include "PatternNoiseSmearing.hpp"
#include "PatternPendulumWave.hpp"
#include "PatternPlasma.hpp"
#include "PatternRadar.hpp"
#include "PatternSimplexNoise.hpp"
#include "PatternSnake.hpp"
#include "PatternSpiral.hpp"
#include "PatternSpiro.hpp"
#include "PatternWave.hpp" 
#include "PatternRain.hpp"
#include "PatternJuliaSetFractal.hpp"
#include "PatternRain.hpp"
#include "PatternFireworks.hpp"



class Patterns {
  private:

    PatternStarfield starfield;
    PatternAttract attract;
    PatternBounce bounce;
    PatternCube cube;
    PatternElectricMandala electricMandala;
    PatternFireKoz fire;
    PatternFlock flock;
    PatternFlowField flowField;
    PatternIncrementalDrift incrementalDrift;
    PatternIncrementalDrift2 incrementalDrift2;
    PatternInfinity infinity;
    PatternMaze maze;    
    PatternMunch munch;
    PatternPendulumWave pendwave;
    PatternPlasma plasma;
    PatternRadar radar;
    PatternSimplexNoise simpnoise;
    PatternSnake snake;
    PatternSpiral spiral;
    PatternSpiro spiro;
    PatternWave wave;
    PatternJuliaSet juliaSet;
    
    PatternRain rain;
    PatternFirework fireworks;
    

    std::vector<Drawable*> availablePatterns = {
      &juliaSet,
      &starfield,
      &attract,
      &bounce,
      &cube,
      &electricMandala,
      &fire,
      &flock,
      &spiro,
      &radar,
      &flowField,
      &incrementalDrift,
      &incrementalDrift2,
      &infinity,
      &maze,
      &munch,
      &pendwave,
      &plasma,
      &radar,
      &simpnoise,
      &spiro,
      &wave,
      &rain,
      &fireworks
    };    

    int currentIndex = 0;
    Drawable* currentItem;

    int getCurrentIndex() {
      return currentIndex;
    }


  public:
    Patterns() {
      this->currentItem = availablePatterns[0];
      this->currentItem->start();
    }

    void stop() {
      if (currentItem)
        currentItem->stop();
    }

    void start() {
      if (currentItem)
        currentItem->start();
    }

    void moveTo(int index) 
    {
      index = ((index >= availablePatterns.size()) || (index < 0)) ? 0 : index;

      if (currentItem)
        currentItem->stop();

      currentIndex = index;
      currentItem = availablePatterns[currentIndex];

      Serial.print("Changing pattern to:  ");
      Serial.println(getCurrentPatternName());      

      if (currentItem)
        currentItem->start();

    } // index   


    void move(int step) {

      currentIndex += step;

      if (currentIndex >= availablePatterns.size()) currentIndex = 0;
      else if (currentIndex < 0) currentIndex = 0;

      moveTo(currentIndex);
    }

    void moveRandom(int step) {
      int rand_index =  random(0, availablePatterns.size()-1);
      moveTo(rand_index);
    }

    unsigned int drawFrame() {
      return currentItem->drawFrame();
    }

    void listPatterns() {
      Serial.println(F("{"));
      Serial.print(F("  \"count\": "));
      Serial.print(availablePatterns.size());
      Serial.println(",");
      Serial.println(F("  \"results\": ["));

      for (size_t i = 0; i < availablePatterns.size(); i++) {
        Serial.print(F("    \""));
        Serial.print(i, DEC);
        Serial.print(F(": "));
        Serial.print(availablePatterns[i]->name);
        if (i == availablePatterns.size() - 1)
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
/*
    bool setPattern(String name) {

      for (size_t i = 0; i < availablePatterns.size(); i++) {
        if (name.compareTo(availablePatterns[i]->name) == 0) {
          moveTo(i);
          return true;
        }
      }

      return false;
    }

*/
    bool setPattern(int index) {
      if (index >= availablePatterns.size() || index < 0)
        return false;

      moveTo(index);

      return true;
    }
};

#endif