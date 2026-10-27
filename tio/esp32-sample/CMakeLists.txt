# The ESP32 sample, an ESP-IDF project. ebisu is built as the component in
# components/ebisu, from the repository's sources; the network, Wi-Fi on a board
# or Ethernet under QEMU, is brought up by ESP-IDF's protocol_examples_common.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ebisu_esp32_sample)
