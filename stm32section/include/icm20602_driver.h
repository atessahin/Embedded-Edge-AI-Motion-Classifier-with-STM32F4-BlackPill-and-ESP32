#ifndef ICM20602_H
#define ICM20602_H
#define ICM20602_WHO_AM_I_EXPECTED 0x12U

#define SMPLRT_DIV 0x19U
#define CONFIG 0x1AU
#define GYRO_CONFIG 0x1BU
#define ACCEL_CONFIG 0x1CU
#define ACCEL_CONFIG2 0x1DU
#define FIFO_EN 0x23U
#define INT_PIN_CFG 0x37U
#define INT_ENABLE 0x38U
#define INT_STATUS 0x3AU
#define ACCEL_XOUT_H 0x3BU
#define TEMP_OUT_H 0x41U
#define GYRO_XOUT_H 0x43U
#define ACCEL_INTEL_CTRL 0x69U
#define USER_CTRL 0x6AU
#define PWR_MGMT_1 0x6BU
#define PWR_MGMT_2 0x6CU
#define I2C_IF 0x70U
#define WHO_AM_I 0x75U

#define ICM_READ 0x80U
#define ICM_WRITE 0x00U

#define CS_LOW() (GPIOA->BSRR = (1UL << (15U + 16U)))
#define CS_HIGH() (GPIOA->BSRR = (1UL << 15U))

void ICM20602_SPI_Pins_Init();
void ICM20602_init();
typedef struct
{
    int16_t Accel_X;
    int16_t Accel_Y;
    int16_t Accel_Z;
    int16_t Gyro_X;
    int16_t Gyro_Y;
    int16_t Gyro_Z;
} IMU_Data_t;

typedef struct
{
    float Accel_X_g;
    float Accel_Y_g;
    float Accel_Z_g;
    float Gyro_X_dps; // degrees per second
    float Gyro_Y_dps; // degrees per second
    float Gyro_Z_dps; // degrees per second
} IMU_ProcessedData_t;

extern IMU_ProcessedData_t imuProcessed;

extern IMU_Data_t imuData;

void readMpuData();
void calibrate_ICM20602();
void calibratededData();
#endif /* ICM20602_H */