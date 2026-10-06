#include <stdarg.h>
#include <stdint.h>

#include "kprintf.h"
#include "uart.h"

static void put_uint(uint64_t val, unsigned base, int width, char pad)
{
    char buf[20]; /* UINT64_MAX is 20 decimal digits */
    int n = 0;

    do {
        unsigned digit = val % base;
        buf[n++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        val /= base;
    } while (val);

    while (width-- > n)
        uart_putc(pad);
    while (n--)
        uart_putc(buf[n]);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            uart_putc(*fmt);
            continue;
        }
        if (!*++fmt)
            break;

        char pad = ' ';
        int width = 0;
        int is_long = 0;

        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l') { /* l and ll are both 64-bit on AArch64 */
            is_long = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            uart_putc((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            uart_puts(s ? s : "(null)");
            break;
        }
        case 'd': {
            int64_t v = is_long ? va_arg(ap, long) : va_arg(ap, int);
            if (v < 0) {
                uart_putc('-');
                put_uint(0 - (uint64_t)v, 10, width - 1, pad);
            } else {
                put_uint((uint64_t)v, 10, width, pad);
            }
            break;
        }
        case 'u':
            put_uint(is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10, width, pad);
            break;
        case 'x':
            put_uint(is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, width, pad);
            break;
        case 'p':
            uart_puts("0x");
            put_uint((uintptr_t)va_arg(ap, void *), 16, 16, '0');
            break;
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }
    }
    va_end(ap);
}
