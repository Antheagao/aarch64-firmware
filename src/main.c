#include <stdbool.h>
#include <stdint.h>

#include "kprintf.h"
#include "mmio.h"
#include "semihost.h"
#include "sysreg.h"
#include "uart.h"

#define SELFTEST_POLL_SPINS 1000U

void fw_main(void) __attribute__((noreturn));

/* QEMU's PL011 never reports BUSY or a full FIFO, so the UART cannot show
 * that its waits are bounded. Poll a word that never clears instead: the
 * helper must give up after its budget rather than spin forever. */
static void selftest_poll_timeout(void)
{
    static volatile uint32_t stuck = 1;
    static volatile uint32_t clear = 0;
    bool timed_out = !mmio_poll_clear32((uintptr_t)&stuck, 1, SELFTEST_POLL_SPINS);
    bool cleared = mmio_poll_clear32((uintptr_t)&clear, 1, SELFTEST_POLL_SPINS);

    kprintf("selftest: poll gives up on a stuck bit after %u spins: %s\n", SELFTEST_POLL_SPINS,
            timed_out ? "ok" : "FAIL");
    kprintf("selftest: poll returns at once on a clear bit: %s\n", cleared ? "ok" : "FAIL");
}

/* First C code after reset; boot.S calls it on the primary CPU only. */
void fw_main(void)
{
    uart_init();

    kprintf("\naarch64-firmware: reset vector reached at EL%u\n", current_el());
    kprintf("  MIDR_EL1  = 0x%016lx\n", read_sysreg(midr_el1));
    kprintf("  MPIDR_EL1 = 0x%016lx\n", read_sysreg(mpidr_el1));

    selftest_poll_timeout();

    /* Milestones 1-8 in docs/ROADMAP.md grow from here. */

    kprintf("uart: tx timeouts = %u\n", uart_tx_timeouts());
    kprintf("milestone 0: boot OK\n");
    semihost_exit(0);
}
