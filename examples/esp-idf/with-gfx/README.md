# ESP-IDF Example With Adafruit GFX Library

This folder contains example code for using this library with `esp-idf` and the [Adafruit GFX library](https://github.com/adafruit/Adafruit-GFX-Library).

First, follow the [Getting Started Guide for ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to install ESP-IDF onto your computer.

When you are ready to start your first project with this library, follow folow these steps:

  1. Copy the files in this folder (and sub folders) into a new directory for your project.
  1. Clone the required repositories:
     ```
     git clone https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA.git components/ESP32-HUB75-MatrixPanel-I2S-DMA
     git clone https://github.com/adafruit/Adafruit-GFX-Library.git components/Adafruit-GFX-Library
     git clone https://github.com/adafruit/Adafruit_BusIO.git components/Adafruit_BusIO
     git clone https://github.com/espressif/arduino-esp32.git components/arduino
     ```
  1. Build your project: `idf.py build`
