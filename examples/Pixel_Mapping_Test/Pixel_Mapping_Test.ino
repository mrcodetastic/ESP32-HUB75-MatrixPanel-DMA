/*************************************************************************
 * Pixel_Mapping_Test.ino
 *
 * Interactive & configurable diagnostic sketch for discovering and verifying
 * non-standard (4-scan 2*W x H/2 and 8-scan 4*W x H/4) HUB75 panels using
 * VirtualMatrixPanel_T.
 *
 * Works two ways:
 *   1) Edit the #defines below and upload (traditional Discussion #622 workflow).
 *   2) Open the Serial Monitor @ 115200 baud while the test runs to change
 *      pxbase / flags live without reflashing, then press 'p' to print the
 *      exact C++ FourScanMapping<...> / EightScanMapping<...> line to copy!
 *
 * See doc/VirtualMatrixPanel_T.md for the step-by-step mapping & IC guide.
 *************************************************************************/

#include <Arduino.h>
#include <cstdlib>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

// -----------------------------------------------------------------------
// 1. Panel dimensions (physical pixels of a single module)
// -----------------------------------------------------------------------
#define PANEL_RES_X 32
#define PANEL_RES_Y 16

// Chain grid size (start with 1x1 while discovering a panel's mapping!)
#define NUM_ROWS 1
#define NUM_COLS 1
#define VIRTUAL_CHAIN_TYPE CHAIN_NONE

// -----------------------------------------------------------------------
// 2. Scan parallel factor:
//    4 = Four-Scan (2*W x H/2 DMA): covers 1/4 (16px), 1/5 (20px),
//        1/6 (24px), 1/8 (32px), 1/10 (40px), 1/16 16S (64px)
//    8 = Eight-Scan (4*W x H/4 DMA): covers 1/2 (16px) or 1/4 (32px)
// -----------------------------------------------------------------------
#define PANEL_PARALLEL_ROWS 4

// -----------------------------------------------------------------------
// 3. Initial mapping parameters (Step 2-4 of the mapping guide)
// -----------------------------------------------------------------------
// Segment width in pixels (try PANEL_RES_X first, then 32, 16, 8, 4, or 1).
#define PANEL_PX_BASE PANEL_RES_X

#if PANEL_PARALLEL_ROWS == 8
  // Bitflags from EIGHT_SCAN_FLAGS:
  //   EIGHT_SCAN_NONE | EIGHT_SCAN_REVERSE_BANDS |
  //   EIGHT_SCAN_REVERSE_EVEN_SLOTS | EIGHT_SCAN_REVERSE_ODD_SLOTS |
  //   EIGHT_SCAN_INVERT_ROW_IN_BAND
  #define PANEL_SCAN_FLAGS EIGHT_SCAN_NONE
#else
  // Bitflags from FOUR_SCAN_FLAGS:
  //   FOUR_SCAN_NONE | FOUR_SCAN_SWAP_BLOCKS | FOUR_SCAN_XOR_SEGMENTS |
  //   FOUR_SCAN_REVERSE_LEFT | FOUR_SCAN_REVERSE_RIGHT
  #define PANEL_SCAN_FLAGS FOUR_SCAN_NONE
#endif

// Only set true for 64x64 4-scan panels using 8-row bit-swapped blocks
// (e.g. ICN2037BP outdoor modules, issue #345). Standard 64x64 16S stays false.
#define PANEL_BITSWAP_64PX false

// Pixel fill delay (ms per pixel; adjust slower for recording videos)
#define PIXEL_STEP_DELAY_MS 20

// -----------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------
MatrixPanel_I2S_DMA *dma_display = nullptr;

#if PANEL_PARALLEL_ROWS == 8
  using TestDisplay = VirtualMatrixPanel_T<CHAIN_RUNTIME, RuntimeEightScanMapping>;
#else
  using TestDisplay = VirtualMatrixPanel_T<CHAIN_RUNTIME, RuntimeFourScanMapping>;
#endif

TestDisplay *testPanel = nullptr;

static char serialBuf[32];
static size_t serialPos = 0;

