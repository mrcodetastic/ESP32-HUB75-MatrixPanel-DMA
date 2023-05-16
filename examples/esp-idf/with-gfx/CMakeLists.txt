# This is a boilerplate top-level project CMakeLists.txt file.
# This is the primary file which CMake uses to learn how to build the project.
#
# Most of the important stuff happens in the 'main' directory.
#
# See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#example-project for more details.
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(with-gfx)
