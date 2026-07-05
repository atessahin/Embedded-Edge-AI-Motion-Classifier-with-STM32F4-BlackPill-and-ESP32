#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdint.h>
#include "stm32f411xe.h" // CMSIS donanım adreslerinin olduğu resmi kütüphane

// Pin Modları (MODER)
typedef enum
{
    GPIO_MODE_INPUT = 0x00,
    GPIO_MODE_OUTPUT = 0x01,
    GPIO_MODE_AF = 0x02,
    GPIO_MODE_ANALOG = 0x03
} GPIO_Mode_t;

// Çıkış Hızları (OSPEEDR)
typedef enum
{
    GPIO_SPEED_LOW = 0x00,
    GPIO_SPEED_MED = 0x01,
    GPIO_SPEED_FAST = 0x02,
    GPIO_SPEED_HIGH = 0x03
} GPIO_Speed_t;

// Çıkış Tipi (OTYPER)
typedef enum
{
    GPIO_OTYPE_PP = 0x00, // Push-Pull (Genel kullanım, SPI, UART vb.)
    GPIO_OTYPE_OD = 0x01  // Open-Drain (I2C vb.)
} GPIO_OType_t;

// Pull-Up / Pull-Down (PUPDR)
typedef enum
{
    GPIO_PUPD_NONE = 0x00,
    GPIO_PUPD_PU = 0x01,
    GPIO_PUPD_PD = 0x02
} GPIO_PuPd_t;

typedef struct
{
    uint8_t Pin;        // 0 - 15 arası pin numarası
    GPIO_Mode_t Mode;   // Çalışma modu
    GPIO_Speed_t Speed; // Sinyal hızı
    GPIO_OType_t OType; // Çıkış tipi
    GPIO_PuPd_t PuPd;   // Pull-up direnci durumu
    uint8_t AltFunc;    // Alternate Function numarası
} GPIO_Config_t;

// Fonksiyon Prototipi
void GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_Config_t *GPIO_Config);
void uartPinsInit();

#endif /* GPIO_DRIVER_H */