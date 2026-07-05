#include "gpio_driver.h"
#include "icm20602_driver.h"
#include "communication.h"
uint8_t SPI1_Transfer(uint8_t data);
void spiConfig()
{
    // spi1 clock enable
    RCC->APB2ENR |= (1 << 12);
    // spi1 disable
    SPI1->CR1 &= ~(1 << 6);
    // baud rate control
    SPI1->CR1 |= (3 << 3);
    // data format 8bit
    SPI1->CR1 &= ~(1 << 11);
    // master selection
    SPI1->CR1 |= (1 << 2);
    // MSB format
    SPI1->CR1 &= ~(1 << 7);
    // software slave man and nss pind and ıo val of the nss pin ignore
    SPI1->CR1 |= (1 << 8) | (1 << 9);
    // cpol=0 cpha=0
    SPI1->CR1 &= ~(1 << 1);
    SPI1->CR1 &= ~(1 << 0);
    // start
    SPI1->CR1 |= (1 << 6);
}

void spiWrite(uint8_t reg, uint8_t value)
{

    CS_LOW();

    SPI1_Transfer(reg & 0x7FU);
    SPI1_Transfer(value);

    CS_HIGH();
}

uint8_t spiRead(uint8_t reg)
{

    uint8_t value;
    CS_LOW();

    SPI1_Transfer(reg | 0x80);
    value = SPI1_Transfer(0x00);

    CS_HIGH();
    return value;
}
uint8_t SPI1_Transfer(uint8_t data)
{
    // check  Transmit buffer empty
    while (!(SPI1->SR & (1 << 1)))
        ;
    // send data
    SPI1->DR = data;
    // check  receive buffer empty
    while (!(SPI1->SR & (1 << 0)))
        ;

    uint8_t receive = (uint8_t)SPI1->DR;
    // check busy flag
    while (SPI1->SR & (1 << 7))
        ;
    return receive;
}

void uartConfig()
{
    // uart2 enable
    RCC->APB1ENR |= (1 << 17);
    // uart disable
    USART2->CR1 &= ~(1 << 13);
    // set baudrate 115200
    USART2->BRR = 0x1B2;
    // oversample 16
    USART2->CR1 &= ~(1 << 15);
    // word length
    USART2->CR1 &= ~(1 << 12);
    // parity disable
    USART2->CR1 &= ~(1 << 10);
    // tx enable rx enable
    USART2->CR1 |= (1 << 3);
    // rx enable
    USART2->CR1 |= (1 << 2);
    // stop bit 1
    USART2->CR2 &= ~(3U << 12);
    // dma tx rx enable
    USART2->CR3 |= (1 << 7);
    USART2->CR3 |= (1 << 6);
    // enable uart
    USART2->CR1 |= (1 << 13);
}
/*DMA1 Stream 6=USART2_TX(Channel 4) Stream 7= USART2_RX(Channel 6)*/
void dmaConfig()
{
    // dma1 enable
    RCC->AHB1ENR |= (1 << 21);
    // dma disable
    DMA1_Stream6->CR &= ~(1 << 0);
    while (DMA1_Stream6->CR & (1 << 0))
        ;
    // transfer direction  Memory-to-peripheral
    DMA1_Stream6->CR |= (1 << 6);
    // circular mode off
    DMA1_Stream6->CR &= ~(1 << 8);
    // peripheral increment mode off
    DMA1_Stream6->CR &= ~(1 << 9);
    // memory increment mode off
    DMA1_Stream6->CR |= (1 << 10);
    // priorty level
    DMA1_Stream6->CR |= (2U << 16);
    // channel selection
    DMA1_Stream6->CR |= (4U << 25);
    // perhipal and memeory data size = 8 bit
    DMA1_Stream6->CR &= ~(3U << 11 | 3U << 13);
    // perhipal adres
    DMA1_Stream6->PAR = (uint32_t)&(USART2->DR);
}

void dmaUart2Tx(uint8_t *data, uint16_t size)
{
    if (data == 0 || size == 0)
    {
        return;
    }
    DMA1_Stream6->CR &= ~(1 << 0);
    while (DMA1_Stream6->CR & (1 << 0))
        ;
    DMA1->HIFCR = (1 << 21) | (1 << 20);
    DMA1_Stream6->M0AR = (uint32_t)data;
    DMA1_Stream6->NDTR = size;
    DMA1_Stream6->CR |= (1 << 0);
}

uint16_t intToStr(int32_t val, uint8_t *buffer)
{
    uint8_t temp[10];
    uint16_t i = 0, j = 0;

    if (val < 0)
    {
        buffer[j++] = '-';
        val = -val;
    }

    if (val == 0)
    {
        buffer[j++] = '0';
        return j;
    }

    while (val > 0)
    {
        temp[i++] = (val % 10) + '0';
        val /= 10;
    }

    while (i > 0)
    {
        buffer[j++] = temp[--i];
    }

    return j;
}
