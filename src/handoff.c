/*
 * The EL3 to EL1 hand-off: configure the state EL1 inherits, then eret.
 *
 * Ordering is the whole difficulty here. On a CPU with Secure EL2
 * (FEAT_SEL2, which QEMU's -cpu max implements) the EL1 and EL2 registers
 * are banked by security state, and which bank EL3 sees is selected by
 * SCR_EL3.NS. So SCR_EL3.NS is set *first*, and only then are HCR_EL2 and
 * SCTLR_EL1 written, because those writes must land in the Non-secure bank
 * that EL1 will actually run with. Writing them first and flipping NS
 * afterwards configures the Secure copies and leaves the Non-secure ones
 * UNKNOWN, which is the bug this comment exists to prevent.
 */
#include <stdint.h>

#include "cpuid.h"
#include "handoff.h"
#include "kprintf.h"
#include "sysreg.h"
#include "sysreg_bits.h"

/* Defined in src/boot.S: loads ELR_EL3 and erets. Separate because eret
 * cannot be expressed in C. */
void el3_eret_to(uint64_t entry) __attribute__((noreturn));

void el3_enter_el1(uint64_t entry, const struct cpu_id *id)
{
    /* Non-secure from here on, as far as the lower exception levels are
     * concerned. Execution stays at EL3 and stays Secure; what changes is
     * which bank of the EL1 and EL2 registers these writes reach. */
    write_sysreg(scr_el3, SCR_EL3_RES1 | SCR_EL3_RW | SCR_EL3_NS);
    isb(); /* the banked view must switch before the writes below */

    /* EL2 is implemented on this machine even though the firmware skips it,
     * and HCR_EL2.RW still decides whether Non-secure EL1 is AArch64. Ask
     * the ID registers rather than assuming: writing HCR_EL2 on a CPU
     * without EL2 is not architecturally valid. */
    if (cpuid_field(id->pfr0, 8, 4) != 0)
        write_sysreg(hcr_el2, HCR_EL2_RW);

    /* Same argument as SCTLR_EL3 at reset: these bits are UNKNOWN out of
     * reset, so EL1 is handed a known state. MMU and caches stay off, and
     * alignment checking stays on, which matches the -mstrict-align build. */
    write_sysreg(sctlr_el1, SCTLR_EL1_RES1 | SCTLR_EL1_A | SCTLR_EL1_SA);

    /* EL1h: EL1 with its own stack pointer, entered with all four of DAIF
     * masked so the kernel starts with interrupts off and decides for
     * itself when to take them. */
    write_sysreg(spsr_el3, SPSR_EL3_M_EL1H | SPSR_EL3_D | SPSR_EL3_A | SPSR_EL3_I | SPSR_EL3_F);
    isb();

    /* EL3 is the only level that can know the security state it configured,
     * so it is the level that reports it. EL1 cannot read SCR_EL3. */
    kprintf("el3: entering EL1 (Non-secure) at %p\n", (void *)entry);

    el3_eret_to(entry);
}
