#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
#set(SHIELD ssd1306_128x64)
project(nrf_dm)

include_directories(src/inc)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
  src/scan.c
  src/peer.c
  src/color.c
)

target_sources_ifdef(CONFIG_DISTANCE_DISPLAY_OLED app PRIVATE src/display.c src/logo.c)
# NORDIC SDK APP END

zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
