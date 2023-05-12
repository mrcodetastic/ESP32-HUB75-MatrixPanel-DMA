#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#define WIDTH 64
#define HEIGHT 64
#define CHAIN 1

MatrixPanel_I2S_DMA *dma_display = nullptr;

extern "C" void app_main() {
  HUB75_I2S_CFG mxconfig(WIDTH, HEIGHT, CHAIN);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(80);
  dma_display->clearScreen();
}
