/*
 * Reset vector.
 *
 * QEMU (-M virt,secure=on with -bios) starts *every* CPU here: EL3, MMU and
 * caches off, executing straight out of secure flash. That is the same state a
 * real SoC's boot ROM inherits from hardware reset, and the same place Trusted
 * Firmware-A's BL1 starts.
 *
 * Flash is read-only, so before any C runs this code has to give it a stack,
 * copy .data into SRAM, and zero .bss.
 */

#include "sysreg_bits.h"

    .section .text.boot, "ax"
    .global _start
_start:
    /* Only the primary CPU (affinity 0.0.0) boots. Secondaries park here
     * until milestone 6 releases them with PSCI CPU_ON. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xffffff           // Aff2.Aff1.Aff0
    cbnz    x0, park

    /* Hardware resets many EL3 control bits to UNKNOWN values, so firmware
     * writes them before anything can depend on them. This happens first:
     * until VBAR_EL3 is set it holds its reset value, and a fault would
     * vector into whatever code happens to sit at offset 0x200. */

    /* SCTLR_EL3: MMU, caches and icache stay off; A and SA turn on alignment
     * and stack-alignment checking so a misaligned access faults where it
     * happens rather than reading neighbouring bytes. The RES1 bits must be
     * written as 1. Arm ARM (DDI 0487), SCTLR_EL3. */
    ldr     x0, =(SCTLR_EL3_RES1 | SCTLR_EL3_A | SCTLR_EL3_SA)
    msr     sctlr_el3, x0
    isb                                 // the checks apply from the next instruction

    /* SCR_EL3.RW = 1: the next lower EL is AArch64. NS stays 0, so lower ELs
     * are Secure until M3 hands off to a Non-secure EL1. Interrupt routing
     * (IRQ, FIQ, EA) is M5's, and is left at 0 deliberately rather than by
     * omission. Arm ARM (DDI 0487), SCR_EL3. */
    ldr     x0, =(SCR_EL3_RES1 | SCR_EL3_RW)
    msr     scr_el3, x0

    /* CPTR_EL3.TFP = 1 traps FP and Advanced SIMD at EL3. The build is
     * -mgeneral-regs-only, so nothing should use them; trapping turns a
     * mistake into a reported exception instead of UNKNOWN behavior. EZ
     * stays 0, which traps SVE. M7 opens both. Arm ARM (DDI 0487), CPTR_EL3. */
    ldr     x0, =CPTR_EL3_TFP
    msr     cptr_el3, x0

    /* Install the vector table. Arm ARM (DDI 0487), "Exception vectors". */
    ldr     x0, =vectors
    msr     vbar_el3, x0
    isb                                 // exceptions use the new base from here

    ldr     x0, =__stack_top
    mov     sp, x0

    /* .data: copy from its load address in flash to its run address in SRAM. */
    ldr     x0, =__data_start
    ldr     x1, =__data_end
    ldr     x2, =__data_load
1:  cmp     x0, x1
    b.hs    2f
    ldr     x3, [x2], #8
    str     x3, [x0], #8
    b       1b

    /* .bss: zero it, because C assumes it starts zeroed. */
2:  ldr     x0, =__bss_start
    ldr     x1, =__bss_end
3:  cmp     x0, x1
    b.hs    4f
    str     xzr, [x0], #8
    b       3b

4:  bl      fw_main                     // does not return

park:
    wfe
    b       park
