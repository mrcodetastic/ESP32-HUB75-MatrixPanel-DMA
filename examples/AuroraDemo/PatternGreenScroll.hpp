// Codetastic 2024
// ChatGPT was used to create this.
// It sucks.

#ifndef PatternTheMatrix_H
#define PatternTheMatrix_H

// Function to generate a random greenish color for the digital rain
CRGB generateRainColor() {
  return CHSV(96 + random(64), 255, 255); // Greenish colors
}


class PatternTheMatrix : public Drawable {

  public:
    PatternTheMatrix() {
      name = (char *)"The Matrix";
    }


    // Function to draw the digital rain effect
    void drawDigitalRain() {
      // Shift all the LEDs down by one row
      for (int x = 0; x < VPANEL_W ; x++) {
        for (int y = VPANEL_H - 1; y > 0; y--) {
          effects.leds[XY(x, y)] = effects.leds[XY(x, y - 1)];
        }
        // Add a new drop at the top of the column randomly
        if (random(10) > 7) { // Adjust the probability to control density of rain
          effects.leds[XY(x, 0)] = generateRainColor();
        } else {
          effects.leds[XY(x, 0)] = CRGB::Black;
        }
      }
    }
        

    void start()
    {
       
    };

    unsigned int drawFrame() {

      effects.DimAll(250);

       drawDigitalRain();

      effects.ShowFrame();
      return 0;
    }
};

#endif
