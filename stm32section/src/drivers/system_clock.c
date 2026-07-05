#include "gpio_driver.h"

void systemClockConfig()
{
    // Enable HSE (High Speed External clock)
    RCC->CR |= RCC_CR_HSEON;

    // Wait until HSE is ready
    while (!(RCC->CR & RCC_CR_HSERDY))
        ;

    // Enable prefetch, instruction cache, and data cache
    FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    // Set Flash latency to 3 wait states (required for 100 MHz)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_3WS;

    // Enable Power Control clock (Fixed: APB1ENR instead of AHB1ENR)
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    // Set voltage scaling to Scale 1 (max performance for high frequencies)
    PWR->CR |= PWR_CR_VOS;

    // Disable PLL before configuring
    RCC->CR &= ~RCC_CR_PLLON;

    // Wait until PLL is fully stopped
    while (RCC->CR & RCC_CR_PLLRDY)
        ;

    // Configure PLL for 100 MHz (using 25 MHz HSE)
    // Formula: SYSCLK = (HSE / PLLM) * PLLN / PLLP
    // Calc: (25 MHz / 25) * 200 / 2 = 100 MHz

    // Clear previous PLL configuration
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLSRC);

    // Set new PLL parameters
    RCC->PLLCFGR |= (25U << RCC_PLLCFGR_PLLM_Pos) |  // PLLM = 25
                    (200U << RCC_PLLCFGR_PLLN_Pos) | // PLLN = 200
                    // PLLP = 2 (Hardware default is 00, no need to set)
                    RCC_PLLCFGR_PLLSRC_HSE; // PLL Source = HSE

    // Enable PLL (CRITICAL FIX: Turn PLL back on after configuring)
    RCC->CR |= RCC_CR_PLLON;

    // Wait until PLL is locked and ready
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ;

    // Configure bus prescalers
    RCC->CFGR &= ~RCC_CFGR_HPRE;
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1; // AHB = SYSCLK / 1 = 100 MHz

    RCC->CFGR &= ~RCC_CFGR_PPRE1;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2; // APB1 = SYSCLK / 2 = 50 MHz (Max limit)

    RCC->CFGR &= ~RCC_CFGR_PPRE2;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1; // APB2 = SYSCLK / 1 = 100 MHz

    // Clear system clock switch bits
    RCC->CFGR &= ~RCC_CFGR_SW;

    // Select PLL as system clock
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    // Wait until PLL is successfully used as system clock
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
        ;

    // Update system core clock variable
    SystemCoreClockUpdate();
}