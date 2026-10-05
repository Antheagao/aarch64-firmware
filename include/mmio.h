/* Memory-mapped I/O accessors. Drivers use these instead of casting
 * pointers to volatile themselves, so every device access is in one place. */
#ifndef MMIO_H
#define MMIO_H

#include <stdbool.h>
#include <stdint.h>

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write32(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

/* Spin until every bit in `mask` reads as 0, or until `max_spins` reads have
 * been made. Returns true if the bits cleared, false on timeout. A device that
 * stops responding must never hang firmware, so every hardware poll goes
 * through here with a finite budget. */
static inline bool mmio_poll_clear32(uintptr_t addr, uint32_t mask, uint32_t max_spins)
{
    for (uint32_t i = 0; i < max_spins; i++) {
        if (!(mmio_read32(addr) & mask))
            return true;
    }
    return false;
}

#endif
