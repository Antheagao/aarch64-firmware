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
#include "mmu.h"
#include "semihost.h"
#include "sysreg.h"
#include "sysreg_bits.h"
#include "trapframe.h"
#include "uart.h"

void kernel_main(void) __attribute__((noreturn));

/* Defined in src/vectors.S, assembled for this image with TRAP_EL=1. */
extern char vectors[];

/* Fault on purpose at EL1, to prove EL1 handles its own exceptions. If this
 * were reported by EL3 the trap went to the wrong level: a lower-EL fault
 * only reaches EL3 when EL1 has not claimed it. */
static void selftest_el1_brk(void)
{
    trap_expect_next();
    __asm__ volatile("brk #0");

    uint64_t esr = trap_last_esr();
    kprintf("kernel: brk #0 trapped at EL1: EC=0x%02x: %s\n", ESR_EC(esr),
            ESR_EC(esr) == ESR_EC_BRK ? "ok" : "FAIL");
}

/* Call down to EL3 on purpose. An smc from EL1 is a request, not a fault:
 * it lands in EL3's lower-EL AArch64 synchronous vector, and EL3 is expected
 * to return control here rather than park. */
static void selftest_smc(void)
{
    kprintf("kernel: calling smc #0\n");
    __asm__ volatile("smc #0" ::: "memory");
    kprintf("kernel: returned from smc: ok\n");
}

/* The permissions are only real if the hardware refuses the write. */
static const volatile uint64_t rodata_value = 0x1234;

static void selftest_rodata_write(void)
{
    /* const is cast away on purpose: the point is that the MMU stops this,
     * not the compiler. volatile keeps the store from being optimised out. */
    volatile uint64_t *writable = (volatile uint64_t *)(uintptr_t)&rodata_value;

    trap_expect_next();
    *writable = 0xdead;

    uint64_t esr = trap_last_esr();
    bool ok = ESR_EC(esr) == ESR_EC_DATA_ABORT && ESR_DFSC(ESR_ISS(esr)) == ESR_DFSC_PERM_L3;
    kprintf("kernel: rodata write trapped: EC=0x%02x DFSC=0x%02x: %s\n", ESR_EC(esr),
            ESR_DFSC(ESR_ISS(esr)), ok ? "ok" : "FAIL");
}

/* In .data, so it is mapped writable and execute-never. The contents are a
 * NOP, which would run perfectly well if the page allowed it: the fault is
 * the mapping's doing, not the data's. */
static uint64_t data_code[2] = {0xd503201fUL, 0xd65f03c0UL}; /* nop; ret */

static void selftest_execute_data(void)
{
    void (*volatile call_into_data)(void) = (void (*)(void))(uintptr_t)data_code;

    trap_expect_next();
    call_into_data();

    uint64_t esr = trap_last_esr();
    kprintf("kernel: execute from data trapped: EC=0x%02x: %s\n", ESR_EC(esr),
            ESR_EC(esr) == ESR_EC_INSN_ABORT ? "ok" : "FAIL");
}

void kernel_main(void)
{
    /* The firmware already brought the UART up, but this image must not
     * depend on that: it is a separate binary that another loader could
     * enter with the UART untouched. */
    uart_init();

    /* EL1 cannot read SCR_EL3, so it reports only what it can actually
     * observe. The security state is reported by EL3, which set it. */
    kprintf("kernel: running at EL%u\n", current_el());

    /* EL1 gets its own vectors before anything can fault. VBAR_EL1 has the
     * same 2 KiB alignment rule as VBAR_EL3. */
    write_sysreg(vbar_el1, (uint64_t)(uintptr_t)vectors);
    isb();
    kprintf("kernel: vector table installed: %s\n",
            (read_sysreg(vbar_el1) == (uint64_t)(uintptr_t)vectors) ? "ok" : "FAIL");

    /* The MMU goes on before the self-tests, so everything after this point
     * is running translated: the faults they raise are proof the exception
     * path survives translation, not just that it worked beforehand. */
    if (!mmu_enable())
        semihost_exit(1);

    selftest_el1_brk();
    selftest_rodata_write();
    selftest_execute_data();
    selftest_smc();

    semihost_exit(0);
}
