# drv_common — header-only interface library.
# Defines the portable abstractions (drv_status.h, i2c_transport.h) that both
# LIB device drivers and DRIVERS platform drivers depend on.
# Neither drv_status.h nor i2c_transport.h include any STM32 header.
add_library(drv_common INTERFACE)
target_include_directories(drv_common INTERFACE inc)