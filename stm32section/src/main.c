#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#include "stm32f4xx.h"
#include "gpio_driver.h"
#include "system_clock.h"

#include "icm20602_driver.h"
#include "communication.h"
#include "delay.h"

TaskHandle_t readDataTask;

void collectData(void *pvParameters);

int main(void)
{
    systemClockConfig();
    delayInit();

    ICM20602_SPI_Pins_Init();
    uartPinsInit();
    delay_ms(10);

    spiConfig();
    dmaConfig();
    uartConfig();
    delay_ms(10);

    ICM20602_init();
    delay_ms(10);
    calibrate_ICM20602();

    xTaskCreate(collectData, "READ", 512, NULL, 1, &readDataTask);
    vTaskStartScheduler();

    while (1)
    {
    }
}
// uint8_t uartTxBuffer[128];
void collectData(void *pvParameters)
{

    (void)pvParameters;

    uint8_t uartTxBuffer[128];

    uint16_t index;
    while (1)
    {
        readMpuData();

        calibratededData();

        index = 0;

        index += intToStr((int32_t)(imuProcessed.Accel_X_g * 100), &uartTxBuffer[index]);
        uartTxBuffer[index++] = ',';

        index += intToStr((int32_t)(imuProcessed.Accel_Y_g * 100), &uartTxBuffer[index]);
        uartTxBuffer[index++] = ',';

        index += intToStr((int32_t)(imuProcessed.Accel_Z_g * 100), &uartTxBuffer[index]);

        uartTxBuffer[index++] = '\r';
        uartTxBuffer[index++] = '\n';

        dmaUart2Tx(uartTxBuffer, index);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}