# Platform driver layer for the STM32F4 family.
# Implements the abstract transport interfaces declared in LIB/drv_common/inc/.
# Shared by all STM32F4 boards (F401, F407, etc.) because they carry the same
# I2C v1 peripheral and identical LL register sequences.
#
# The target is deliberately named `drivers` (not `stm32f4_drivers`) so that
# APPS/CMakeLists.txt needs no change when a new family is added.
#
# Note: there is NO include path to BOARDS/${BOARD_NAME} here. Driver sources
# must not depend on pin_definitions.h — choosing which peripheral a device
# sits on is the application's job. The missing path enforces this.
file(GLOB DRIVERS_SOURCES CONFIGURE_DEPENDS "src/*.c")
add_library(drivers STATIC ${DRIVERS_SOURCES})
target_include_directories(drivers PUBLIC inc)

# PUBLIC because bus_i2c.h and bus_uart.h include LL headers: any target that
# consumes those headers transitively needs the board config include paths.
target_link_libraries(drivers PUBLIC ${BOARD_NAME}_config drv_common)

if(TARGET ${BOARD_NAME})
    target_link_libraries(${BOARD_NAME} PRIVATE drivers)
endif()