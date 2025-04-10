# Copyright (c) 2019 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0
#
cmake_minimum_required(VERSION 3.20.0)

# This sample is specific to x_nucleo_iks01a3 shield. Enforce -DSHIELD option
set(SHIELD adafruit_2_8_tft_touch_v2  x_nucleo_iks01a3 )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(standard)
set(LVGL_DIR ${ZEPHYR_LVGL_MODULE_DIR})
FILE(GLOB app_sources src/*.c src/ui/*.c src/ui/images/*.c src/ui/fonts/*.c src/ui/screens/*.c src/ui/components/*.c)

target_sources(app PRIVATE ${app_sources})
