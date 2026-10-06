/*
 * The EL1 image's C entry point.
 *
 * It shares src/uart.c and src/kprintf.c with the firmware rather than
 * duplicating them: the PL011 is at the same address either side of the
 * hand-off, and a second copy of a driver is a second place for a bug.
 *
 * Nothing here runs yet. The firmware loads this image but does not enter
 * it until the next queue row configures the drop to EL1, which keeps a
 * failure in the load distinguishable from a failure in the hand-off.
 */
#include <stdint.h>

#include "kprintf.h"
#include "sysreg.h"
#include "uart.h"

void kernel_main(void) __attribute__((noreturn));

void kernel_main(void)
{
    /* The firmware already brought the UART up, but this image must not
     * depend on that: it is a separate binary that could be entered from a
     * different loader. */
    uart_init();

    kprintf("kernel: running at EL%u\n", current_el());

    for (;;)
        __asm__ volatile("wfe");
}
