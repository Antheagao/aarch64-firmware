/* Test double for the UART, so kprintf can be exercised on the host. */
#ifndef FAKE_UART_H
#define FAKE_UART_H

void fake_uart_reset(void);

/* Everything written since the last reset, NUL-terminated. */
const char *fake_uart_text(void);

#endif
