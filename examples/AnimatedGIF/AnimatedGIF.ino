/*
 * Written by: Craig A. Lindley
 *
 * Copyright (c) 2014 Craig A. Lindley
 * Refactoring by Louis Beaudoin (Pixelmatix)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
 
#include <ESP32-RGB64x32MatrixPanel-I2S-DMA.h>
#include <SPIFFS.h>
#include <Arduino.h>

RGB64x32MatrixPanel_I2S_DMA matrix;


#include "GifDecoder.h"
#include "FilenameFunctions.h"

/* GIF files for this particular example need to be put into the 'data' directoy of the
 * sketch and saved to the ESP32 using the "ESP32 Sketch Data Upload" tool in Arduino.
 * 
 * URL: https://github.com/me-no-dev/arduino-esp32fs-plugin
 * 
 */
#define GIF_DIRECTORY "/"
#define DISPLAY_TIME_SECONDS 5

// Gif sizes should match exactly that of the RGB Matrix display.
const uint8_t GIFWidth = 64;     
const uint8_t GIFHeight = 32;   

/* template parameters are maxGifWidth, maxGifHeight, lzwMaxBits
 * 
 * The lzwMaxBits value of 12 supports all GIFs, but uses 16kB RAM
 * lzwMaxBits can be set to 10 or 11 for small displays, 12 for large displays
 * All 32x32-pixel GIFs tested work with 11, most work with 10
 */
GifDecoder<GIFWidth, GIFHeight, 12> decoder;

int num_files;

void screenClearCallback(void) {
  matrix.fillScreen(matrix.color565(0,0,0));
}

void updateScreenCallback(void) {
  //backgroundLayer.swapBuffers();
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
    matrix.drawPixelRGB888(x, y, red, green, blue);
}

// Setup method runs once, when the sketch starts
void setup() {
  
    Serial.begin(115200);
    Serial.println("Starting AnimatedGIFs Sketch");
 
    // Start filesystem
    Serial.println(" * Loading SPIFFS");
    if(!SPIFFS.begin()){
          Serial.println("SPIFFS Mount Failed");
    }

    decoder.setScreenClearCallback(screenClearCallback);
    decoder.setUpdateScreenCallback(updateScreenCallback);
    decoder.setDrawPixelCallback(drawPixelCallback);

    decoder.setFileSeekCallback(fileSeekCallback);
    decoder.setFilePositionCallback(filePositionCallback);
    decoder.setFileReadCallback(fileReadCallback);
    decoder.setFileReadBlockCallback(fileReadBlockCallback);

    matrix.begin();

    // Clear screen
    matrix.fillScreen(matrix.color565(0,0,0));
 
    // Determine how many animated GIF files exist
    num_files = enumerateGIFFiles(GIF_DIRECTORY, false);

    if(num_files < 0) {
        Serial.println("No gifs directory");
        while(1);
    }

    if(!num_files) {
        Serial.println("Empty gifs directory");
        while(1);
    }
}

int file_index = -1;
void loop() {
    static unsigned long futureTime;

   // int index = -1; //random(num_files);

    if(futureTime < millis()) {
        if (++file_index >= num_files) {
            file_index = 0;
        }

        if (openGifFilenameByIndex(GIF_DIRECTORY, file_index) >= 0) {
            // Can clear screen for new animation here, but this might cause flicker with short animations
            // matrix.fillScreen(COLOR_BLACK);
            // matrix.swapBuffers();

            decoder.startDecoding();

            // Calculate time in the future to terminate animation
            futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
        }
    }

    decoder.decodeFrame();
}
