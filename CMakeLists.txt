# HUB75 RGB LED matrix library utilizing ESP32 DMA Engine
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "esp32_i2s_parallel_dma.c" "ESP32-HUB75-MatrixPanel-I2S-DMA.cpp" "ESP32-HUB75-MatrixPanel-leddrivers.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES arduino Adafruit-GFX-Library)

# In case you are running into issues with "missing" header files from 3rd party libraries
# you can add them to the REQUIRES section above. If you use some of the build options below
# you probably want to remove (NO_GFX) or replace Adafruit-GFX-Library (USE_GFX_ROOT)

# Example to build with USE_GFX_ROOT or NO_GFX / just uncomment the appropriate line
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DUSE_GFX_ROOT)
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX)

# You can also use multiple options like this
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX -DNO_FAST_FUNCTIONS)

# All options can be found here:
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/blob/master/doc/BuildOptions.md

project(ESP32-HUB75-MatrixPanel-I2S-DMA)
