#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

/**
 * Verification sketch for the FOUR_SCAN_20PX_HIGH mapping in
 * ESP32-HUB75-VirtualMatrixPanel_T.hpp, on 40x20 P8 1/5-scan modules.
 *
 * The mapping in the library is a closed-form expression; this sketch checks it
 * reproduces the behaviour that was measured on real hardware.
 *
 * Cycles through five tests, 6 seconds each. What each one is for:
 *
 *   1 SQUARES    Two 16x16 squares with both diagonals, one centred per module.
 *                Squares make the diagonals exactly 45 degrees, so each must
 *                hold ONE pixel per row - no rasteriser artefacts to misread.
 *                Corners must land at (12,2)-(27,17) and (52,2)-(67,17).
 *
 *   2 GRID       5x5 checkerboard. The scan segments are 4 and 8 columns wide,
 *                so 5px blocks straddle segment boundaries; a misplaced pixel
 *                shows as a bite out of a corner instead of being swallowed by
 *                a same-coloured neighbour (which is what 10px blocks did).
 *
 *   3 LINES      Vertical lines at columns 0/13/26/39 and horizontal lines at
 *                rows 3/4/5/6. The verticals check column mapping and the A/B
 *                alignment; the horizontals check row ORDER across the scan
 *                boundary, which the verticals are blind to.
 *
 *   4 TEXT       Letters at two heights in different colours. Tall glyphs cross
 *                three scan-band boundaries, short ones straddle the middle;
 *                distinct colours make a stray pixel traceable to its letter.
 *
 *   5 BORDER     1px border plus centre cross - checks the extreme rows and
 *                columns, which the other tests leave untouched.
 */

#define PANEL_RES_X     40
#define PANEL_RES_Y     20

#define VDISP_NUM_ROWS  1
#define VDISP_NUM_COLS  2

#define PANEL_CHAIN_LEN  (VDISP_NUM_ROWS * VDISP_NUM_COLS)
#define PANEL_CHAIN_TYPE CHAIN_NONE
#define PANEL_SCAN_TYPE  FOUR_SCAN_20PX_HIGH

// HUB75 pins
#define R1   2
#define G1  15
#define BL1  4
#define R2  16
#define G2  27
#define BL2 17
#define CH_A  5
#define CH_B 18
#define CH_C 19
#define CH_D 21
#define CH_E 12
#define CLK 22
#define LAT 26
#define OE  25

MatrixPanel_I2S_DMA *dma_display = nullptr;

using MyScanTypeMapping = ScanTypeMapping<PANEL_SCAN_TYPE>;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, MyScanTypeMapping> *virtualDisp = nullptr;

HUB75_I2S_CFG::i2s_pins _pins = {R1, G1, BL1, R2, G2, BL2,
                                 CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};

uint16_t RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, ORANGE;

// --------------------------------------------------------------------------
// Test 1: squares with diagonals

#define SQUARE_SIDE 16

void drawSquareWithDiagonals(int x0, int y0, uint16_t outline) {
  const int last = SQUARE_SIDE - 1;

  virtualDisp->drawRect(x0, y0, SQUARE_SIDE, SQUARE_SIDE, outline);

  for (int i = 0; i < SQUARE_SIDE; i++) {
    virtualDisp->drawPixel(x0 + i,        y0 + i, RED);
    virtualDisp->drawPixel(x0 + last - i, y0 + i, GREEN);
  }
}

void testSquares() {
  virtualDisp->clearScreen();

  const int x0 = (PANEL_RES_X - SQUARE_SIDE) / 2;   // 12
  const int y0 = (PANEL_RES_Y - SQUARE_SIDE) / 2;   // 2

  drawSquareWithDiagonals(x0,               y0, CYAN);
  drawSquareWithDiagonals(x0 + PANEL_RES_X, y0, YELLOW);
}

// --------------------------------------------------------------------------
// Test 2: 5x5 checkerboard

void testCheckerboard5() {
  virtualDisp->clearScreen();

  const uint16_t palette[4] = { RED, GREEN, BLUE, YELLOW };

  for (int by = 0; by < virtualDisp->height() / 5; by++) {
    for (int bx = 0; bx < virtualDisp->width() / 5; bx++) {
      uint16_t color = palette[(bx + by) % 4];
      for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
          virtualDisp->drawPixel(bx * 5 + x, by * 5 + y, color);
        }
      }
    }
  }
}

// --------------------------------------------------------------------------
// Test 3: alignment lines

