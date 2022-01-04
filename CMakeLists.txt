# HUB75 RGB LED matrix library utilizing ESP32 DMA Engine
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "esp32_i2s_parallel_dma.c" "ESP32-HUB75-MatrixPanel-I2S-DMA.cpp" "ESP32-HUB75-MatrixPanel-leddrivers.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES arduino)

project(ESP32-HUB75-MatrixPanel-I2S-DMA)
