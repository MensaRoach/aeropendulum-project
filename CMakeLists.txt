cmake_minimum_required(VERSION 3.22)

# Set the custom toolchain file BEFORE project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/.cmake/gcc-arm-none-eabi.cmake)

project(Aeropendulum C ASM)

include(.cmake/cortex_profiles.cmake)
include(.cmake/stm32.cmake)

stm32_list_boards(AVAILABLE_BOARDS)
set(BOARD_NAME "NUCLEO-F401RE" CACHE STRING "Selected board")
set_property(CACHE BOARD_NAME PROPERTY STRINGS ${AVAILABLE_BOARDS})

add_stm32_board(${BOARD_NAME})
add_subdirectory(LIB)
add_subdirectory(DRIVERS)
add_subdirectory(APPS)
