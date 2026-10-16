#ifndef PatternVoronoi_H
#define PatternVoronoi_H

class PatternVoronoi : public Drawable {
  private:
    static const uint8_t SITE_COUNT = 8;

    struct Site {
      float x;
      float y;
      float vx;
      float vy;
    };

    Site sites[SITE_COUNT];
    uint32_t frameNumber = 0;

    void moveSites() {
      for (uint8_t i = 0; i < SITE_COUNT; ++i) {
        Site& site = sites[i];
        site.x += site.vx;
        site.y += site.vy;

        if (site.x < 0) {
          site.x = 0;
          site.vx = -site.vx;
        } else if (site.x > VPANEL_W - 1) {
          site.x = VPANEL_W - 1;
          site.vx = -site.vx;
        }

        if (site.y < 0) {
          site.y = 0;
          site.vy = -site.vy;
        } else if (site.y > VPANEL_H - 1) {
          site.y = VPANEL_H - 1;
          site.vy = -site.vy;
        }
      }
    }

  public:
    PatternVoronoi() {
      name = "Drifting Voronoi";
    }

    void start() override {
      frameNumber = 0;
      effects.ClearFrame();

      for (uint8_t i = 0; i < SITE_COUNT; ++i) {
        sites[i].x = random(VPANEL_W);
        sites[i].y = random(VPANEL_H);
        const float speedX = random(20, 61) * 0.01f;
        const float speedY = random(20, 61) * 0.01f;
        sites[i].vx = random(2) ? speedX : -speedX;
        sites[i].vy = random(2) ? speedY : -speedY;
      }
    }

    unsigned int drawFrame() override {
      if (!effects.isReady() || VPANEL_W <= 0 || VPANEL_H <= 0) return 100;

      ++frameNumber;
      for (int y = 0; y < VPANEL_H; ++y) {
        for (int x = 0; x < VPANEL_W; ++x) {
          float nearest = 1.0e9f;
          float secondNearest = 1.0e9f;
          uint8_t nearestSite = 0;

          for (uint8_t i = 0; i < SITE_COUNT; ++i) {
            const float dx = x - sites[i].x;
            const float dy = y - sites[i].y;
            const float distance = dx * dx + dy * dy;
            if (distance < nearest) {
              secondNearest = nearest;
              nearest = distance;
              nearestSite = i;
            } else if (distance < secondNearest) {
              secondNearest = distance;
            }
          }

          // A narrow bright band marks the boundary between neighboring cells.
          if (secondNearest - nearest < 150.0f) {
            effects.setPixel(x, y, CRGB(180, 220, 255));
          } else {
            const uint8_t paletteIndex = static_cast<uint8_t>(
                nearestSite * 32 + frameNumber * 2);
            effects.setPixel(x, y, effects.ColorFromCurrentPalette(paletteIndex));
          }
        }
      }

      effects.ShowFrame();
      moveSites();
      return 35;
    }
};

#endif
