#include <stdbool.h>
#include <stdint.h>

#include "kprintf.h"
#include "mmio.h"
#include "semihost.h"
#include "sysreg.h"
#include "sysreg_bits.h"
#include "trapframe.h"
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

/* Defined in src/vectors.S. Only its address is used. */
extern char vectors[];

/* boot.S puts the EL3 control registers into a known state before any C runs.
 * Report what actually landed in them: a register that reads back wrong is a
 * silent fault everywhere later, so it is checked once, here. */
static void report_el3_state(void)
{
    uint64_t vbar = read_sysreg(vbar_el3);
    uint64_t sctlr = read_sysreg(sctlr_el3);

    kprintf("  SCTLR_EL3 = 0x%016lx\n", sctlr);
    kprintf("  SCR_EL3   = 0x%016lx\n", read_sysreg(scr_el3));
    kprintf("  CPTR_EL3  = 0x%016lx\n", read_sysreg(cptr_el3));
    kprintf("  VBAR_EL3  = 0x%016lx\n", vbar);

    kprintf("el3: vector table installed: %s\n",
            vbar == (uint64_t)(uintptr_t)vectors ? "ok" : "FAIL");
    kprintf("el3: vector table 2 KiB aligned: %s\n", (vbar & VBAR_ALIGN_MASK) ? "FAIL" : "ok");
    kprintf("el3: alignment and stack checks on: %s\n",
            (sctlr & SCTLR_EL3_A) && (sctlr & SCTLR_EL3_SA) ? "ok" : "FAIL");
}

/* Take a deliberate exception. This proves the whole path at once: the
 * vector table is installed, the frame is saved and restored correctly, the
 * syndrome decodes, and the firmware resumes instead of hanging. */
static void selftest_brk(void)
{
    trap_expect_next();
    __asm__ volatile("brk #0");

    uint64_t esr = trap_last_esr();
    kprintf("selftest: brk #0 trapped: EC=0x%02x: %s\n", ESR_EC(esr),
            ESR_EC(esr) == ESR_EC_BRK ? "ok" : "FAIL");
}

/* First C code after reset; boot.S calls it on the primary CPU only. */
void fw_main(void)
{
    uart_init();

    kprintf("\naarch64-firmware: reset vector reached at EL%u\n", current_el());
    kprintf("  MIDR_EL1  = 0x%016lx\n", read_sysreg(midr_el1));
    kprintf("  MPIDR_EL1 = 0x%016lx\n", read_sysreg(mpidr_el1));

    report_el3_state();
    selftest_poll_timeout();
    selftest_brk();

    /* Milestones 1-8 in docs/ROADMAP.md grow from here. */

    kprintf("uart: tx timeouts = %u\n", uart_tx_timeouts());
    kprintf("milestone 0: boot OK\n");
    semihost_exit(0);
}
