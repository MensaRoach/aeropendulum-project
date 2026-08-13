#include "mpu6050_telemetry.h"
#include "cobs.h"
#include "pin_definitions.h"
#include "stm32f4xx_ll_i2c.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_gpio.h"

#define MPU6050_ADDR 0xD0

static void MPU6050_WriteReg(uint8_t reg, uint8_t data)
{
    LL_I2C_GenerateStartCondition(MPU6050_I2C);
    while (!LL_I2C_IsActiveFlag_SB(MPU6050_I2C))
        ;

    LL_I2C_TransmitData8(MPU6050_I2C, MPU6050_ADDR);
    while (!LL_I2C_IsActiveFlag_ADDR(MPU6050_I2C))
        ;
    LL_I2C_ClearFlag_ADDR(MPU6050_I2C);

    while (!LL_I2C_IsActiveFlag_TXE(MPU6050_I2C))
        ;
    LL_I2C_TransmitData8(MPU6050_I2C, reg);

    while (!LL_I2C_IsActiveFlag_TXE(MPU6050_I2C))
        ;
    LL_I2C_TransmitData8(MPU6050_I2C, data);

    while (!LL_I2C_IsActiveFlag_BTF(MPU6050_I2C))
        ;
    LL_I2C_GenerateStopCondition(MPU6050_I2C);
}

static void MPU6050_ReadRegs(uint8_t reg, uint8_t *buffer, uint8_t size)
{
    LL_I2C_AcknowledgeNextData(MPU6050_I2C, LL_I2C_ACK);

    LL_I2C_GenerateStartCondition(MPU6050_I2C);
    while (!LL_I2C_IsActiveFlag_SB(MPU6050_I2C))
        ;

    LL_I2C_TransmitData8(MPU6050_I2C, MPU6050_ADDR);
    while (!LL_I2C_IsActiveFlag_ADDR(MPU6050_I2C))
        ;
    LL_I2C_ClearFlag_ADDR(MPU6050_I2C);

    while (!LL_I2C_IsActiveFlag_TXE(MPU6050_I2C))
        ;
    LL_I2C_TransmitData8(MPU6050_I2C, reg);
    while (!LL_I2C_IsActiveFlag_BTF(MPU6050_I2C))
        ;

    LL_I2C_GenerateStartCondition(MPU6050_I2C);
    while (!LL_I2C_IsActiveFlag_SB(MPU6050_I2C))
        ;

    LL_I2C_TransmitData8(MPU6050_I2C, MPU6050_ADDR | 0x01); // Read
    while (!LL_I2C_IsActiveFlag_ADDR(MPU6050_I2C))
        ;

    if (size == 1)
    {
        LL_I2C_AcknowledgeNextData(MPU6050_I2C, LL_I2C_NACK);
        LL_I2C_ClearFlag_ADDR(MPU6050_I2C);
        LL_I2C_GenerateStopCondition(MPU6050_I2C);
        while (!LL_I2C_IsActiveFlag_RXNE(MPU6050_I2C))
            ;
        buffer[0] = LL_I2C_ReceiveData8(MPU6050_I2C);
    }
    else
    {
        LL_I2C_ClearFlag_ADDR(MPU6050_I2C);
        while (size > 3)
        {
            while (!LL_I2C_IsActiveFlag_RXNE(MPU6050_I2C))
                ;
            *buffer++ = LL_I2C_ReceiveData8(MPU6050_I2C);
            size--;
        }
        
        // Now size == 3
        while (!LL_I2C_IsActiveFlag_BTF(MPU6050_I2C))
            ;
        
        // DR contains byte N-2, Shift Reg contains byte N-1
        LL_I2C_AcknowledgeNextData(MPU6050_I2C, LL_I2C_NACK); // Send NACK for byte N
        *buffer++ = LL_I2C_ReceiveData8(MPU6050_I2C); // Read byte N-2
        
        // Now wait for BTF again (byte N-1 in DR, byte N in Shift Reg)
        while (!LL_I2C_IsActiveFlag_BTF(MPU6050_I2C))
            ;
            
        LL_I2C_GenerateStopCondition(MPU6050_I2C);
        *buffer++ = LL_I2C_ReceiveData8(MPU6050_I2C); // Read byte N-1
        *buffer++ = LL_I2C_ReceiveData8(MPU6050_I2C); // Read byte N
    }
}

