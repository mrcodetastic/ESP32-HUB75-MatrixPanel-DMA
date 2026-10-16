#ifndef PatternGameOfLife_H
#define PatternGameOfLife_H

#include <string.h>

class PatternGameOfLife : public Drawable {
  private:
    static const size_t CELL_COUNT =
        (VPANEL_W > 0 && VPANEL_H > 0) ? static_cast<size_t>(VPANEL_W) * VPANEL_H : 1;

    uint8_t cellsA[CELL_COUNT];
    uint8_t cellsB[CELL_COUNT];
    uint8_t *current = cellsA;
    uint8_t *next = cellsB;
    uint32_t generation = 0;

    size_t indexOf(int x, int y) const {
      return static_cast<size_t>(y) * VPANEL_W + x;
    }

    void seedCells() {
      memset(cellsA, 0, sizeof(cellsA));
      memset(cellsB, 0, sizeof(cellsB));

      // A sparse random seed creates several independent colonies.
      for (int y = 0; y < VPANEL_H; ++y) {
        for (int x = 0; x < VPANEL_W; ++x) {
          if (random(100) < 20)
            cellsA[indexOf(x, y)] = 1;
        }
      }

      current = cellsA;
      next = cellsB;
      generation = 0;
    }

    uint8_t countNeighbors(int x, int y) const {
      uint8_t count = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          const int nx = x + dx;
          const int ny = y + dy;
          if (nx >= 0 && nx < VPANEL_W && ny >= 0 && ny < VPANEL_H)
            count += current[indexOf(nx, ny)];
        }
      }
      return count;
    }

  public:
    PatternGameOfLife() {
      name = "Game of Life";
      memset(cellsA, 0, sizeof(cellsA));
      memset(cellsB, 0, sizeof(cellsB));
    }

    void start() override {
      seedCells();
      effects.ClearFrame();
    }

    unsigned int drawFrame() override {
      if (!effects.isReady() || VPANEL_W <= 0 || VPANEL_H <= 0) return 100;

      uint16_t population = 0;
      for (int y = 0; y < VPANEL_H; ++y) {
        for (int x = 0; x < VPANEL_W; ++x) {
          const size_t i = indexOf(x, y);
          const uint8_t neighbors = countNeighbors(x, y);
          next[i] = current[i]
              ? ((neighbors == 2 || neighbors == 3) ? 1 : 0)
              : (neighbors == 3 ? 1 : 0);
          population += next[i];
        }
      }

      uint8_t *oldCurrent = current;
      current = next;
      next = oldCurrent;
      ++generation;

      // Re-seed if the board has reached an uninteresting near-empty state.
      if (population < 6) {
        seedCells();
      }

      for (int y = 0; y < VPANEL_H; ++y) {
        for (int x = 0; x < VPANEL_W; ++x) {
          if (current[indexOf(x, y)]) {
            const uint8_t hue = static_cast<uint8_t>(generation * 3 + x * 5 + y * 7);
            effects.setPixel(x, y, effects.ColorFromCurrentPalette(hue));
          } else {
            effects.setPixel(x, y, CRGB::Black);
          }
        }
      }

      effects.ShowFrame();
      return 120;
    }
};

#endif
