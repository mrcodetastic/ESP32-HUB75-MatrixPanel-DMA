# HUB75 RGB LED matrix library utilizing ESP32 DMA Engine
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
# MIT License

cmake_minimum_required(VERSION 3.5)
idf_build_get_property(target IDF_TARGET)

if(ARDUINO_ARCH_ESP32)
    list(APPEND arduino_build arduino Adafruit-GFX-Library)
else()
    list(APPEND esp_idf_build esp_lcd driver)
endif()
idf_component_register(SRCS "src/platforms/esp32/esp32_i2s_parallel_dma.cpp" "src/ESP32-HUB75-MatrixPanel-I2S-DMA.cpp" "src/ESP32-HUB75-MatrixPanel-leddrivers.cpp"
                       src/platforms/${target}/gdma_lcd_parallel16.cpp
                       INCLUDE_DIRS "./src" 
                       REQUIRES ${arduino_build} ${esp_idf_build}
                       )

# In case you are running into issues with "missing" header files from 3rd party libraries
# you can add them to the REQUIRES section above. If you use some of the build options below
# you probably want to remove (NO_GFX) or replace Adafruit-GFX-Library (USE_GFX_ROOT)

# Example to build with USE_GFX_ROOT or NO_GFX / just uncomment the appropriate line
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DUSE_GFX_ROOT)
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX)

# esp-idf does not have any GFX library support yet, so we need to define NO_GFX
if(ARDUINO_ARCH_ESP32)
else()
    target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX)
    if(${target} STREQUAL "esp32s3")
        target_compile_options(${COMPONENT_TARGET} PUBLIC -DSPIRAM_FRAMEBUFFER)
    endif()
endif()

# You can also use multiple options like this
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX -DNO_FAST_FUNCTIONS)

# All options can be found here:
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/blob/master/doc/BuildOptions.md

project(ESP32-HUB75-MatrixPanel-I2S-DMA)
