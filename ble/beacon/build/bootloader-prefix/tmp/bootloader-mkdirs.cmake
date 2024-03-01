# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/willow/esp/v5.1.2/esp-idf/components/bootloader/subproject"
  "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader"
  "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix"
  "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix/tmp"
  "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix/src"
  "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/willow/esp/project/esp32_s3_lcd_ev_board/esp32/ble/beacon/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
