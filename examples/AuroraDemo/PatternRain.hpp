#ifndef PatternRain_H
#define PatternRain_H

// Codetastic 2024

struct rainDrop {
    uint8_t x;
    uint8_t y;
    CRGB colour;
};

#define MAX_RAINDROPS   128

class PatternRain : public Drawable {

  public:
    PatternRain() 
    {
        name = (char *)"PatternRain";
    }

    void start() {

        buffer = (uint16_t *) malloc(((VPANEL_W*VPANEL_H)+1)*sizeof(uint16_t)); // always alloc an extra amount for XY
    }

    void stop() {

      free(buffer);

    }


    unsigned int drawFrame() 
    {
        rain(32, 255, 224, 240, CRGB::Green);

      effects.ShowFrame();        

        return 45; // 1000/45 frames per secton
        
    }



  private:

      struct rainDrop  rainDrops[MAX_RAINDROPS];
      int    rainDropPos = 0;      

      uint16_t* buffer = NULL; // buffer of number

      void rain(byte backgroundDepth, byte maxBrightness, byte spawnFreq, byte tailLength, CRGB rainColor)
      {          
          CRGBPalette16 rain_p( CRGB::Black, rainColor );

          // Dim routine
          for (int16_t i = 0; i < VPANEL_W; i++) {
            for (int16_t j = 0; j < VPANEL_H; j++) {
              uint16_t xy = XY16(i, j);
              effects.leds[xy].nscale8(tailLength);
            }
          }    

          // Genrate a new raindrop if the randomness says we should           
          if (random(255) < spawnFreq) {

            // Find a spare raindrop slot
           for (int d = 0; d < MAX_RAINDROPS; d++)  {

              // This raindrop is done with, it has... dropped
              if (rainDrops[d].y >= VPANEL_H ) // not currently in use
              {
                  rainDrops[d].colour =  ColorFromPalette(rain_p, random(backgroundDepth, maxBrightness)); 
                  rainDrops[d].x      = random(VPANEL_W-1);
                  rainDrops[d].y      =  0;

                  break; // exit until next time.
              }             
           }            
          } // end random spawn

          // Iterate through all the rainDrops, draw the drop pixel on the layer
           for (int d = 0; d < MAX_RAINDROPS; d++)  {
              effects.setPixel(rainDrops[d].x, rainDrops[d].y++, rainDrops[d].colour);
           }
 
      }


};

#endif