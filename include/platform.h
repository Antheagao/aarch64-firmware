/* QEMU `virt` machine memory map (secure=on, virtualization=on, gic-version=3).
 * Source: hw/arm/virt.c in QEMU. Real SoCs publish the same kind of table in
 * their TRM; firmware is the first code that has to agree with it. */
#ifndef PLATFORM_H
#define PLATFORM_H

#define SECURE_FLASH_BASE   0x00000000UL  /* -bios image lands here; reset vector */
#define GICD_BASE           0x08000000UL  /* GICv3 distributor */
#define GICR_BASE           0x080A0000UL  /* GICv3 redistributors, 128 KiB per CPU */
#define UART0_BASE          0x09000000UL  /* PL011, wired to QEMU's stdio */
#define SECURE_SRAM_BASE    0x0E000000UL  /* 16 MiB, only visible to Secure world */
#define DRAM_BASE           0x40000000UL  /* Non-secure RAM; EL1 kernel goes here */

#define UART0_CLK_HZ        24000000UL

#endif
