cmake_minimum_required(VERSION 3.22)

# Set the custom toolchain file BEFORE project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/.cmake/gcc-arm-none-eabi.cmake)

project(Aeropendulum C ASM)

include(.cmake/cortex_profiles.cmake)
include(.cmake/stm32.cmake)

stm32_list_boards(AVAILABLE_BOARDS)

# No default board. A bare `cmake ..` with nothing selected must fail loudly
# rather than silently building for whichever board happened to be tracked
# first — with two boards in the tree now, guessing is worse than asking.
set(BOARD_NAME "" CACHE STRING "Selected board (required)")
set_property(CACHE BOARD_NAME PROPERTY STRINGS ${AVAILABLE_BOARDS})

if(NOT BOARD_NAME)
    string(REPLACE ";" ", " AVAILABLE_BOARDS_STR "${AVAILABLE_BOARDS}")
    message(FATAL_ERROR
        "No board selected. Available boards: ${AVAILABLE_BOARDS_STR}.\n"
        "Configure with a preset, e.g.:\n"
        "    cmake --preset NUCLEO-F401RE-Debug\n"
        "or set BOARD_NAME explicitly:\n"
        "    cmake -B .build/mybuild -D BOARD_NAME=NUCLEO-F401RE")
endif()

add_stm32_board(${BOARD_NAME})
add_subdirectory(LIB)
add_subdirectory(DRIVERS)
add_subdirectory(APPS)
