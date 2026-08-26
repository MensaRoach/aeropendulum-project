file(GLOB MPU6050_SOURCES CONFIGURE_DEPENDS "src/*.c")
add_library(mpu6050 STATIC ${MPU6050_SOURCES})
target_include_directories(mpu6050 PUBLIC inc)

# Link _cpu only — never _config.
# _cpu gives the compiler the MCU ABI (CPU/FPU flags) without granting access
# to stm32f4xx_ll_*.h or any other board include path. This makes it a build
# error to #include an STM32 peripheral header from inside this library, which
# is the enforcement mechanism for its portability claim.
target_link_libraries(mpu6050 PUBLIC drv_common PRIVATE ${BOARD_NAME}_cpu)