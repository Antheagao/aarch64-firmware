/*
 * EL3 exception vector table and the save/restore around the C handler.
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
 * An entry has only 0x80 bytes, so it saves the little it needs and branches
 * to the common path: it pushes a frame, records x0 and x1, puts the vector
 * index where the handler can name the slot, and leaves the rest to
 * el3_trap_common.
 */

#include "trapframe.h"

    .section .text.vectors, "ax"

    .macro vector_entry index, name
    .balign 0x80
\name:
    sub     sp, sp, #TF_SIZE
    stp     x0, x1, [sp, #TF_X0]        // free two registers to work with
    mov     x0, #\index
    b       el3_trap_common
    .endm

    .balign 0x800
    .global vectors
vectors:
    /* Current EL with SP_EL0. Unreachable here: EL3 runs on SP_EL3. */
    vector_entry 0, sync_cur_sp0
    vector_entry 1, irq_cur_sp0
    vector_entry 2, fiq_cur_sp0
    vector_entry 3, serror_cur_sp0

    /* Current EL with SP_ELx: where an EL3 bug lands. */
    vector_entry 4, sync_cur_spx
    vector_entry 5, irq_cur_spx
    vector_entry 6, fiq_cur_spx
    vector_entry 7, serror_cur_spx

    /* Lower EL in AArch64: SMC and traps from EL1 once M3 hands off. */
    vector_entry 8, sync_lower_a64
    vector_entry 9, irq_lower_a64
    vector_entry 10, fiq_lower_a64
    vector_entry 11, serror_lower_a64

    /* Lower EL in AArch32: never taken, because SCR_EL3.RW is 1. */
    vector_entry 12, sync_lower_a32
    vector_entry 13, irq_lower_a32
    vector_entry 14, fiq_lower_a32
    vector_entry 15, serror_lower_a32

    /* Past the end of the 2 KiB table, so this is never a vector target. */
    .balign 0x80
el3_trap_common:
    /* x0 holds the vector index and x1 is free; both originals are already
     * in the frame. Save the rest before using any of them as scratch. */
    stp     x2, x3,   [sp, #16 * 1]
    stp     x4, x5,   [sp, #16 * 2]
    stp     x6, x7,   [sp, #16 * 3]
    stp     x8, x9,   [sp, #16 * 4]
    stp     x10, x11, [sp, #16 * 5]
    stp     x12, x13, [sp, #16 * 6]
    stp     x14, x15, [sp, #16 * 7]
    stp     x16, x17, [sp, #16 * 8]
    stp     x18, x19, [sp, #16 * 9]
    stp     x20, x21, [sp, #16 * 10]
    stp     x22, x23, [sp, #16 * 11]
    stp     x24, x25, [sp, #16 * 12]
    stp     x26, x27, [sp, #16 * 13]
    stp     x28, x29, [sp, #16 * 14]
    str     x30,      [sp, #TF_X30]     // the C call below overwrites it
    str     x0,       [sp, #TF_VECTOR]

    /* The syndrome registers say why we are here, and are read before any
     * C runs so nothing can disturb them. */
    mrs     x1, elr_el3
    mrs     x2, spsr_el3
    stp     x1, x2,   [sp, #TF_ELR]
    mrs     x1, esr_el3
    mrs     x2, far_el3
    stp     x1, x2,   [sp, #TF_ESR]

    mov     x0, sp                      // the frame is the handler's argument
    bl      el3_trap

    /* Restore ELR and SPSR first, while x1 and x2 are still scratch: the
     * handler may have advanced ELR to step over a faulting instruction. */
    ldp     x1, x2,   [sp, #TF_ELR]
    msr     elr_el3, x1
    msr     spsr_el3, x2

    ldp     x0, x1,   [sp, #16 * 0]
    ldp     x2, x3,   [sp, #16 * 1]
    ldp     x4, x5,   [sp, #16 * 2]
    ldp     x6, x7,   [sp, #16 * 3]
    ldp     x8, x9,   [sp, #16 * 4]
    ldp     x10, x11, [sp, #16 * 5]
    ldp     x12, x13, [sp, #16 * 6]
    ldp     x14, x15, [sp, #16 * 7]
    ldp     x16, x17, [sp, #16 * 8]
    ldp     x18, x19, [sp, #16 * 9]
    ldp     x20, x21, [sp, #16 * 10]
    ldp     x22, x23, [sp, #16 * 11]
    ldp     x24, x25, [sp, #16 * 12]
    ldp     x26, x27, [sp, #16 * 13]
    ldp     x28, x29, [sp, #16 * 14]
    ldr     x30,      [sp, #TF_X30]
    add     sp, sp, #TF_SIZE
    eret
