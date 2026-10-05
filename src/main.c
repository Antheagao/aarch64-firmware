#include "kprintf.h"
#include "semihost.h"
#include "sysreg.h"
#include "uart.h"

void fw_main(void) __attribute__((noreturn));

/* First C code after reset; boot.S calls it on the primary CPU only. */
void fw_main(void)
{
    uart_init();

    kprintf("\naarch64-firmware: reset vector reached at EL%u\n", current_el());
    kprintf("  MIDR_EL1  = 0x%016lx\n", read_sysreg(midr_el1));
    kprintf("  MPIDR_EL1 = 0x%016lx\n", read_sysreg(mpidr_el1));

    /* Milestones 1-8 in docs/ROADMAP.md grow from here. */

    kprintf("milestone 0: boot OK\n");
    semihost_exit(0);
}
