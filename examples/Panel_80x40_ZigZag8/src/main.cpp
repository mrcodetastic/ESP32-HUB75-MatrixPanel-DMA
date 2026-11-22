/**
 * Panel_80x40_ZigZag8 (src/main.cpp)
 *
 * Drives an 80x40, 1/10-scan ("4-scan") outdoor HUB75 panel whose SM16208
 * driver ICs interleave two logical rows into one 160-clock shift register
 * per colour lane, with 8-pixel-segment column reversal (a "ZIGZAG8" panel).
 *
 * The refactored VirtualMatrixPanel_T supports this panel natively via:
 *   FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>   (or alias FourScan80x40_ZigZag8)
 * and sizes the folded 160x20 DMA surface automatically with makeDMAConfig().
 *
 * Set USE_CUSTOM_POLICY to 1 below to see the equivalent sketch-local custom
 * ScanPolicy implementation using ScanPolicyTraits<2, 2> and PanelGeometry.
 *
 * See doc/Panel_80x40_4Scan_ZigZag8.md for the full hardware write-up.
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

#define USE_CUSTOM_POLICY 0

// ---------------------------------------------------------------------
// Logical panel resolution (what your drawing code addresses).
// ---------------------------------------------------------------------
#define PANEL_RES_X 80 // logical panel width
#define PANEL_RES_Y 40 // logical panel height

static_assert(PANEL_RES_Y % 20 == 0,
              "This mapping folds rows in pairs of 20 - logical height must be a multiple of 20");

#if USE_CUSTOM_POLICY
// ---------------------------------------------------------------------
// Equivalent sketch-local ScanPolicy using the clean PanelGeometry API.
// ---------------------------------------------------------------------
struct SM16208ZigZagMapping : ScanPolicyTraits<2, 2> {
  static constexpr int kPixelBase = 8;

  constexpr VirtualCoords map(VirtualCoords coords, const PanelGeometry &geom) const noexcept {
    const int panel_idx  = coords.x / geom.panel_res_x;
    const int local_x    = coords.x % geom.panel_res_x;
    const int row_group  = coords.y / (geom.panel_res_y / 4); // 0..3 (bands of 10 on 40px)
    const int col_group  = local_x / kPixelBase;
    const int col_within = local_x % kPixelBase;

    int mapped_local_x = col_group * (kPixelBase * 2);
    if ((row_group % 2) == 0) {
      mapped_local_x += (kPixelBase - 1) - col_within; // reversed phase (left DMA slot)
    } else {
      mapped_local_x += kPixelBase + col_within;       // forward phase (right DMA slot)
    }

    const int half_h = geom.panel_res_y / 2;
    const int q_h    = half_h / 2;
    coords.x = static_cast<int16_t>(panel_idx * (geom.panel_res_x * 2) + mapped_local_x);
    coords.y = static_cast<int16_t>((coords.y / half_h) * q_h + (coords.y % q_h));
    return coords;
  }
};
using PanelScan = SM16208ZigZagMapping;
#else
// ---------------------------------------------------------------------
// Built-in parameterized policy (equivalent 1-liner).
// FOUR_SCAN_ZIGZAG_SM16208 = FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_REVERSE_LEFT
// ---------------------------------------------------------------------
using PanelScan = FourScan80x40_ZigZag8; // or FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>
#endif

using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, PanelScan>;

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualDisplay *panel = nullptr;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Derives the folded 160x20 DMA buffer automatically from 80x40 + PanelScan traits.
  auto mxconfig = VirtualDisplay::makeDMAConfig(PANEL_RES_X, PANEL_RES_Y);
  mxconfig.clkphase = false; // SM16208 latches on the inverted shift clock

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();

  // Virtual panel addressed in logical 80x40 coordinates without repeating dimensions.
  panel = new VirtualDisplay(*dma_display, mxconfig);
}

void drawBorder(uint16_t color) {
  panel->drawLine(0, 0, panel->width() - 1, 0, color);
  panel->drawLine(0, panel->height() - 1, panel->width() - 1, panel->height() - 1, color);
  panel->drawLine(0, 0, 0, panel->height() - 1, color);
  panel->drawLine(panel->width() - 1, 0, panel->width() - 1, panel->height() - 1, color);
}

void loop() {
  // --- Test 1: sequential pixel fill ---
  for (int y = 0; y < panel->height(); y++) {
    for (int x = 0; x < panel->width(); x++) {
      panel->drawPixel(x, y, panel->color565(255, 0, 0));
      delay(1);
    }
  }
  delay(1500);
  dma_display->clearScreen();

  // --- Test 2: colour bars (4 horizontal bands of 10 rows) ---
  for (int y = 0; y < panel->height(); y++) {
    uint16_t c = (y < 10)   ? panel->color565(255, 0, 0)
                 : (y < 20) ? panel->color565(0, 255, 0)
                 : (y < 30) ? panel->color565(0, 0, 255)
                            : panel->color565(255, 255, 0);
    for (int x = 0; x < panel->width(); x++) {
      panel->drawPixel(x, y, c);
    }
  }
  delay(1500);
  dma_display->clearScreen();

  // --- Test 3: border + corner markers + text ---
  drawBorder(panel->color565(255, 255, 255));
  panel->fillRect(0, 0, 5, 5, panel->color565(255, 0, 255));
  panel->fillRect(panel->width() - 5, 0, 5, 5, panel->color565(0, 255, 255));
  panel->fillRect(0, panel->height() - 5, 5, 5, panel->color565(255, 128, 0));
  panel->fillRect(panel->width() - 5, panel->height() - 5, 5, 5, panel->color565(128, 128, 128));
  panel->setTextColor(panel->color565(255, 255, 255));
  panel->setCursor(10, 24);
  panel->print("ZIGZAG8");
  delay(4000);
  dma_display->clearScreen();
}
