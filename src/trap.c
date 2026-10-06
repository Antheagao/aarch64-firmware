/* Exception reporting. The point of this file is that a crash is readable:
 * which vector fired, why, where, and with what register state.
 *
 * Built into both images, with TRAP_EL saying which exception level this
 * copy serves. The decode is identical either side; only the level printed
 * and the banked registers read (in src/vectors.S) differ. */
#ifndef TRAP_EL
#error "TRAP_EL must be defined as 1 or 3"
#endif
#include <stdbool.h>
#include <stdint.h>

#include "kprintf.h"
#include "trapframe.h"

static const char *const VECTOR_NAMES[16] = {
    "sync_cur_sp0",   "irq_cur_sp0",   "fiq_cur_sp0",   "serror_cur_sp0",
    "sync_cur_spx",   "irq_cur_spx",   "fiq_cur_spx",   "serror_cur_spx",
    "sync_lower_a64", "irq_lower_a64", "fiq_lower_a64", "serror_lower_a64",
    "sync_lower_a32", "irq_lower_a32", "fiq_lower_a32", "serror_lower_a32",
};

struct code_name {
    uint32_t code;
    const char *name;
};

/* Arm ARM (DDI 0487), "ESR_ELx, Exception Syndrome Register", EC encodings.
 * Only the classes this firmware can currently raise or route are listed; an
 * unlisted class still prints its raw EC. */
static const struct code_name EC_NAMES[] = {
    {0x00, "unknown reason"},
    {0x07, "SVE, SIMD or FP access trapped"},
    {0x0e, "illegal execution state"},
    {0x15, "SVC"},
    {0x16, "HVC"},
    {0x17, "SMC"},
    {0x18, "trapped MSR or MRS"},
    {0x20, "instruction abort, lower EL"},
    {0x21, "instruction abort, same EL"},
    {0x22, "PC alignment fault"},
    {0x24, "data abort, lower EL"},
    {0x25, "data abort, same EL"},
    {0x26, "SP alignment fault"},
    {0x2f, "SError"},
    {0x30, "breakpoint, lower EL"},
    {0x31, "breakpoint, same EL"},
    {0x3c, "BRK instruction"},
};

/* Arm ARM (DDI 0487), ISS encoding for an exception from a Data Abort, DFSC. */
static const struct code_name DFSC_NAMES[] = {
    {0x04, "translation fault, level 0"}, {0x05, "translation fault, level 1"},
    {0x06, "translation fault, level 2"}, {0x07, "translation fault, level 3"},
    {0x09, "access flag fault, level 1"}, {0x0a, "access flag fault, level 2"},
    {0x0b, "access flag fault, level 3"}, {0x0d, "permission fault, level 1"},
    {0x0e, "permission fault, level 2"},  {0x0f, "permission fault, level 3"},
    {0x10, "synchronous external abort"}, {0x21, "alignment fault"},
};

static const char *lookup(const struct code_name *table, unsigned n, uint32_t code)
{
    for (unsigned i = 0; i < n; i++) {
        if (table[i].code == code)
            return table[i].name;
    }
    return "unnamed";
}

#define LOOKUP(table, code) lookup((table), sizeof(table) / sizeof((table)[0]), (code))

/* Set by trap_expect_next() before a deliberate fault. volatile because the
 * handler runs between the store here and the load after the faulting
 * instruction, and the compiler cannot see that control flow. */
static volatile bool expect_trap;
static volatile uint64_t last_expected_esr;

void trap_expect_next(void)
{
    expect_trap = true;
}

uint64_t trap_last_esr(void)
{
    return last_expected_esr;
}

static void report(const struct trap_frame *tf)
{
    uint32_t ec = ESR_EC(tf->esr);
    uint32_t iss = ESR_ISS(tf->esr);

    /* The level is printed because both images report through this code: a
     * fault at EL1 reported by EL3 means it went to the wrong level, which
     * is a real failure rather than a cosmetic one. */
    kprintf("trap: EL%u vector=%u (%s) EC=0x%02x (%s) IL=%u ISS=0x%07x\n", TRAP_EL,
            (unsigned)tf->vector, VECTOR_NAMES[tf->vector & 0xf], ec, LOOKUP(EC_NAMES, ec),
            ESR_IL(tf->esr), iss);
    kprintf("trap:   ESR=0x%016lx ELR=0x%016lx SPSR=0x%016lx\n", tf->esr, tf->elr, tf->spsr);

    /* FAR only means anything for aborts, so it is not printed otherwise:
     * a stale address beside an unrelated fault sends people the wrong way. */
    if (ec == 0x20 || ec == 0x21 || ec == 0x24 || ec == ESR_EC_DATA_ABORT) {
        kprintf("trap:   FAR=0x%016lx DFSC=0x%02x (%s)\n", tf->far, ESR_DFSC(iss),
                LOOKUP(DFSC_NAMES, ESR_DFSC(iss)));
    }

    for (unsigned i = 0; i < 31; i += 4) {
        kprintf("trap:   ");
        for (unsigned j = i; j < i + 4 && j < 31; j++)
            kprintf("x%2u=0x%016lx ", j, tf->x[j]);
        kprintf("\n");
    }
}

void trap_handler(struct trap_frame *tf)
{
    report(tf);

#if TRAP_EL == 3
    /* A synchronous exception from a lower EL is a request, not a fault:
     * EL1 called down on purpose, and parking it would hang the run. Step
     * over the SMC and return, which resumes EL1 at the next instruction.
     *
     * Plumbing only. The SMCCC argument convention and a real function
     * table belong to M6, with PSCI. */
    if (tf->vector == VECTOR_SYNC_LOWER_A64 && ESR_EC(tf->esr) == ESR_EC_SMC) {
        /* ELR is NOT advanced here, unlike the self-tests below. Arm ARM
         * (DDI 0487), "Exception return": for an exception taken from SVC,
         * HVC or SMC, ELR already holds the address *after* the instruction,
         * because the call completed. BRK is the opposite: it is a debug
         * exception and ELR points at the BRK itself, so stepping over it
         * needs the +4. Adding 4 here skips a real instruction, and EL1
         * resumes mid-sequence. */
        kprintf("trap: SMC from a lower EL handled, returning\n");
        return;
    }
#endif

    if (expect_trap) {
        expect_trap = false;
        last_expected_esr = tf->esr;
        if (ESR_EC(tf->esr) == ESR_EC_INSN_ABORT) {
            /* An instruction abort cannot be stepped over: ELR points at the
             * address that could not be fetched, so ELR+4 is still inside
             * memory this CPU may not execute and the fault just repeats.
             * The faulting fetch was a call, so x30 holds the address after
             * it, and returning there makes a failed call behave like one
             * that returned. */
            tf->elr = tf->x[30];
        } else {
            /* A64 instructions are 4 bytes, and ELR points at the one that
             * faulted, so stepping over it resumes after the deliberate
             * fault. */
            tf->elr += 4;
        }
        kprintf("trap: expected, stepping over it\n");
        return;
    }

    kprintf("trap: unhandled, parking\n");
    for (;;)
        __asm__ volatile("wfe");
}
