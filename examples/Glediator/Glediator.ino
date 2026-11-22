/**
 * Glediator Placeholder Example
 *
 * The full ESP32 HUB75 receiver firmware and desktop LED matrix controller
 * live in a dedicated repository so the firmware and protocol stay in sync:
 *
 *   Repository:    https://github.com/mrcodetastic/Glediator
 *   ESP32 sketch:  https://github.com/mrcodetastic/Glediator/tree/master/esp32-firmware
 *   Protocol docs: https://github.com/mrcodetastic/Glediator/blob/master/docs/protocol.md
 *
 * That ESP32 use-case sketch combines ESP32-HUB75-MatrixPanel-I2S-DMA
 * (double-buffered) with UDP LAN discovery and WebSocket RGB565 frame
 * streaming (full, delta rectangle, and RLE frames).
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

MatrixPanel_I2S_DMA *dma_display = nullptr;

void setup() {
  Serial.begin(115200);
  Serial.println("Glediator ESP32 HUB75 receiver example has moved to:");
  Serial.println("  https://github.com/mrcodetastic/Glediator");
  Serial.println("  https://github.com/mrcodetastic/Glediator/tree/master/esp32-firmware");

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("DMA allocation failed");
    while (true) {
      delay(1000);
    }
  }

  dma_display->setBrightness8(96);
  dma_display->clearScreen();
  dma_display->setTextWrap(false);
  dma_display->setTextColor(dma_display->color565(0, 220, 255));
  dma_display->setCursor(5, 3);
  dma_display->print("Glediator");
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setCursor(2, 13);
  dma_display->print("See GitHub");
  dma_display->setCursor(2, 22);
  dma_display->print("/Glediator");
}

void loop() {
  delay(1000);
}
