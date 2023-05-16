#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

MatrixPanel_I2S_DMA *dma_display = nullptr;

extern "C" void app_main() {
  HUB75_I2S_CFG mxconfig(/* width = */ 64, /* height = */ 64, /* chain = */ 1);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(80);
  dma_display->clearScreen();
  // `println` is only available when the Adafruit GFX library is used.
  dma_display->println("Test message");
}