void printActiveConfig() {
#if PANEL_PARALLEL_ROWS == 8
  const EightScanConfig &cfg = testPanel->scanPolicy().config;
  Serial.printf("\n[8-Scan] panel=%ux%u pxbase=%u flags=0x%02X -> ",
                PANEL_RES_X, PANEL_RES_Y,
                cfg.pxbase ? cfg.pxbase : PANEL_RES_X, cfg.flags);
  Serial.print("EightScanMapping<");
  Serial.print(cfg.pxbase);
  if (cfg.flags != EIGHT_SCAN_NONE) {
    Serial.print(", ");
    bool first = true;
    auto emitFlag = [&](uint8_t bit, const char *name) {
      if (cfg.flags & bit) {
        if (!first) Serial.print(" | ");
        Serial.print(name);
        first = false;
      }
    };
    emitFlag(EIGHT_SCAN_REVERSE_BANDS,      "EIGHT_SCAN_REVERSE_BANDS");
    emitFlag(EIGHT_SCAN_REVERSE_EVEN_SLOTS, "EIGHT_SCAN_REVERSE_EVEN_SLOTS");
    emitFlag(EIGHT_SCAN_REVERSE_ODD_SLOTS,  "EIGHT_SCAN_REVERSE_ODD_SLOTS");
    emitFlag(EIGHT_SCAN_INVERT_ROW_IN_BAND, "EIGHT_SCAN_INVERT_ROW_IN_BAND");
  }
  Serial.println(">");
#else
  const FourScanConfig &cfg = testPanel->scanPolicy().config;
  Serial.printf("\n[4-Scan] panel=%ux%u pxbase=%u flags=0x%02X bitswap64=%s -> ",
                PANEL_RES_X, PANEL_RES_Y,
                cfg.pxbase ? cfg.pxbase : PANEL_RES_X,
                cfg.flags, cfg.bitswap_64px ? "true" : "false");
  if (cfg.bitswap_64px) {
    Serial.print("FourScan64pxBitSwapMapping<");
  } else {
    Serial.print("FourScanMapping<");
  }
  Serial.print(cfg.pxbase);
  if (cfg.flags != FOUR_SCAN_NONE) {
    Serial.print(", ");
    bool first = true;
    auto emitFlag = [&](uint8_t bit, const char *name) {
      if (cfg.flags & bit) {
        if (!first) Serial.print(" | ");
        Serial.print(name);
        first = false;
      }
    };
    emitFlag(FOUR_SCAN_SWAP_BLOCKS,   "FOUR_SCAN_SWAP_BLOCKS");
    emitFlag(FOUR_SCAN_XOR_SEGMENTS,  "FOUR_SCAN_XOR_SEGMENTS");
    emitFlag(FOUR_SCAN_REVERSE_LEFT,  "FOUR_SCAN_REVERSE_LEFT");
    emitFlag(FOUR_SCAN_REVERSE_RIGHT, "FOUR_SCAN_REVERSE_RIGHT");
  }
  Serial.println(">");
#endif
}

void printHelp() {
  Serial.println("\nSerial tuning commands (@ 115200):");
  Serial.println("  b <N> : set pxbase (e.g. b 64, b 16, b 8, b 4, b 1, or b 0=auto)");
#if PANEL_PARALLEL_ROWS == 8
  Serial.println("  r     : toggle EIGHT_SCAN_REVERSE_BANDS");
  Serial.println("  e     : toggle EIGHT_SCAN_REVERSE_EVEN_SLOTS");
  Serial.println("  o     : toggle EIGHT_SCAN_REVERSE_ODD_SLOTS");
  Serial.println("  i     : toggle EIGHT_SCAN_INVERT_ROW_IN_BAND");
#else
  Serial.println("  s     : toggle FOUR_SCAN_SWAP_BLOCKS");
  Serial.println("  x     : toggle FOUR_SCAN_XOR_SEGMENTS");
  Serial.println("  l     : toggle FOUR_SCAN_REVERSE_LEFT");
  Serial.println("  r     : toggle FOUR_SCAN_REVERSE_RIGHT");
  Serial.println("  6     : toggle 64x64 ICN2037BP 8-row bitswap (#345)");
#endif
  Serial.println("  p     : print copy-paste C++ policy declaration");
  Serial.println("  c     : clear screen");
  Serial.println("  h / ? : show this help");
}

