#ifndef PatternRain_H
#define PatternRain_H

struct RainDrop {
    uint16_t x;
    uint16_t y;
    uint8_t trailLength;
    uint8_t speed;
    CRGB colour;
    bool active;
};

class PatternRain : public Drawable {
  private:
    static const uint8_t MAX_RAINDROPS = 96;
    static const uint8_t MAX_PUDDLE_ROWS = 5;

    RainDrop rainDrops[MAX_RAINDROPS];
    uint8_t puddleEnergy[(VPANEL_W > 0) ? VPANEL_W : 1];
    uint32_t frameNumber = 0;

    static uint8_t addSaturated(uint8_t value, uint8_t amount) {
      const uint16_t sum = static_cast<uint16_t>(value) + amount;
      return sum > 255 ? 255 : static_cast<uint8_t>(sum);
    }

    bool spawnDrop(const CRGBPalette16& palette) {
      for (uint8_t i = 0; i < MAX_RAINDROPS; ++i) {
        RainDrop& drop = rainDrops[i];
        if (drop.active) continue;

        drop.x = random(VPANEL_W);
        drop.y = 0;
        drop.trailLength = random(4, max(5, min(VPANEL_H, 14) + 1));
        drop.speed = random(1, 4); // stagger the streams like classic Matrix rain
        drop.colour = ColorFromPalette(palette, random(96, 256));
        drop.active = true;
        return true;
      }
      return false;
    }

    void addImpact(uint16_t x) {
      const uint16_t center = min(x, static_cast<uint16_t>(VPANEL_W - 1));
      puddleEnergy[center] = addSaturated(puddleEnergy[center], 76);

      // Spread a little energy sideways so impacts merge into a shallow pool.
      if (center > 0)
        puddleEnergy[center - 1] = addSaturated(puddleEnergy[center - 1], 24);
      if (center + 1 < VPANEL_W)
        puddleEnergy[center + 1] = addSaturated(puddleEnergy[center + 1], 24);
    }

    void drawPuddle() {
      for (int x = 0; x < VPANEL_W; ++x) {
        uint8_t energy = puddleEnergy[x];
        if (energy == 0) continue;

        uint8_t depth = min(MAX_PUDDLE_ROWS, static_cast<uint8_t>(energy / 48));
        depth = max(static_cast<uint8_t>(1), depth);
        for (uint8_t row = 0; row < depth && row < VPANEL_H; ++row) {
          const uint8_t brightness = (static_cast<uint16_t>(energy) * (depth - row)) / depth;
          const int y = VPANEL_H - 1 - row;
          effects.setPixel(x, y, CRGB(0, brightness, brightness / 5));
        }

        // A brighter, irregular waterline gives impacts a lingering splash.
        const int surfaceY = VPANEL_H - 1 - (depth - 1);
        effects.setPixel(x, surfaceY, CRGB(24, energy, energy / 3));
      }
    }

  public:
    PatternRain() {
      name = "Matrix Rain";
    }

    void start() override {
      frameNumber = 0;
      effects.ClearFrame();
      for (uint8_t i = 0; i < MAX_RAINDROPS; ++i) {
        rainDrops[i].active = false;
        rainDrops[i].x = 0;
        rainDrops[i].y = 0;
        rainDrops[i].trailLength = 0;
        rainDrops[i].speed = 1;
        rainDrops[i].colour = CRGB::Green;
      }
      for (int x = 0; x < VPANEL_W; ++x)
        puddleEnergy[x] = 0;
    }

    unsigned int drawFrame() override {
      if (!effects.isReady() || VPANEL_W <= 0 || VPANEL_H <= 0) return 100;

      ++frameNumber;
      effects.DimAll(218); // trails and the bottom pool slowly fade

      CRGBPalette16 rainPalette(CRGB::Black, CRGB(0, 40, 0), CRGB::Green, CRGB(160, 255, 180));
      if (random(256) < 180) spawnDrop(rainPalette);
      if (random(256) < 45) spawnDrop(rainPalette);

      for (uint8_t i = 0; i < MAX_RAINDROPS; ++i) {
        RainDrop& drop = rainDrops[i];
        if (!drop.active) continue;

        // Bright glyph/head, followed by a tapered green trail.
        effects.setPixel(drop.x, drop.y, CRGB(190, 255, 205));
        for (uint8_t tail = 1; tail <= drop.trailLength; ++tail) {
          if (drop.y < tail) break;
          CRGB trail = drop.colour;
          const uint8_t scale = 255 - (static_cast<uint16_t>(tail) * 190 / (drop.trailLength + 1));
          trail.nscale8(scale);
          effects.setPixel(drop.x, drop.y - tail, trail);
        }

        if (frameNumber % drop.speed == 0) {
          if (drop.y + 1 >= VPANEL_H) {
            addImpact(drop.x);
            drop.active = false;
          } else {
            ++drop.y;
          }
        }
      }

      // Per-column surface energy decays after impacts, but overlapping drops
      // keep the shallow, rippling pool alive.
      if (frameNumber % 3 == 0) {
        for (int x = 0; x < VPANEL_W; ++x) {
          if (puddleEnergy[x] > 0) --puddleEnergy[x];
        }
      }
      drawPuddle();
      effects.ShowFrame();
      return 40;
    }
};

#endif
