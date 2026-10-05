#ifndef KPRINTF_H
#define KPRINTF_H

/* Supports %c %s %d %u %x %p %% with optional l/ll, zero-pad, and width,
 * e.g. "%016lx". Enough for register dumps; nothing more. */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
