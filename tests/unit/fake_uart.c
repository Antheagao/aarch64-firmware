/*
 * The seam that makes kprintf testable: it writes through uart_putc, so the
 * unit tests link the real src/kprintf.c against this capturing UART instead
 * of the PL011 driver. kprintf.c itself is compiled unchanged, which is the
 * point: the code under test is the code that ships.
 */
#include <stddef.h>
#include <stdint.h>

#include "fake_uart.h"
#include "uart.h"

#define CAPTURE_MAX 4096

static char captured[CAPTURE_MAX];
static size_t captured_len;

void fake_uart_reset(void)
{
    captured_len = 0;
    captured[0] = '\0';
}

const char *fake_uart_text(void)
{
    return captured;
}

void uart_init(void)
{
    fake_uart_reset();
}

void uart_putc(char c)
{
    if (captured_len + 1 < CAPTURE_MAX)
        captured[captured_len++] = c;
    captured[captured_len] = '\0';
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

uint32_t uart_tx_timeouts(void)
{
    return 0;
}
