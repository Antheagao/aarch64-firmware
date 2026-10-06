/*
 * EL1 image entry point.
 *
 * The firmware copies this image to DRAM and (from the next queue row) erets
 * to its first byte. Nothing here may assume the firmware left any state
 * behind beyond the exception level itself.
 */

    .section .text.kstart, "ax"
    .global kernel_start
kernel_start:
    ldr     x0, =__kstack_top
    mov     sp, x0

    /* Zero .bss: it is NOLOAD, so it was never part of the copied image. */
    ldr     x0, =__kbss_start
    ldr     x1, =__kbss_end
1:  cmp     x0, x1
    b.hs    2f
    str     xzr, [x0], #8
    b       1b

2:  bl      kernel_main         // does not return

park:
    wfe
    b       park
