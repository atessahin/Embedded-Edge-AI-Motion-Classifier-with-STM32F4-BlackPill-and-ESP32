#ifndef communication_H
#define communication_H
#include <stdint.h>
void spiConfig();
uint8_t SPI1_Transfer(uint8_t data);
uint8_t spiRead(uint8_t reg);
void spiWrite(uint8_t reg, uint8_t value);
void dmaUart2Tx(uint8_t *data, uint16_t size);
void dmaConfig();
void uartConfig();
uint16_t intToStr(int32_t val, uint8_t *buffer);
void uart2SendBlocking(uint8_t *data, uint16_t size);
#endif /* communication_H */