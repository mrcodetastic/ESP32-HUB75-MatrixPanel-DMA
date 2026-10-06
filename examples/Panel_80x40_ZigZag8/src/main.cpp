/**
 * Panel_80x40_ZigZag8 (src/main.cpp)
 *
 * Drives an 80x40, 1/10-scan ("4-scan") outdoor HUB75 panel whose SM16208
 * driver ICs interleave two logical rows into one 160-clock shift register
 * per colour lane, with 8-pixel-segment column reversal (a "ZIGZAG8" panel).
 *
 * How it works:
 *   VirtualMatrixPanel_T takes its scan-type mapping as a template *type*,
 *   not a fixed enum. The class only ever calls ScanTypeMapping::apply(coords,
 *   panel_pixel_base), so any struct with a matching static apply() can be
 *   passed as that template argument directly from sketch code. This sketch
 *   defines the ZIGZAG8 remap locally (SM16208ZigZagMapping below) instead of
 *   selecting a built-in scan type from PANEL_SCAN_TYPE.
 *
 *   See doc/Panel_80x40_4Scan_ZigZag8.md for the full write-up.
 *
 * Panel wiring notes:
 *   - Standard HUB75 RGB interface, A/B/C/D address lines (no E).
 *   - The panel expects a FOLDED 160x20 DMA surface, not 80x40 - see the
 *     NOTE in setup().
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

// ---------------------------------------------------------------------
// Logical panel resolution (what your drawing code addresses).
// ---------------------------------------------------------------------
#define PANEL_RES_X 80  // logical panel width
#define PANEL_RES_Y 40  // logical panel height

// ---------------------------------------------------------------------
// DMA framebuffer geometry. The driver ICs fold two logical rows into
// one 160-long shift register per colour lane, so the physical DMA
// surface is 160x20. DO NOT change these to 80x40 - that silently drops
// every mapped x >= 80 and never lights the second RGB lane
// (rows 20-39 stay dark).
// ---------------------------------------------------------------------
#define DMA_RES_X (PANEL_RES_X * 2)  // 160
#define DMA_RES_Y (PANEL_RES_Y / 2)  // 20

static_assert(DMA_RES_X >= PANEL_RES_X * 2,
              "DMA width must be 2x logical width - ZIGZAG8 maps logical 80 "
              "cols onto a folded 160-wide shift chain");
static_assert(PANEL_RES_Y % 20 == 0,
              "This mapping folds rows in pairs of 20 - logical height must be "
              "a multiple of 20");

// ---------------------------------------------------------------------
// Custom scan-type policy for this panel's SM16208 driver ICs.
// Same interface the library's built-in ScanTypeMapping<T> uses, so it
// can be passed directly as VirtualMatrixPanel_T's ScanType template
// argument.
//
// For each 10-row band, every PANEL_PIXEL_BASE-wide column segment is
// clocked into the 160-long shift chain in two back-to-back phases: the
// sub-row at scan address k reversed (offsets 7->0), its partner k+10
// forward (offsets 0->7). Bands 0-1 ride the RGB1 lane, bands 2-3 the
// RGB2 lane, folded onto the 20 DMA rows by (y/20)*10 + (y%10).
// ---------------------------------------------------------------------
#define PANEL_PIXEL_BASE 8  // this panel's segment width (8-LED blocks)

struct SM16208ZigZagMapping {
  // Note: the library's built-in ScanTypeMapping<T> marks this "constexpr";
  // VirtualCoords' default constructor makes that a no-op (compiler warns
  // -Winvalid-constexpr), so it is omitted here.
  static VirtualCoords apply(VirtualCoords coords, int panel_pixel_base) {
    int row_group = coords.y / 10;                 // 0..3
    int col_group = coords.x / panel_pixel_base;   // segment index
    int col_within = coords.x % panel_pixel_base;  // offset within segment

    if (row_group % 2 == 0) {
      coords.x = col_group * (panel_pixel_base * 2) +
                 (panel_pixel_base - 1 - col_within);  // reversed phase
    } else {
      coords.x = col_group * (panel_pixel_base * 2) + panel_pixel_base +
                 col_within;  // forward phase
    }

    // Fold 40 logical rows into 20 DMA rows (RGB1: 0-9, RGB2: 10-19).
    coords.y = (coords.y / 20) * 10 + (coords.y % 10);
    return coords;
  }
};

// ---------------------------------------------------------------------
// Display declarations
// ---------------------------------------------------------------------
MatrixPanel_I2S_DMA* dma_display = nullptr;

// CHAIN_NONE (single panel), our sketch-local mapping as the ScanType
// template argument, ScaleFactor 1.
VirtualMatrixPanel_T<CHAIN_NONE, SM16208ZigZagMapping, 1>* panel = nullptr;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // NOTE: DMA_RES_X x DMA_RES_Y (160x20), not 80x40 - see note above.
  HUB75_I2S_CFG mxconfig(DMA_RES_X, DMA_RES_Y, 1);
  mxconfig.clkphase = false;  // SM16208 latches on the inverted shift clock

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);  // 0-255
  dma_display->clearScreen();

  // Virtual panel addressed in logical 80x40 coordinates.
  panel = new VirtualMatrixPanel_T<CHAIN_NONE, SM16208ZigZagMapping, 1>(
      1, 1, PANEL_RES_X, PANEL_RES_Y);
  panel->setDisplay(*dma_display);
  panel->setPixelBase(
      PANEL_PIXEL_BASE);  // REQUIRED - the default pixel base
                          // (panel width, 80) scrambles the image
}

// Draws a 1-pixel border around the whole logical panel.
void drawBorder(uint16_t color) {
  panel->drawLine(0, 0, panel->width() - 1, 0, color);
  panel->drawLine(0, panel->height() - 1, panel->width() - 1,
                  panel->height() - 1, color);
  panel->drawLine(0, 0, 0, panel->height() - 1, color);
  panel->drawLine(panel->width() - 1, 0, panel->width() - 1,
                  panel->height() - 1, color);
}

void loop() {
  // --- Test 1: sequential pixel fill (the mapping verification) ---------
  // Lights every logical pixel in address order, red. With a correct
  // mapping the fill advances plainly left-to-right, top-to-bottom: the
  // remap below absorbs the panel's 8-pixel-segment zigzag wiring. If
  // segments light out of order, mirror within 8-pixel blocks, or jump
  // columns, setPixelBase() is wrong (or missing) - see the docs.
  for (int y = 0; y < panel->height(); y++) {
    for (int x = 0; x < panel->width(); x++) {
      panel->drawPixel(x, y, panel->color565(255, 0, 0));
      delay(1);
    }
  }
  delay(1500);
  dma_display->clearScreen();

  // --- Test 2: colour bars ---------------------------------------------
  // Four horizontal bars, one per 10-row band. A dark band 2 or 3 proves
  // the RGB2 lane never lights (wrong DMA geometry); swapped or misplaced
  // bands prove the row fold is wrong.
  for (int y = 0; y < panel->height(); y++) {
    uint16_t c = (y < 10)   ? panel->color565(255, 0, 0)     // red
                 : (y < 20) ? panel->color565(0, 255, 0)     // green
                 : (y < 30) ? panel->color565(0, 0, 255)     // blue
                            : panel->color565(255, 255, 0);  // yellow
    for (int x = 0; x < panel->width(); x++) {
      panel->drawPixel(x, y, c);
    }
  }
  delay(1500);
  dma_display->clearScreen();

  // --- Test 3: border + corner markers + text ----------------------------
  // Corners and edges prove logical (0,0) and (79,39) land where expected.
  drawBorder(panel->color565(255, 255, 255));
  panel->fillRect(0, 0, 5, 5,
                  panel->color565(255, 0, 255));  // top-left  magenta
  panel->fillRect(panel->width() - 5, 0, 5, 5,
                  panel->color565(0, 255, 255));  // top-right cyan
  panel->fillRect(0, panel->height() - 5, 5, 5,
                  panel->color565(255, 128, 0));  // bot-left  orange
  panel->fillRect(panel->width() - 5, panel->height() - 5, 5, 5,
                  panel->color565(128, 128, 128));  // bot-right grey
  panel->setTextColor(panel->color565(255, 255, 255));
  panel->setCursor(10, 24);
  panel->print("ZIGZAG8");
  delay(4000);
  dma_display->clearScreen();
}
