/*
 * Turn the MMU on at EL1.
 *
 * The map itself is built by src/pagetable.c, which is pure and tested on
 * the host. What is left here is the register programming and the barriers,
 * and that is where a mistake stops being a message and becomes a hang: once
 * SCTLR_EL1.M is set, every fetch and every load goes through tables that
 * are either right or not there.
 */
#include <stdbool.h>
#include <stdint.h>

#include "cpuid.h"
#include "kprintf.h"
#include "mmu.h"
#include "pagetable.h"
#include "platform.h"
#include "sysreg.h"
#include "sysreg_bits.h"

/* Enough for the root plus a level 2 table for the device block, with room
 * to spare. In .bss, which is NOLOAD, so it costs nothing in the image. */
#define POOL_TABLES 16
static pt_table table_pool[POOL_TABLES] __attribute__((aligned(PT_TABLE_BYTES)));

static struct pt_builder builder = {
    .pool = table_pool,
    .pool_len = POOL_TABLES,
    .used = 0,
};

/* Section bounds from kernel/kernel.ld, all page-aligned there so each can
 * carry its own permissions. Only their addresses are used. */
/* Leading double underscores are reserved for the implementation in C, and
 * clang-tidy is right to say so. They are kept because that is the universal
 * convention for linker-defined symbols, and kernel/kernel.ld is where these
 * are actually defined; the suppression is scoped to these three lines
 * rather than turning the check off everywhere. */
// NOLINTBEGIN(bugprone-reserved-identifier)
extern char __ktext_start[], __ktext_end[];
extern char __krodata_start[], __krodata_end[];
extern char __kdata_start[], __kend[];
// NOLINTEND(bugprone-reserved-identifier)

static uint64_t sym(const char *s)
{
    return (uint64_t)(uintptr_t)s;
}

/* One region per section, rather than one block for the lot. Nothing is
 * writable and executable at once, and nothing outside the image is mapped
 * at all: an address the kernel has no business touching now faults instead
 * of quietly working. */
static unsigned build_regions(struct pt_region *out)
{
    unsigned n = 0;

    out[n++] = (struct pt_region){sym(__ktext_start), sym(__ktext_end) - sym(__ktext_start),
                                  PT_NORMAL, PT_RO_X};
    out[n++] = (struct pt_region){sym(__krodata_start), sym(__krodata_end) - sym(__krodata_start),
                                  PT_NORMAL, PT_RO_XN};
    /* .data, .bss and the stack are contiguous and share one permission. */
    out[n++] = (struct pt_region){sym(__kdata_start), sym(__kend) - sym(__kdata_start), PT_NORMAL,
                                  PT_RW_XN};
    out[n++] = (struct pt_region){UART0_BASE, PT_BLOCK_L2, PT_DEVICE, PT_RW_XN};
    return n;
}

/* IPS must describe the CPU, not a guess: it is the one TCR field that
 * depends on the implementation. ID_AA64MMFR0_EL1.PARange uses the same
 * encoding, so it is copied across, capped at 48 bits because anything
 * larger needs FEAT_LPA support this firmware does not have. */
static uint64_t ips_from_cpu(void)
{
    struct cpu_id id;

    cpuid_read(&id);
    uint32_t parange = cpuid_field(id.mmfr0, 0, 4);
    return parange > 5 ? 5 : parange;
}

bool mmu_enable(void)
{
    struct pt_region regions[8];
    unsigned n = build_regions(regions);

    int err = pt_build(&builder, regions, n);
    if (err != PT_OK) {
        kprintf("kernel: page tables failed: %d\n", err);
        return false;
    }
    kprintf("kernel: page tables built, %u of %u tables used\n", builder.used, POOL_TABLES);
    kprintf("kernel: text %p-%p RX, rodata %p-%p RO, data %p-%p RW+XN\n", (void *)__ktext_start,
            (void *)__ktext_end, (void *)__krodata_start, (void *)__krodata_end,
            (void *)__kdata_start, (void *)__kend);

    write_sysreg(mair_el1, MAIR_EL1_VALUE);
    write_sysreg(tcr_el1, TCR_EL1_T0SZ(64 - 39) | TCR_EL1_IRGN0_WB | TCR_EL1_ORGN0_WB |
                              TCR_EL1_SH0_INNER | TCR_EL1_TG0_4K | TCR_EL1_TG1_4K | TCR_EL1_EPD1 |
                              TCR_EL1_IPS(ips_from_cpu()));
    write_sysreg(ttbr0_el1, (uint64_t)(uintptr_t)pt_root(&builder));
    isb(); /* the three registers above must be in effect before the enable */

    /* The tables were written with the MMU and caches off, so they went
     * straight to memory; the walker is about to read them as Normal
     * cacheable. dsb ish makes those writes visible to it, and the TLB is
     * invalidated because nothing guarantees it was empty.
     *
     * On real hardware this sequence also wants cache maintenance over the
     * tables, since writes made with caches off can sit behind stale clean
     * lines. QEMU does not model that, so the omission is recorded here
     * rather than discovered on silicon. */
    dsb(ishst);
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    dsb(ish);
    isb();

    uint64_t sctlr = read_sysreg(sctlr_el1);
    sctlr |= SCTLR_EL1_M | SCTLR_EL1_C | SCTLR_EL1_I;
    write_sysreg(sctlr_el1, sctlr);
    isb(); /* from here every access is translated */

    /* Reaching this line at all means the map covers this code, this stack
     * and the UART, because the read, the call and the print all went
     * through it. */
    sctlr = read_sysreg(sctlr_el1);
    kprintf("kernel: MMU enabled: %s\n", (sctlr & SCTLR_EL1_M) ? "ok" : "FAIL");
    kprintf("kernel: SCTLR_EL1=0x%016lx TTBR0_EL1=0x%016lx\n", sctlr, read_sysreg(ttbr0_el1));
    return true;
}
