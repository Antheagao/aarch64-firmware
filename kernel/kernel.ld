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

    .text : {
        KEEP(*(.text.kstart))   /* kernel_start must be the first byte */
        *(.text .text.*)
    }

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(8) {
        *(.data .data.*)
    }

    /* NOLOAD keeps .bss out of the binary, so the image stays small and the
     * firmware copies only what it must. kernel_start zeroes it. */
    .bss (NOLOAD) : ALIGN(16) {
        __kbss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __kbss_end = .;
    }

    .stack (NOLOAD) : ALIGN(16) {
        . += 16K;
        __kstack_top = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
