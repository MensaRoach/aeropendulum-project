#include "blinky.h"
#include "pin_definitions.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_utils.h"

#define BLINK_PERIOD_MS 500

void blinky_init(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    // MX_GPIO_Init() already enabled the status LED's port clock, but it
    // leaves the pin in analog mode along with the other unused pins, so
    // claim it as an output.
    GPIO_InitStruct.Pin = STATUS_LED_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(STATUS_LED_PORT, &GPIO_InitStruct);

    LL_GPIO_ResetOutputPin(STATUS_LED_PORT, STATUS_LED_PIN);
}

void blinky_loop(void)
{
    LL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
    LL_mDelay(BLINK_PERIOD_MS);
}
