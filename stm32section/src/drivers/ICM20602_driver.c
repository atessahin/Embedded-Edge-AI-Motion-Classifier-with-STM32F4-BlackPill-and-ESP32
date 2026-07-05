#include "gpio_driver.h"
#include "icm20602_driver.h"
#include "communication.h"
#include "delay.h"
IMU_Data_t imuData;

IMU_ProcessedData_t imuProcessed;
int32_t gyroX_offset = 0, gyroY_offset = 0, gyroZ_offset = 0;
int32_t accelX_offset = 0, accelY_offset = 0;
void ICM20602_SPI_Pins_Init()
{
    GPIO_Config_t spiConfig;
    spiConfig.Mode = GPIO_MODE_AF;
    spiConfig.Speed = GPIO_SPEED_HIGH;
    spiConfig.OType = GPIO_OTYPE_PP;
    spiConfig.PuPd = GPIO_PUPD_NONE;
    spiConfig.AltFunc = 5;

    // spi1 sck-PB3
    spiConfig.Pin = 3;
    GPIO_Init(GPIOB, &spiConfig);
    // spi1-miso-pb4
    spiConfig.Pin = 4;
    GPIO_Init(GPIOB, &spiConfig);
    // spi1-mosi-pb5
    spiConfig.Pin = 5;
    GPIO_Init(GPIOB, &spiConfig);

    GPIO_Config_t csPin;
    csPin.Mode = GPIO_MODE_OUTPUT;
    csPin.Speed = GPIO_SPEED_HIGH;
    csPin.OType = GPIO_OTYPE_PP;
    csPin.PuPd = GPIO_PUPD_PU;
    csPin.Pin = 15;
    GPIO_Init(GPIOA, &csPin);
    GPIOA->BSRR = (1U << 15);
}

void ICM20602_init()
{
    uint8_t who_am_i_val;

    spiWrite(PWR_MGMT_1, 0x80U);
    delay_ms(100);

    spiWrite(I2C_IF, 0x40U);
    delay_ms(1);

    who_am_i_val = spiRead(WHO_AM_I);
    if (who_am_i_val != ICM20602_WHO_AM_I_EXPECTED)
    {
        /* code */
    }

    spiWrite(PWR_MGMT_1, 0x01U);
    delay_ms(10);
    spiWrite(CONFIG, 0x03U);
    spiWrite(SMPLRT_DIV, 9U);
    spiWrite(GYRO_CONFIG, 0x10);
    spiWrite(ACCEL_CONFIG, 0x10);
}

void readMpuData()
{
    uint8_t dataBuff[14] = {0};

    CS_LOW();
    // open spi read mode make it register|0x80
    SPI1_Transfer(0xBB);
    for (int i = 0; i < 14; i++)
    {

        dataBuff[i] = SPI1_Transfer(0x00);
    }
    CS_HIGH();
    // burst read
    imuData.Accel_X = (int16_t)((dataBuff[0] << 8) | dataBuff[1]);
    imuData.Accel_Y = (int16_t)((dataBuff[2] << 8) | dataBuff[3]);
    imuData.Accel_Z = (int16_t)((dataBuff[4] << 8) | dataBuff[5]);

    // collabBuf[6] ve [7]  (TEMP_OUT)

    imuData.Gyro_X = (int16_t)((dataBuff[8] << 8) | dataBuff[9]);
    imuData.Gyro_Y = (int16_t)((dataBuff[10] << 8) | dataBuff[11]);
    imuData.Gyro_Z = (int16_t)((dataBuff[12] << 8) | dataBuff[13]);
}

// for calib cause Z axis drift even while stable
void calibrate_ICM20602()
{
    for (int i = 0; i < 1000; i++)
    {
        readMpuData();
        gyroX_offset += imuData.Gyro_X;
        gyroY_offset += imuData.Gyro_Y;
        gyroZ_offset += imuData.Gyro_Z;

        accelX_offset += imuData.Accel_X;
        accelY_offset += imuData.Accel_Y;
        delay_ms(2);
    }

    gyroX_offset /= 1000;
    gyroY_offset /= 1000;
    gyroZ_offset /= 1000;
    accelX_offset /= 1000;
    accelY_offset /= 1000;
}
void calibratededData()
{
    /*
    according to datasheet

    accel (±8g)  LSB/g: 4096

    gyroscop (±1000 dps)  LSB/dps : 32.8*/

    imuProcessed.Accel_X_g = (imuData.Accel_X - accelX_offset) / 4096.0f;
    imuProcessed.Accel_Y_g = (imuData.Accel_Y - accelY_offset) / 4096.0f;
    imuProcessed.Accel_Z_g = (imuData.Accel_Z) / 4096.0f;

    imuProcessed.Gyro_X_dps = (imuData.Gyro_X - gyroX_offset) / 32.8f;
    imuProcessed.Gyro_Y_dps = (imuData.Gyro_Y - gyroY_offset) / 32.8f;
    imuProcessed.Gyro_Z_dps = (imuData.Gyro_Z - gyroZ_offset) / 32.8f;
}