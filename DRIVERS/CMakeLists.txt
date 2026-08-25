# Find all source files in the src directory
file(GLOB DRIVERS_SOURCES CONFIGURE_DEPENDS "src/*.c")

# DRIVERS will be a static library linked to the board executable
add_library(drivers STATIC ${DRIVERS_SOURCES})

target_include_directories(drivers PUBLIC inc)

# PUBLIC, not PRIVATE: bus_i2c.h and bus_uart.h include LL headers, so anything
# consuming those headers needs the board config's include paths and defines too.
#
# Note there is deliberately no include path to BOARDS/${BOARD_NAME} here. Driver
# sources must not depend on that board's pin_definitions.h aliases -- picking the
# concrete peripheral is the application's job, and leaving the path out is what
# makes that rule enforced rather than merely documented.
target_link_libraries(drivers PUBLIC ${BOARD_NAME}_config)

# Link DRIVERS to the board executable
if(TARGET ${BOARD_NAME})
    target_link_libraries(${BOARD_NAME} PRIVATE drivers)
endif()
