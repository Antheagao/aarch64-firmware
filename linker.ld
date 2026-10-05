/*
 * Code and read-only data execute in place from secure flash (where -bios puts
 * the image). Writable data, .bss and the stack live in secure SRAM. This is
 * the same split a boot ROM / TF-A BL1 uses.
 */
ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64M
    SRAM  (rwx) : ORIGIN = 0x0E000000, LENGTH = 16M
}

STACK_SIZE = 16K;

SECTIONS
{
    .text : {
        KEEP(*(.text.boot))             /* _start must be at the reset vector */
        *(.text .text.*)
    } > FLASH

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    } > FLASH

    /* Runs from SRAM, stored in flash; boot.S copies it across. */
    .data : ALIGN(8) {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(8);
        __data_end = .;
    } > SRAM AT > FLASH
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } > SRAM

    .stack (NOLOAD) : ALIGN(16) {
        . += STACK_SIZE;
        __stack_top = .;
    } > SRAM

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
