/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Original Copyright (c) 2014 Jason Coon
 * 
 * Modified by Codetastic 2024
 * 
*/

#ifndef Patterns_H
#define Patterns_H

#include <array>

#include "Vector2.hpp"
#include "Boid.hpp"
#include "Attractor.hpp"

// Pattern implementations share the effects framebuffer and use load()/unload()
// only for sizable optional state; pattern objects themselves remain static.
#include "PatternStarfield.hpp" // new 2024
#include "PatternAttract.hpp"
#include "PatternBounce.hpp"
#include "PatternCube.hpp"
#include "PatternElectricMandala.hpp"
#include "PatternFireKoz.hpp"
#include "PatternFlock.hpp"
#include "PatternFlowField.hpp"
#include "PatternIncrementalDrift.hpp"
#include "PatternIncrementalDrift2.hpp"
#include "PatternInfinity.hpp"
#include "PatternMaze.hpp"
#include "PatternMunch.hpp"
#include "PatternGameOfLife.hpp"
//#include "PatternNoiseSmearing.hpp"
#include "PatternPendulumWave.hpp"
#include "PatternPlasma.hpp"
#include "PatternRadar.hpp"
#include "PatternSimplexNoise.hpp"
#include "PatternSpiral.hpp"
#include "PatternSpiro.hpp"
#include "PatternWave.hpp" 
#include "PatternRain.hpp"
#include "PatternJuliaSetFractal.hpp"
#include "PatternFireworks.hpp"
#include "PatternVoronoi.hpp"



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
    PatternGameOfLife gameOfLife;
    PatternPendulumWave pendwave;
    PatternPlasma plasma;
    PatternRadar radar;
    PatternSimplexNoise simpnoise;
    PatternSpiral spiral;
    PatternSpiro spiro;
    PatternWave wave;
    PatternJuliaSet juliaSet;
    
    PatternRain rain;
    PatternFirework fireworks;
    PatternVoronoi voronoi;
    

    std::array<Drawable*, 25> availablePatterns = {{
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
      &gameOfLife,
      &pendwave,
      &plasma,
      &simpnoise,
      &spiral,
      &wave,
      &rain,
      &fireworks,
      &voronoi
    }};

    int currentIndex = 0;
    Drawable* currentItem = nullptr;
    bool currentLoaded = false;

    void deactivateCurrent() {
      if (!currentItem || !currentLoaded) return;
      currentItem->stop();
      currentItem->unload();
      currentLoaded = false;
    }

    bool activateCurrent() {
      if (!currentItem || currentLoaded) return currentLoaded;
      if (!currentItem->load()) {
        Serial.print(F("Failed to load pattern: "));
        Serial.println(currentItem->name);
        return false;
      }
      currentLoaded = true;
      currentItem->start();
      return true;
    }


  public:
    Patterns() : currentItem(availablePatterns[0]) {}

    void stop() {
      deactivateCurrent();
    }

    void start() {
      activateCurrent();
    }

    void moveTo(int index) 
    {
      const int count = static_cast<int>(availablePatterns.size());
      if (count == 0) return;
      index %= count;
      if (index < 0) index += count;

      deactivateCurrent();

      currentIndex = index;
      currentItem = availablePatterns[currentIndex];

      Serial.print("Changing pattern to:  ");
      Serial.println(getCurrentPatternName());      

      activateCurrent();

    } // index   


    void move(int step) {

      const int count = static_cast<int>(availablePatterns.size());
      if (count == 0) return;
      moveTo((currentIndex + step % count + count) % count);
    }

    void moveRandom(int step = 0) {
      (void)step;
      if (availablePatterns.empty()) return;
      int rand_index = random(static_cast<long>(availablePatterns.size()));
      moveTo(rand_index);
    }

    unsigned int drawFrame() {
      return (currentLoaded && currentItem) ? currentItem->drawFrame() : 100;
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

    const char * getCurrentPatternName()
    {
      return currentItem ? currentItem->name : "None";
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
      if (index >= static_cast<int>(availablePatterns.size()) || index < 0)
        return false;

      moveTo(index);

      return true;
    }
};

#endif
