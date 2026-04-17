cmake_minimum_required(VERSION 3.16)

# Tokmagotchi firmware — SenseCAP Watcher (ESP32-S3)
# Build with ESP-IDF v5.1:
#   . $IDF_PATH/export.sh
#   idf.py set-target esp32s3
#   idf.py build flash monitor

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tokmagotchi)
