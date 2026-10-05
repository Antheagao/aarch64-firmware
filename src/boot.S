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

    .section .text.boot, "ax"
    .global _start
_start:
    /* Only the primary CPU (affinity 0.0.0) boots. Secondaries park here
     * until milestone 6 releases them with PSCI CPU_ON. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xffffff           // Aff2.Aff1.Aff0
    cbnz    x0, park

    /* TODO(milestone 1): set VBAR_EL3 and put SCTLR_EL3/SCR_EL3/CPTR_EL3 in
     * a known state. Until then VBAR_EL3 is 0 (QEMU's reset value), so a
     * synchronous exception "returns" to 0x200 -- the middle of kprintf. */

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
