#include "gpio_driver.h"

void GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_Config_t *GPIO_Config)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // select gpio mod
    GPIOx->MODER &= ~(3U << (GPIO_Config->Pin * 2));
    GPIOx->MODER |= (GPIO_Config->Mode << (GPIO_Config->Pin * 2));

    // select speed
    if (GPIO_Config->Mode == GPIO_MODE_OUTPUT || GPIO_Config->Mode == GPIO_MODE_AF)
    {
        // set spped high
        GPIOx->OSPEEDR &= ~(3U << (GPIO_Config->Pin * 2));
        GPIOx->OSPEEDR |= (GPIO_Config->Speed << (GPIO_Config->Pin * 2));

        // set push-pull
        GPIOx->OTYPER &= ~(1U << GPIO_Config->Pin);
        GPIOx->OTYPER |= (GPIO_Config->OType << GPIO_Config->Pin);
    }

    GPIOx->PUPDR &= ~(0x03U << (GPIO_Config->Pin * 2));
    GPIOx->PUPDR |= (GPIO_Config->PuPd << (GPIO_Config->Pin * 2));

    if (GPIO_Config->Mode == GPIO_MODE_AF)
    {

        uint8_t reg_index = GPIO_Config->Pin / 8;
        uint8_t bit_pos = (GPIO_Config->Pin % 8) * 4;

        GPIOx->AFR[reg_index] &= ~(0x0FU << bit_pos);
        GPIOx->AFR[reg_index] |= (GPIO_Config->AltFunc << bit_pos);
    }
}

void uartPinsInit()
{
    // uart Tx pa2
    GPIO_Config_t uartConfig;

    uartConfig.Mode = GPIO_MODE_AF;
    uartConfig.Speed = GPIO_SPEED_HIGH;
    uartConfig.OType = GPIO_OTYPE_PP;
    uartConfig.PuPd = GPIO_PUPD_PU;
    uartConfig.AltFunc = 7;

    uartConfig.Pin = 2;
    GPIO_Init(GPIOA, &uartConfig);
}