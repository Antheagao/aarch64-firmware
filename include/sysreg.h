#ifndef SYSREG_H
#define SYSREG_H

#include <stdint.h>

#define read_sysreg(reg)                                 \
    ({                                                   \
        uint64_t __val;                                  \
        __asm__ volatile("mrs %0, " #reg : "=r"(__val)); \
        __val;                                           \
    })

#define write_sysreg(reg, val) __asm__ volatile("msr " #reg ", %0" : : "r"((uint64_t)(val)))

#define isb()   __asm__ volatile("isb" : : : "memory")
#define dsb(op) __asm__ volatile("dsb " #op : : : "memory")

/* CurrentEL holds the exception level in bits [3:2]. */
static inline unsigned current_el(void)
{
    return (read_sysreg(CurrentEL) >> 2) & 0x3;
}

#endif