void handleCommand(const char *cmd) {
  if (!cmd[0]) return;
  if (cmd[0] == 'b') {
    int val = atoi(cmd + 1);
    if (val >= 0 && val <= PANEL_RES_X) {
      testPanel->scanPolicy().config.pxbase = static_cast<uint16_t>(val);
      dma_display->clearScreen();
      printActiveConfig();
    } else {
      Serial.println("Invalid pxbase (must be 0..PANEL_RES_X)");
    }
    return;
  }
  switch (cmd[0]) {
#if PANEL_PARALLEL_ROWS == 8
    case 'r': testPanel->scanPolicy().config.flags ^= EIGHT_SCAN_REVERSE_BANDS;      dma_display->clearScreen(); printActiveConfig(); break;
    case 'e': testPanel->scanPolicy().config.flags ^= EIGHT_SCAN_REVERSE_EVEN_SLOTS; dma_display->clearScreen(); printActiveConfig(); break;
    case 'o': testPanel->scanPolicy().config.flags ^= EIGHT_SCAN_REVERSE_ODD_SLOTS;  dma_display->clearScreen(); printActiveConfig(); break;
    case 'i': testPanel->scanPolicy().config.flags ^= EIGHT_SCAN_INVERT_ROW_IN_BAND; dma_display->clearScreen(); printActiveConfig(); break;
#else
    case 's': testPanel->scanPolicy().config.flags ^= FOUR_SCAN_SWAP_BLOCKS;         dma_display->clearScreen(); printActiveConfig(); break;
    case 'x': testPanel->scanPolicy().config.flags ^= FOUR_SCAN_XOR_SEGMENTS;        dma_display->clearScreen(); printActiveConfig(); break;
    case 'l': testPanel->scanPolicy().config.flags ^= FOUR_SCAN_REVERSE_LEFT;        dma_display->clearScreen(); printActiveConfig(); break;
    case 'r': testPanel->scanPolicy().config.flags ^= FOUR_SCAN_REVERSE_RIGHT;       dma_display->clearScreen(); printActiveConfig(); break;
    case '6': testPanel->scanPolicy().config.bitswap_64px = !testPanel->scanPolicy().config.bitswap_64px; dma_display->clearScreen(); printActiveConfig(); break;
#endif
    case 'c': dma_display->clearScreen(); break;
    case 'p': printActiveConfig(); break;
    case 'h':
    case '?': printHelp(); break;
    default: break;
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r' || ch == '\n') {
      serialBuf[serialPos] = '\0';
      handleCommand(serialBuf);
      serialPos = 0;
    } else if (serialPos + 1 < sizeof(serialBuf)) {
      serialBuf[serialPos++] = ch;
    }
  }
}

void delayAndPoll(uint32_t ms) {
  uint32_t start = millis();
  do {
    pollSerial();
    delay(1);
  } while ((millis() - start) < ms);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Automatically configures DMA as 2*W x H/2 (4-scan) or 4*W x H/4 (8-scan).
  auto mxconfig = TestDisplay::makeDMAConfig(
      PANEL_RES_X, PANEL_RES_Y, NUM_ROWS, NUM_COLS);

  // Common hardware options before mapping tuning (see doc/VirtualMatrixPanel_T.md):
  // Invert shift clock phase if pixels shift horizontally or ghost on SM16208 / MBI5124 panels:
  mxconfig.clkphase = false;

  // Uncomment for shift-register line decoders (SM5368 / RUL5158 / TC7558 / DP32020A):
  // mxconfig.line_decoder = HUB75_I2S_CFG::TYPE595;

  // Uncomment for direct MOSFET row multiplexing (no 74HC138/595 decoder chip, e.g. 4953):
  // mxconfig.line_decoder = HUB75_I2S_CFG::TYPE_DIRECT;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->setBrightness8(40);

  if (!dma_display->begin()) {
    Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
    while (true) { delay(1000); }
  }

  dma_display->clearScreen();
  delay(500);

#if PANEL_PARALLEL_ROWS == 8
  EightScanConfig scanCfg{PANEL_PX_BASE, PANEL_SCAN_FLAGS, 0};
  testPanel = new TestDisplay(*dma_display, mxconfig, scanCfg);
#else
  FourScanConfig scanCfg{PANEL_PX_BASE, PANEL_SCAN_FLAGS, 0, PANEL_BITSWAP_64PX};
  testPanel = new TestDisplay(*dma_display, mxconfig, scanCfg);
#endif
  testPanel->setChainType(VIRTUAL_CHAIN_TYPE);

  printActiveConfig();
  printHelp();
}

void loop() {
  for (int y = 0; y < testPanel->height(); y++) {
    for (int x = 0; x < testPanel->width(); x++) {
      testPanel->drawPixel(x, y, testPanel->color565(255, 0, 0));
      delayAndPoll(PIXEL_STEP_DELAY_MS);
    }
  }
  delayAndPoll(2000);
  dma_display->clearScreen();
}