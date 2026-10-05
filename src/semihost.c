#include <stdint.h>

#include "semihost.h"

#define SYS_EXIT                        0x18
#define ADP_STOPPED_APPLICATION_EXIT    0x20026

void semihost_exit(int code)
{
    /* On AArch64, SYS_EXIT takes a pointer to a {reason, exit code} block. */
    uint64_t block[2] = { ADP_STOPPED_APPLICATION_EXIT, (uint64_t)(int64_t)code };
    register uint64_t x0 __asm__("x0") = SYS_EXIT;
    register uint64_t x1 __asm__("x1") = (uint64_t)(uintptr_t)block;

    __asm__ volatile("hlt #0xf000" : : "r"(x0), "r"(x1) : "memory");
    for (;;)
        __asm__ volatile("wfe");
}
