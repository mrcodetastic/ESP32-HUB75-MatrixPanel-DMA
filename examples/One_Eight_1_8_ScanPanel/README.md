# Using this library with 32x16 1/8 Scan Panels

## Problem
ESP32-HUB75-MatrixPanel-I2S-DMA library will not display output correctly with 1/8 scan panels such [as this](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/154) by default.

## Solution
It is possible to connect 1/8 scan panels to this library and 'trick' the output to work correctly on these panels by way of adjusting the pixel co-ordinates that are 'sent' to the ESP32-HUB75-MatrixPanel-I2S-DMA library (in this example, it is the 'dmaOutput' class).

Creation of a '1_8_ScanPanel.h' class which sends an adjusted drawPixel() x,y co-ordinates to the underlying ESP32-HUB75-MatrixPanel-I2S-DMA library's drawPixel routine, to trick the output to look pixel perfect. 

Refer to the '1_8_ScanPanel.h' logic which builds upon the library's core Virtual Display 'ESP32-VirtualMatrixPanel-I2S-DMA.h' to also support chaining of 1/8 Scan Panels as well. Refer to 'ChainedPanels' example on how to configure panel chaining to create bigger displays.


