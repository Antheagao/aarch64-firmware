/*
 * The EL1 image, embedded in the firmware binary.
 *
 * This is what a boot ROM or TF-A BL2 does with the next stage: carry it,
 * then place it where that stage can actually run. Non-secure EL1 cannot
 * fetch from secure flash, so the firmware copies this into DRAM at boot.
 */

    .section .rodata.kernel_image, "a"
    .balign 8
    .global kernel_image_start
kernel_image_start:
    .incbin "build/kernel.bin"
    /* The copy moves 8 bytes at a time, so the end is padded to match. */
    .balign 8
    .global kernel_image_end
kernel_image_end:
