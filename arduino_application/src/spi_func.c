#include <stdlib.h>
#include <string.h>
#include "usart.h"
#include "spi_func.h"

static const uint8_t spi0x2000[144] = {
    0x00, 0x22, 0x54, 0x5E, 0x5C, 0x52, 0x1E, 0x0A, 0x51, 0x99, 0x1A, 0xD3,
    0x27, 0x14, 0x6F, 0x7E, 0x4F, 0xD7, 0x5D, 0x14, 0x6B, 0xEB, 0x17, 0x5D,
    0x7C, 0xE7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x68, 0x00, 0x95, 0x22, 0x70, 0x72, 0x9C, 0xB6, 0xD0, 0xF7, 0xF8, 0x10,
    0x86, 0x78, 0x8D, 0xA2, 0xDB, 0x6D, 0x0F, 0x6F, 0x5C, 0xEE, 0x3F, 0xFD,
    0x76, 0x05, 0x65, 0xA3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const uint8_t spi0x6000[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x90, 0x00, 0x7F, 0xFE,
    0xC9, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0xFB, 0xFF, 0xD0, 0xFF,
    0xC0, 0xFF, 0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xA1, 0xA5, 0x67, 0x8A, 0x88, 0x7B, 0xD5, 0xE5, 0x59, 0x52, 0x18,
    0x7D, 0x07, 0xF6, 0x5D, 0xB8, 0x85, 0x61, 0xFF, 0x32, 0x32, 0x32, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x50, 0xFD, 0x00, 0x00,
    0xC6, 0x0F, 0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41,
    0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63, 0x0F, 0x30, 0x61, 0x96,
    0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33,
    0x36, 0x63, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF};

void spi_read(uint16_t addr, uint8_t len, uint8_t *buffer)
{
    uint8_t *data = NULL;
    switch (addr & 0xF000)
    {
    case 0x6000:
        data = (uint8_t *)&spi0x6000[addr & 0xFF];
        usart_send_str("Factory Configuration and Calibration");
        break;
    case 0x2000:
        data = (uint8_t *)&spi0x2000[addr & 0x7F];
        usart_send_str("Paring info");
        break;

    default:
        usart_send_str("Unknown spi area");
        break;
    }

    if (data)
        memcpy(buffer, data, len);
    else
        memset(buffer, 0xFF, len);
}

void spi_write(uint16_t addr, uint8_t len, uint8_t *buffer)
{
    usart_send_str("spi_write TODO");
}

void spi_erase(uint16_t addr, uint8_t len)
{
    usart_send_str("spi_erase TODO");
}

