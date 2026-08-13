cmake_minimum_required(VERSION 3.22)

# Set the custom toolchain file BEFORE project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

project(Aeropendulum C ASM)

# Include our custom STM32 build script
include(cmake/stm32.cmake)

# Define which board to build
set(BOARD_NAME "NUCLEO-F401RE" CACHE STRING "Selected Board")

# Build the STM32 board
# We specify the MCU symbol (STM32F401xE) based on the board
add_stm32_board(BOARDS/${BOARD_NAME} ${BOARD_NAME} STM32F401xE)

# Add the APPS directory
add_subdirectory(APPS)
