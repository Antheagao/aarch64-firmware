/*
 * EL3 exception vector table.
 *
 * Arm ARM (DDI 0487), "Exception vectors": VBAR_EL3 points at a 2 KiB-aligned
 * table of 16 entries of 0x80 bytes each, in four groups of four:
 *
 *   +0x000  current EL with SP_EL0
 *   +0x200  current EL with SP_ELx
 *   +0x400  lower EL in AArch64
 *   +0x600  lower EL in AArch32
 *
 * Each group holds synchronous, IRQ, FIQ and SError entries in that order.
 * The CPU jumps to the entry, not through it, so the table is code rather
 * than a list of addresses.
 *
 * Every entry parks for now. There is no trap frame yet, so nothing can be
 * printed safely; parking contains the fault and leaves it visible in the
 * debugger instead of letting it run on into unrelated code. Queue row 2
 * replaces the stub with a trap frame and an ESR_EL3 decode.
 */

    .section .text.vectors, "ax"

    /* One vector slot: aligned to its 0x80 stride and named so a
     * disassembly says which exception was taken. */
    .macro vector_entry name
    .balign 0x80
\name:
    b       el3_park
    .endm

    .balign 0x800
    .global vectors
vectors:
    /* Current EL with SP_EL0. Unreachable here: EL3 runs on SP_EL3. */
    vector_entry sync_cur_sp0
    vector_entry irq_cur_sp0
    vector_entry fiq_cur_sp0
    vector_entry serror_cur_sp0

    /* Current EL with SP_ELx: where an EL3 bug lands. */
    vector_entry sync_cur_spx
    vector_entry irq_cur_spx
    vector_entry fiq_cur_spx
    vector_entry serror_cur_spx

    /* Lower EL in AArch64: SMC and traps from EL1 once M3 hands off. */
    vector_entry sync_lower_a64
    vector_entry irq_lower_a64
    vector_entry fiq_lower_a64
    vector_entry serror_lower_a64

    /* Lower EL in AArch32: never taken, because SCR_EL3.RW is 1. */
    vector_entry sync_lower_a32
    vector_entry irq_lower_a32
    vector_entry fiq_lower_a32
    vector_entry serror_lower_a32

    .global el3_park
el3_park:
    wfe
    b       el3_park
