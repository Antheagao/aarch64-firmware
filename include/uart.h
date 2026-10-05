#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);

/* Number of UART waits that timed out since uart_init(), counting each
 * character dropped after the first TX timeout. 0 means the UART is healthy. */
uint32_t uart_tx_timeouts(void);

#endif
