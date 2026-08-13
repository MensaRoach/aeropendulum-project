#include "main_app.h"
#include "app_selector.h"

// Include selected app headers
#ifdef APP_HELLO_USART
#include "hello_usart.h"
#endif

#ifdef APP_MPU6050_TELEMETRY
#include "mpu6050_telemetry.h"
#endif

void main_app_init(void)
{
#ifdef APP_HELLO_USART
    hello_usart_init();
#endif

#ifdef APP_MPU6050_TELEMETRY
    mpu6050_telemetry_init();
#endif
}

void main_app_loop(void)
{
#ifdef APP_HELLO_USART
    hello_usart_loop();
#endif

#ifdef APP_MPU6050_TELEMETRY
    mpu6050_telemetry_loop();
#endif
}

void app_main(void)
{
    // Application entry point
    main_app_init();
    while (1)
    {
        main_app_loop();
    }
}
