/* Polled PL011 UART driver. Register map from the Arm PrimeCell UART (PL011)
 * Technical Reference Manual, DDI 0183. */
#include <stdint.h>

#include "platform.h"
#include "uart.h"

#define UARTDR      0x00
#define UARTFR      0x18
#define UARTIBRD    0x24
#define UARTFBRD    0x28
#define UARTLCR_H   0x2C
#define UARTCR      0x30

#define FR_BUSY     (1U << 3)
#define FR_TXFF     (1U << 5)   /* transmit FIFO full */
#define LCR_H_FEN   (1U << 4)   /* enable FIFOs */
#define LCR_H_WLEN8 (3U << 5)   /* 8 data bits */
#define CR_UARTEN   (1U << 0)
#define CR_TXE      (1U << 8)
#define CR_RXE      (1U << 9)

#define BAUD        115200UL

static inline void reg_write(uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)(UART0_BASE + off) = val;
}

static inline uint32_t reg_read(uint32_t off)
{
    return *(volatile uint32_t *)(UART0_BASE + off);
}

void uart_init(void)
{
    /* Baud divisor is clk / (16 * baud) in 16.6 fixed point, so compute it
     * pre-multiplied by 64 and round: 24 MHz / 115200 -> 13 + 1/64. */
    uint32_t div = (uint32_t)((4 * UART0_CLK_HZ + BAUD / 2) / BAUD);

    reg_write(UARTCR, 0);                   /* disable while reprogramming */
    while (reg_read(UARTFR) & FR_BUSY)
        ;
    reg_write(UARTIBRD, div >> 6);
    reg_write(UARTFBRD, div & 0x3f);
    reg_write(UARTLCR_H, LCR_H_WLEN8 | LCR_H_FEN);   /* 8N1; also latches the divisor */
    reg_write(UARTCR, CR_UARTEN | CR_TXE | CR_RXE);
}

void uart_putc(char c)
{
    if (c == '\n')
        uart_putc('\r');
    while (reg_read(UARTFR) & FR_TXFF)
        ;
    reg_write(UARTDR, (unsigned char)c);
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
