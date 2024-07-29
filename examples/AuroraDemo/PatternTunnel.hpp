// Codetastic 2024
// ChatGPT was used to create this.

#ifndef PatternTunnel_H
#define PatternTunnel_H

class PatternTunnel : public Drawable {

  private:

  uint8_t circlePositions[5] = {0, 6, 12, 18, 24}; // Initial positions of circles


  public:
    PatternTunnel() {
      name = (char *)"Tunnel";
    }

    

    // Function to draw a circle on the matrix
    void drawCircle(int centerX, int centerY, int radius, CRGB color) {
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          if (x*x + y*y <= radius*radius) {
            int drawX = centerX + x;
            int drawY = centerY + y;
            if (drawX >= 0 && drawX < VPANEL_W && drawY >= 0 && drawY < VPANEL_H) {
              effects.leds[XY(drawX, drawY)] = color;
            }
          }
        }
      }
    } 

    void start()
    {
       
    };

    unsigned int drawFrame() {

      effects.DimAll(250);


      // Draw circles
      for (int i = 0; i < 5; i++) {
        int radius = circlePositions[i] % 32;
        CRGB color = CHSV(map(radius, 0, 31, 0, 255), 255, 255);
        drawCircle(VPANEL_W / 2, VPANEL_H / 2, radius, color);
        
        // Move circles forward
        circlePositions[i]++;
        
        // Reset the position if the circle is out of bounds
        if (circlePositions[i] >= 32) {
          circlePositions[i] = 0;
        }
      }

      effects.ShowFrame();
      return 0;
    }
};

#endif
