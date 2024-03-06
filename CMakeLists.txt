# HUB75 RGB LED matrix library utilizing ESP32 DMA Engine
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
# MIT License

cmake_minimum_required(VERSION 3.5)
idf_build_get_property(target IDF_TARGET)

if(ARDUINO_ARCH_ESP32 OR CONFIG_ESP32_HUB75_USE_GFX)
    list(APPEND build_dependencies arduino Adafruit-GFX-Library)
else()
    list(APPEND build_dependencies esp_lcd driver)
endif()

if(${target} STREQUAL "esp32s3")
  list(APPEND extra_srcs src/platforms/${target}/gdma_lcd_parallel16.cpp)

  # Required by gdma_lcd_parallel16.cpp
  if (NOT esp_lcd IN_LIST build_dependencies)
    list(APPEND build_dependencies esp_lcd)
  endif()
endif()

idf_component_register(SRCS "src/platforms/esp32/esp32_i2s_parallel_dma.cpp" "src/ESP32-HUB75-MatrixPanel-I2S-DMA.cpp" "src/ESP32-HUB75-MatrixPanel-leddrivers.cpp" ${extra_srcs}
                       INCLUDE_DIRS "./src"
                       )

# Dependencies cannot be added to the REQUIRES argument of `idf_component_register` because (according to the build process
# listed at https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-guides/build-system.html#build-process)
# `idf_component_register` is processed during the "Enumeration" stage which happens before the sdkconfig file is loaded
# in the "Processing" stage. So if dependencies are going to be loaded based on certain CONFIG_* variables we must
# use `target_link_libraries` instead. This is the method used by Arduino's CMakeLists.txt file.
idf_build_get_property(components BUILD_COMPONENTS)
foreach(component_name IN LISTS build_dependencies)
  if (NOT ${component_name} IN_LIST components)
    message(FATAL_ERROR "Missing component: ${component_name}")
  endif()
  idf_component_get_property(lib_name ${component_name} COMPONENT_LIB)
  target_link_libraries(${COMPONENT_LIB} PUBLIC ${lib_name})
endforeach()

# In case you are running into issues with "missing" header files from 3rd party libraries
# you can add them to the REQUIRES section above. If you use some of the build options below
# you probably want to remove (NO_GFX) or replace Adafruit-GFX-Library (USE_GFX_ROOT)

# Example to build with USE_GFX_ROOT or NO_GFX / just uncomment the appropriate line
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DUSE_GFX_ROOT)
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX)

# esp-idf does not have any GFX library support yet, so we need to define NO_GFX
if(ARDUINO_ARCH_ESP32 OR CONFIG_ESP32_HUB75_USE_GFX)
else()
    target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX)
    if(${target} STREQUAL "esp32s3")
		# Don't enable PSRAM based framebuffer just because it's an S3.
		# This is an advanced option and should only be used with an S3 with Octal-SPI RAM.
        # target_compile_options(${COMPONENT_TARGET} PUBLIC -DSPIRAM_FRAMEBUFFER)
		target_compile_options(${COMPONENT_TARGET} PUBLIC)
    endif()
endif()

# You can also use multiple options like this
# target_compile_options(${COMPONENT_TARGET} PUBLIC -DNO_GFX -DNO_FAST_FUNCTIONS)

# All options can be found here:
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/blob/master/doc/BuildOptions.md

project(ESP32-HUB75-MatrixPanel-I2S-DMA)
