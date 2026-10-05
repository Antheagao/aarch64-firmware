/* Polled PL011 UART driver. Register map from the Arm PrimeCell UART (PL011)
 * Technical Reference Manual, DDI 0183. */
#include <stdbool.h>
#include <stdint.h>

#include "mmio.h"
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

/* Upper bound on status-register reads per wait. One character at 115200
 * baud takes about 87 us to shift out, so a healthy UART clears TXFF or BUSY
 * long before a million reads; running out means the device is stuck. */
#define UART_POLL_SPINS 1000000U

static uint32_t tx_timeouts;   /* waits that timed out, plus chars dropped after */
static bool tx_dead;           /* set by the first TX timeout; stays set */

static inline void reg_write(uint32_t off, uint32_t val)
{
    mmio_write32(UART0_BASE + off, val);
}

void uart_init(void)
{
    /* Baud divisor is clk / (16 * baud) in 16.6 fixed point, so compute it
     * pre-multiplied by 64 and round: 24 MHz / 115200 -> 13 + 1/64. */
    uint32_t div = (uint32_t)((4 * UART0_CLK_HZ + BAUD / 2) / BAUD);

    tx_timeouts = 0;
    tx_dead = false;

    reg_write(UARTCR, 0);                   /* disable while reprogramming */
    /* The TRM says to wait for the current character to finish before
     * reprogramming. If BUSY never clears, carry on: a garbled first
     * character is better than a firmware that never boots. */
    if (!mmio_poll_clear32(UART0_BASE + UARTFR, FR_BUSY, UART_POLL_SPINS))
        tx_timeouts++;
    reg_write(UARTIBRD, div >> 6);
    reg_write(UARTFBRD, div & 0x3f);
    reg_write(UARTLCR_H, LCR_H_WLEN8 | LCR_H_FEN);   /* 8N1; also latches the divisor */
    reg_write(UARTCR, CR_UARTEN | CR_TXE | CR_RXE);
}

/* Drop the character rather than hang if the FIFO never drains. After the
 * first timeout, drop everything immediately: a stuck UART would otherwise
 * cost a full spin budget for every remaining character of the boot log. */
static void uart_tx(char c)
{
    if (tx_dead || !mmio_poll_clear32(UART0_BASE + UARTFR, FR_TXFF, UART_POLL_SPINS)) {
        tx_dead = true;
        tx_timeouts++;
        return;
    }
    reg_write(UARTDR, (unsigned char)c);
}

void uart_putc(char c)
{
    if (c == '\n')
        uart_tx('\r');
    uart_tx(c);
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

uint32_t uart_tx_timeouts(void)
{
    return tx_timeouts;
}
