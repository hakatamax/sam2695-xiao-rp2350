cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(sam2695_emulator C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(sam2695_emulator
    main.cpp
)

target_link_libraries(sam2695_emulator
    pico_stdlib
    hardware_pwm
    hardware_clocks
)

pico_enable_stdio_usb(sam2695_emulator 1)
pico_enable_stdio_uart(sam2695_emulator 0)

pico_add_extra_outputs(sam2695_emulator)
