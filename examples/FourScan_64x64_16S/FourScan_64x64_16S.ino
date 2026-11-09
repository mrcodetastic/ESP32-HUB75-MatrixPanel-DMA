/*************************************************************************
 * 64x64 1/16-scan ("16S") HUB75 panel
 *
 * These panels are wired as four 16-row blocks and drive two rows in
 * parallel per address step. The library's DMA engine only does two-row
 * output, so the panel is fed as if it were a 128x32 surface (2*W x H/2)
 * and the coordinates are folded by FOUR_SCAN_64PX_HIGH_16S.
 *
 * This is NOT the same as FOUR_SCAN_64PX_HIGH, which folds 64px-high panels
 * in 8-row blocks (ICN2037BP, issue #345). If your panel shows four
 * scrambled strips with that mode, try this one.
 *
 * See doc/FourScan_64x64_16S.md for the mapping table.
 *
 * Test pattern: green border, coloured corner blocks, 8px grid and both
 * diagonals. If the corners are in the right place and the diagonals are
 * straight, the mapping is correct.
 *************************************************************************/

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

// Logical (real-world) panel size.
#define PANEL_RES_X 64
#define PANEL_RES_Y 64

MatrixPanel_I2S_DMA *dma_display = nullptr;

using Panel16SCanTypeMapping = ScanTypeMapping<FOUR_SCAN_64PX_HIGH_16S>;
VirtualMatrixPanel_T<CHAIN_NONE, Panel16SCanTypeMapping> *panel = nullptr;

void drawTestPattern() {
  panel->fillScreen(0);

  const uint16_t green = panel->color565(0, 255, 0);
  const uint16_t red = panel->color565(255, 0, 0);
  const uint16_t blue = panel->color565(0, 0, 255);
  const uint16_t yellow = panel->color565(255, 255, 0);
  const uint16_t white = panel->color565(255, 255, 255);
  const uint16_t dim = panel->color565(40, 40, 40);

  // Border.
  panel->drawRect(0, 0, panel->width(), panel->height(), green);

  // Corner markers: TL red, TR green, BL blue, BR yellow.
  panel->fillRect(0, 0, 8, 8, red);
  panel->fillRect(panel->width() - 8, 0, 8, 8, green);
  panel->fillRect(0, panel->height() - 8, 8, 8, blue);
  panel->fillRect(panel->width() - 8, panel->height() - 8, 8, 8, yellow);

  // 8px grid.
  for (int x = 8; x < panel->width(); x += 8) {
    panel->drawFastVLine(x, 0, panel->height(), dim);
  }
  for (int y = 8; y < panel->height(); y += 8) {
    panel->drawFastHLine(0, y, panel->width(), dim);
  }

  // Diagonals.
  for (int i = 0; i < panel->width(); i++) {
    panel->drawPixel(i, i, white);
    panel->drawPixel(panel->width() - 1 - i, i, white);
  }

  // Label.
  panel->setTextColor(white);
  panel->setCursor(4, 27);
  panel->print("16S");
}

void setup() {
  Serial.begin(115200);

  // The DMA surface is 2*W x H/2 for this panel type. Do not use 64x64 here.
#if defined(ADAFRUIT_MATRIXPORTAL_ESP32S3)
  // HUB75 pins on the Adafruit MatrixPortal ESP32-S3.
  HUB75_I2S_CFG::i2s_pins pins = {
    42, 41, 40, 38, 39, 37, 45, 36, 48, 35, 21, 47, 14, 2
  };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2, PANEL_RES_Y / 2, 1, pins);
#else
  // Default library pins. Change for your board if needed.
  HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2, PANEL_RES_Y / 2, 1);
#endif

  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::MBI5124;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("DMA allocation failed");
    while (true) {
      delay(1000);
    }
  }

  // Must be after begin(); the library ignores brightness changes before init.
  dma_display->setBrightness8(64);
  dma_display->clearScreen();

  panel = new VirtualMatrixPanel_T<CHAIN_NONE, Panel16SCanTypeMapping>(
      1, 1, PANEL_RES_X, PANEL_RES_Y);
  panel->setDisplay(*dma_display);

  Serial.printf("Panel: %dx%d, scan type FOUR_SCAN_64PX_HIGH_16S\n",
                panel->width(), panel->height());

  drawTestPattern();
}

void loop() {
  // Static test pattern; nothing to do.
  delay(1000);
}
