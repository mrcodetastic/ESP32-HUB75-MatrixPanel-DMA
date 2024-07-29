// Codetastic 2024
// ChatGPT was used to create this.

#ifndef PatternStarfield_H
#define PatternStarfield_H

#include <vector>

#define STAR_COUNT 128

// Star structure
struct Star {
    float x, y, z;
    CRGB colour;
};



class PatternStarfield : public Drawable {

  private:

  std::vector<Star> stars;
  float speed = 0.5f;

  public:
    PatternStarfield() {
      name = (char *)"Starfield";
    }

    void start()
    {

        stars.resize(STAR_COUNT);

        for (int i = 0; i < STAR_COUNT; ++i) {
                stars[i] = { static_cast<float>(rand() % VPANEL_W - VPANEL_W / 2),
                            static_cast<float>(rand() % VPANEL_H - VPANEL_H / 2),
                            static_cast<float>(rand() % VPANEL_W),
                            effects.ColorFromCurrentPalette(rand()*255)
                            };
         } // random positions
    };

    unsigned int drawFrame() {

      effects.DimAll(250);

      // Update star positions
      for (auto& star : stars) {
            star.z -= speed;
            if (star.z <= 0) {
                star.z = VPANEL_W;
                star.x = static_cast<float>(rand() % VPANEL_W - VPANEL_W / 2);
                star.y = static_cast<float>(rand() % VPANEL_H - VPANEL_H / 2);
            }
        }

      // draw position
      for (const auto& star : stars) {
          float k = 128.0f / star.z;
          int x = static_cast<int>(star.x * k + VPANEL_W / 2);
          int y = static_cast<int>(star.y * k + VPANEL_H / 2);

          if (x >= 0 && x < VPANEL_W && y >= 0 && y < VPANEL_H) {

              // TODO: Get brighter as we get closer to edges?
              effects.setPixel(x, y, star.colour);
          }
      }

      effects.ShowFrame();
      return 0;
    }
};

#endif
