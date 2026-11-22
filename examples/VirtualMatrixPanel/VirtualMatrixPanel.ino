/**
 * @file VirtualMatrixPanel.ino
 * @brief Example of using the VirtualMatrixPanel_T template class.
 *
 * The VirtualMatrixPanel_T class can be used for two purposes:
 *
 *   1) Create a much larger display out of a number of physical LED panels
 *      chained in a Serpentine or Zig-Zag manner;
 *
 *   2) Provide a way to deal with non-standard physical panels that do not have
 *      a simple linear X, Y pixel mapping (e.g. 1/4, 1/5, 1/8, 1/10, 16S 4-scan
 *      or 8-scan panels).
 *
 *   1) and 2) can be combined and utilized together.
 *
 *   Select which example gets built via "#define EXAMPLE_NUMBER X":
 *
 *   Example 1:  STANDARD Two-Scan (1/16 on 32px, 1/32 on 64px) LED matrix panels,
 *               64x32 pixels each, in a 2x2 Serpentine grid.
 *
 *   Example 2:  Non-standard Four-Scan (e.g. 64x32 1/8 outdoor) LED matrix panels
 *               with segment width (pxbase) = 16, in a 2x2 Serpentine grid.
 *
 *   Example 3:  Single non-standard Four-Scan 64x32 1/8 outdoor panel (pxbase = 16).
 *
 *   Example 4:  Custom sketch-local ScanPolicy mapping (horizontal mirror).
 */

#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

// Select example to compile:
#define EXAMPLE_NUMBER 1
//#define EXAMPLE_NUMBER 2
//#define EXAMPLE_NUMBER 3
//#define EXAMPLE_NUMBER 4 // Custom ScanPolicy example

/**
 * Physical dimensions of each individual LED panel module.
 */
#define PANEL_RES_X 64 // Width of one panel module in pixels
#define PANEL_RES_Y 32 // Height of one panel module in pixels

#define VDISP_NUM_ROWS 2 // Number of panel rows in the grid
#define VDISP_NUM_COLS 2 // Number of panel columns in the grid

/**
 * Physical chaining order of the panel grid.
 * See PANEL_CHAIN_TYPE in ESP32-HUB75-VirtualMatrixPanel_T.hpp and doc/VirtualMatrixPanel_T.md.
 */
#define VIRTUAL_CHAIN_TYPE CHAIN_TOP_RIGHT_DOWN

MatrixPanel_I2S_DMA *dma_display = nullptr;

// ------------------------------------------------------------------------------------------------------------
// Template instantiations per EXAMPLE_NUMBER
// ------------------------------------------------------------------------------------------------------------
#if EXAMPLE_NUMBER == 1

  // --- Example 1: STANDARD Two-Scan 2x2 grid ---
  using VirtualDisplay = VirtualMatrixPanel_T<VIRTUAL_CHAIN_TYPE>;
  VirtualDisplay *virtualDisp = nullptr;

#elif EXAMPLE_NUMBER == 2

  // --- Example 2: Four-Scan 2x2 grid (64x32 1/8 outdoor, pxbase = 16) ---
  // For other variants, combine FOUR_SCAN_FLAGS:
  //   FourScanMapping<8, FOUR_SCAN_SWAP_BLOCKS>
  //   FourScanMapping<4, FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_XOR_SEGMENTS>
  //   FourScanMapping<8, FOUR_SCAN_ZIGZAG_SM16208>
  using MyScanMapping = FourScanMapping<16>;
  using VirtualDisplay = VirtualMatrixPanel_T<VIRTUAL_CHAIN_TYPE, MyScanMapping>;
  VirtualDisplay *virtualDisp = nullptr;

#elif EXAMPLE_NUMBER == 3

  // --- Example 3: Single Four-Scan panel (64x32 1/8 outdoor, pxbase = 16) ---
  using MyScanMapping = FourScanMapping<16>;
  using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, MyScanMapping>;
  VirtualDisplay *virtualDisp = nullptr;

#elif EXAMPLE_NUMBER == 4

  // --- Example 4: Custom ScanPolicy mapping (horizontal mirror per panel) ---
  // Custom policies inherit ScanPolicyTraits<DmaWidthMult, DmaHeightDiv> and
  // implement map(VirtualCoords, const PanelGeometry&) const.
  struct CustomMirrorScanTypeMapping : ScanPolicyTraits<1, 1> {
    VirtualCoords map(VirtualCoords coords, const PanelGeometry &geom) const {
      const int panel_idx = coords.x / geom.panel_res_x;
      const int local_x   = coords.x % geom.panel_res_x;
      coords.x = static_cast<int16_t>(
          panel_idx * geom.panel_res_x + (geom.panel_res_x - 1 - local_x));
      return coords;
    }
  };

  using VirtualDisplay = VirtualMatrixPanel_T<CHAIN_NONE, CustomMirrorScanTypeMapping>;
  VirtualDisplay *virtualDisp = nullptr;

#endif

// ------------------------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(2000);

  /*
  // Optional custom pin map:
  HUB75_I2S_CFG::i2s_pins _pins = {
    18, 17, 16, 15, 7, 6, 4, 10, 14, 21, 5, 48, 38, 47
  };
  */

#if EXAMPLE_NUMBER == 1 || EXAMPLE_NUMBER == 2
  // Automatically sizes the DMA framebuffer (1x for 2-scan, 2*W x H/2 for 4-scan).
  HUB75_I2S_CFG mxconfig = VirtualDisplay::makeDMAConfig(
      PANEL_RES_X, PANEL_RES_Y, VDISP_NUM_ROWS, VDISP_NUM_COLS);
#else
  HUB75_I2S_CFG mxconfig = VirtualDisplay::makeDMAConfig(PANEL_RES_X, PANEL_RES_Y);
#endif

  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase = false;
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  // mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595; // e.g., SM5368 / RUL5158 / TC7558

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128);
  dma_display->clearScreen();

#if EXAMPLE_NUMBER == 1 || EXAMPLE_NUMBER == 2
  virtualDisp = new VirtualDisplay(
      *dma_display, VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
#else
  virtualDisp = new VirtualDisplay(*dma_display, 1, 1, PANEL_RES_X, PANEL_RES_Y);
#endif

  for (int y = 0; y < virtualDisp->height(); y++) {
    for (int x = 0; x < virtualDisp->width(); x++) {
      uint16_t color = virtualDisp->color565(96, 0, 0);
      if (x == 0) color = virtualDisp->color565(0, 255, 0);
      if (x == (virtualDisp->width() - 1)) color = virtualDisp->color565(0, 0, 255);

      virtualDisp->drawPixel(x, y, color);
      delay(1);
    }
  }

  virtualDisp->drawLine(virtualDisp->width() - 1, virtualDisp->height() - 1,
                        0, 0, virtualDisp->color565(255, 255, 255));

  virtualDisp->print("Virtual Matrix Panel");

  delay(3000);
  virtualDisp->clearScreen();
  virtualDisp->drawDisplayTest();
}

void loop() {
}