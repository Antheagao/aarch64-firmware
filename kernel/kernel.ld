/*
 * The EL1 image: one flat blob in non-secure DRAM.
 *
 * Unlike the firmware, this has no flash/SRAM split. The firmware copies the
 * whole image to DRAM before entering it, so load address and run address
 * are the same and .data needs no relocation.
 */
ENTRY(kernel_start)

SECTIONS
{
    . = 0x40000000;             /* DRAM_BASE: non-secure RAM on QEMU virt */

    /* Every section starts on a page boundary, because the MMU's smallest
     * unit of permission is a page: .text cannot be execute-only if it
     * shares a page with .data. */
    .text : ALIGN(4096) {
        __ktext_start = .;
        KEEP(*(.text.kstart))   /* kernel_start must be the first byte */
        *(.text .text.*)
        . = ALIGN(4096);
        __ktext_end = .;
    }

    .rodata : ALIGN(4096) {
        __krodata_start = .;
        *(.rodata .rodata.*)
        . = ALIGN(4096);
        __krodata_end = .;
    }

    .data : ALIGN(4096) {
        __kdata_start = .;
        *(.data .data.*)
        . = ALIGN(4096);
    }

    /* NOLOAD keeps .bss out of the binary, so the image stays small and the
     * firmware copies only what it must. kernel_start zeroes it. */
    .bss (NOLOAD) : ALIGN(4096) {
        __kbss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __kbss_end = .;
    }

    .stack (NOLOAD) : ALIGN(4096) {
        . += 16K;
        __kstack_top = .;
        . = ALIGN(4096);
        __kend = .;             /* one past everything the kernel owns */
    }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
