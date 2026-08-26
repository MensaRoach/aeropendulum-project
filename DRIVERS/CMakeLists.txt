# Select the platform-specific driver subdirectory based on MCU_FAMILY from
# board.cmake (promoted to a cache variable by add_stm32_board()).
# This is keyed per-family, not per-board: both STM32F401 and STM32F407 use
# the same I2C v1 peripheral and share one DRIVERS/stm32f4/ implementation.
string(TOLOWER "${MCU_FAMILY}" _family_dir)   # e.g. STM32F4 -> stm32f4
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_family_dir}")
    message(FATAL_ERROR
        "No platform drivers for MCU_FAMILY '${MCU_FAMILY}' "
        "(looked for DRIVERS/${_family_dir}/). "
        "Add a DRIVERS/${_family_dir}/ directory with its own CMakeLists.txt.")
endif()
add_subdirectory(${_family_dir})