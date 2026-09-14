cmake_minimum_required(VERSION 3.16.0)

add_compile_definitions(
  BOARD_HAS_PSRAM
  ARDUINO_USB_MODE=1
  ARDUINO_USB_CDC_ON_BOOT=1
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32_S3_Smoke_Test)
