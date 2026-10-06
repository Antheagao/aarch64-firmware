/*
 * The EL1 image's C entry point.
 *
 * It shares src/uart.c, src/kprintf.c and src/semihost.c with the firmware
 * rather than duplicating them: the PL011 is at the same address either side
 * of the hand-off, and a second copy of a driver is a second place for a bug.
 *
 * This image ends the QEMU run, because the eret that reached it is one-way:
 * EL3 cannot regain control to exit on its own.
 */
#include <stdint.h>

#include "kprintf.h"
#include "semihost.h"
#include "sysreg.h"
#include "uart.h"

void kernel_main(void) __attribute__((noreturn));

void kernel_main(void)
{
    /* The firmware already brought the UART up, but this image must not
     * depend on that: it is a separate binary that another loader could
     * enter with the UART untouched. */
    uart_init();

    /* EL1 cannot read SCR_EL3, so it reports only what it can actually
     * observe. The security state is reported by EL3, which set it. */
    kprintf("kernel: running at EL%u\n", current_el());

    semihost_exit(0);
}