void testLines() {
  virtualDisp->clearScreen();

  // Verticals: column mapping and A/B alignment. Must be dead straight.
  const int vcols[4]        = { 0, 13, 26, 39 };
  const uint16_t vcolors[4] = { RED, GREEN, BLUE, YELLOW };

  for (int i = 0; i < 4; i++) {
    for (int y = 0; y < virtualDisp->height(); y++) {
      virtualDisp->drawPixel(vcols[i], y, vcolors[i]);
    }
  }

  // Horizontals across the A/B scan boundary, on the second module only so
  // they don't collide with the verticals. Must appear adjacent and in order.
  const int hrows[4]        = { 3, 4, 5, 6 };
  const uint16_t hcolors[4] = { RED, GREEN, BLUE, YELLOW };

  for (int i = 0; i < 4; i++) {
    for (int x = 44; x < 76; x++) {
      virtualDisp->drawPixel(x, hrows[i], hcolors[i]);
    }
  }
}

// --------------------------------------------------------------------------
// Test 4: mixed-height, multi-colour text

void testMixedText() {
  virtualDisp->clearScreen();
  virtualDisp->setTextWrap(false);

  struct Glyph { char c; uint8_t size; int16_t x; uint16_t color; };

  const Glyph glyphs[] = {
    { 'A', 2,  1, RED     },
    { 'b', 1, 14, GREEN   },
    { 'C', 2, 21, BLUE    },
    { 'd', 1, 34, YELLOW  },
    { 'E', 2, 41, CYAN    },
    { 'f', 1, 54, MAGENTA },
    { 'G', 2, 61, WHITE   },
    { 'h', 1, 74, ORANGE  },
  };

  for (unsigned i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
    virtualDisp->setTextSize(glyphs[i].size);
    virtualDisp->setTextColor(glyphs[i].color);
    virtualDisp->setCursor(glyphs[i].x, glyphs[i].size == 2 ? 2 : 6);
    virtualDisp->print(glyphs[i].c);
  }
}

// --------------------------------------------------------------------------
// Test 5: border and centre cross

void testBorder() {
  virtualDisp->clearScreen();

  virtualDisp->drawRect(0, 0, virtualDisp->width(), virtualDisp->height(), GREEN);

  const int cx = virtualDisp->width() / 2;
  const int cy = virtualDisp->height() / 2;

  for (int x = 0; x < virtualDisp->width(); x++)  virtualDisp->drawPixel(x, cy, RED);
  for (int y = 0; y < virtualDisp->height(); y++) virtualDisp->drawPixel(cx, y, BLUE);
}

// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(2000);

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X * 2,          // DO NOT CHANGE - electrical width (four-scan)
    PANEL_RES_Y / 2,          // DO NOT CHANGE - electrical height (four-scan)
    PANEL_CHAIN_LEN,
    _pins
  );
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  mxconfig.clkphase = false;
  mxconfig.driver   = HUB75_I2S_CFG::ICN2038S;
  mxconfig.setPixelColorDepthBits(8);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(150);
  dma_display->clearScreen();

  virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE, MyScanTypeMapping>(
                      VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
  virtualDisp->setDisplay(*dma_display);

  RED     = virtualDisp->color565(255,   0,   0);
  GREEN   = virtualDisp->color565(  0, 255,   0);
  BLUE    = virtualDisp->color565( 80,  80, 255);
  YELLOW  = virtualDisp->color565(255, 255,   0);
  CYAN    = virtualDisp->color565(  0, 255, 255);
  MAGENTA = virtualDisp->color565(255,   0, 255);
  WHITE   = virtualDisp->color565(255, 255, 255);
  ORANGE  = virtualDisp->color565(255, 140,   0);

  Serial.println("FOUR_SCAN_20PX_HIGH verification - 5 tests, 6s each");
}

void loop() {
  Serial.println("1 SQUARES  - corners must be (12,2)-(27,17) and (52,2)-(67,17)");
  testSquares();
  delay(6000);

  Serial.println("2 GRID     - 5x5 blocks, square corners, no bites");
  testCheckerboard5();
  delay(6000);

  Serial.println("3 LINES    - verticals straight, rows 3/4/5/6 adjacent and in order");
  testLines();
  delay(6000);

  Serial.println("4 TEXT     - A b C d E f G h, all cleanly formed");
  testMixedText();
  delay(6000);

  Serial.println("5 BORDER   - unbroken rectangle plus centre cross");
  testBorder();
  delay(6000);
}