static void USART2_Send(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        while (!LL_USART_IsActiveFlag_TXE(TELEMETRY_USART))
            ;
        LL_USART_TransmitData8(TELEMETRY_USART, data[i]);
    }
}

static void i2c_bus_recovery(void) {
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Configure SCL and SDA as Output Open-Drain
    GPIO_InitStruct.Pin = MPU6050_SCL_PIN | MPU6050_SDA_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
    LL_GPIO_Init(MPU6050_I2C_PORT, &GPIO_InitStruct);
    
    // Set both high
    LL_GPIO_SetOutputPin(MPU6050_I2C_PORT, MPU6050_SCL_PIN | MPU6050_SDA_PIN);
    LL_mDelay(1);
    
    // Toggle SCL 9 times
    for(int i=0; i<9; i++) {
        LL_GPIO_ResetOutputPin(MPU6050_I2C_PORT, MPU6050_SCL_PIN);
        LL_mDelay(1);
        LL_GPIO_SetOutputPin(MPU6050_I2C_PORT, MPU6050_SCL_PIN);
        LL_mDelay(1);
    }
    
    // Generate STOP condition manually (SDA low, SCL high, SDA high)
    LL_GPIO_ResetOutputPin(MPU6050_I2C_PORT, MPU6050_SDA_PIN);
    LL_mDelay(1);
    LL_GPIO_SetOutputPin(MPU6050_I2C_PORT, MPU6050_SCL_PIN);
    LL_mDelay(1);
    LL_GPIO_SetOutputPin(MPU6050_I2C_PORT, MPU6050_SDA_PIN);
    LL_mDelay(1);
    
    // Reconfigure SCL and SDA as Alternate Function (AF4 for I2C)
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
    LL_GPIO_Init(MPU6050_I2C_PORT, &GPIO_InitStruct);
}

void mpu6050_telemetry_init(void)
{
    // Perform I2C bus recovery in case the MPU6050 is stuck holding SDA low
    i2c_bus_recovery();
    
    LL_I2C_Enable(MPU6050_I2C);
    LL_mDelay(100);

    // Wake up MPU6050 (PWR_MGMT_1 register)
    MPU6050_WriteReg(0x6B, 0x00);
    LL_mDelay(10);

    // Set accelerometer range to +/- 2g (ACCEL_CONFIG register)
    MPU6050_WriteReg(0x1C, 0x00);

    // Set gyroscope range to +/- 250 deg/s (GYRO_CONFIG register)
    MPU6050_WriteReg(0x1B, 0x00);
}

void mpu6050_telemetry_loop(void)
{
    MPU6050_Data_t mpu_data;
    uint8_t buffer[14];

    // Read 14 bytes starting from ACCEL_XOUT_H (0x3B)
    MPU6050_ReadRegs(0x3B, buffer, 14);

    // MPU6050 returns MSB first, so we need to assemble the 16-bit values
    mpu_data.accel_x = (int16_t)((buffer[0] << 8) | buffer[1]);
    mpu_data.accel_y = (int16_t)((buffer[2] << 8) | buffer[3]);
    mpu_data.accel_z = (int16_t)((buffer[4] << 8) | buffer[5]);
    mpu_data.temp = (int16_t)((buffer[6] << 8) | buffer[7]);
    mpu_data.gyro_x = (int16_t)((buffer[8] << 8) | buffer[9]);
    mpu_data.gyro_y = (int16_t)((buffer[10] << 8) | buffer[11]);
    mpu_data.gyro_z = (int16_t)((buffer[12] << 8) | buffer[13]);

    // Encode with COBS
    uint8_t cobs_buffer[sizeof(MPU6050_Data_t) + 2]; // Max overhead is 1 byte + 1 byte for trailing zero
    size_t encoded_len = cobs_encode(&mpu_data, sizeof(MPU6050_Data_t), cobs_buffer);
    cobs_buffer[encoded_len] = 0x00; // Append trailing zero

    // Send over USART2
    USART2_Send(cobs_buffer, encoded_len + 1);

    // Wait a bit (e.g., send at ~100Hz)
    LL_mDelay(10);
}
