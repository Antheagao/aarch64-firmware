/*
 * Translation table builder tests.
 *
 * These exist because the failure mode on the target is a hang with nothing
 * to read: the MMU either translates or it does not, and a wrong bit gives
 * no diagnostic. Every encoding rule the Arm ARM states differently from
 * what you would guess gets a test here.
 */
#include <stdint.h>

#include "pagetable.h"
#include "unit.h"

#define POOL_TABLES 8
static pt_table pool[POOL_TABLES] __attribute__((aligned(PT_TABLE_BYTES)));

static struct pt_builder builder(void)
{
    struct pt_builder b = {.pool = pool, .pool_len = POOL_TABLES, .used = 0};
    return b;
}

/* Descriptor field accessors, spelled out so a test failure says which bit. */
#define D_TYPE(d)     ((d) & 0x3UL)
#define D_ATTRIDX(d)  (((d) >> 2) & 0x7UL)
#define D_AP(d)       (((d) >> 6) & 0x3UL)
#define D_SH(d)       (((d) >> 8) & 0x3UL)
#define D_AF(d)       (((d) >> 10) & 0x1UL)
#define D_PXN(d)      (((d) >> 53) & 0x1UL)
#define D_UXN(d)      (((d) >> 54) & 0x1UL)
#define D_ADDR(d)     ((d) & 0x0000fffffffff000UL)

void test_pagetable(void);

void test_pagetable(void)
{
    struct pt_builder b;
    int level = 0;
    uint64_t d;

    /* A 1 GiB-aligned 1 GiB region should become one level 1 block, not
     * 262144 pages. This is the whole point of the largest-block rule. */
    b = builder();
    struct pt_region one_gib = {0x40000000UL, PT_BLOCK_L1, PT_NORMAL, PT_RW_XN};
    unit_expect_u32("a 1 GiB aligned region builds", (uint32_t)pt_build(&b, &one_gib, 1), PT_OK);
    d = pt_lookup(&b, 0x40000000UL, &level);
    unit_expect_u32("it maps at level 1", (uint32_t)level, 1);
    unit_expect_u32("as a block descriptor, not a table", (uint32_t)D_TYPE(d), 1);
    unit_expect_true("identity mapped", D_ADDR(d) == 0x40000000UL);
    unit_expect_u32("only the root table is used", b.used, 1);

    /* 2 MiB lands at level 2, which needs one more table. */
    b = builder();
    struct pt_region two_mib = {0x40000000UL, PT_BLOCK_L2, PT_NORMAL, PT_RW_XN};
    unit_expect_u32("a 2 MiB region builds", (uint32_t)pt_build(&b, &two_mib, 1), PT_OK);
    d = pt_lookup(&b, 0x40000000UL, &level);
    unit_expect_u32("it maps at level 2", (uint32_t)level, 2);
    unit_expect_u32("as a block descriptor", (uint32_t)D_TYPE(d), 1);
    unit_expect_u32("root plus one level 2 table", b.used, 2);

    /* 4 KiB goes all the way down, and the leaf encoding inverts: at level 3
     * a leaf is 0b11, the value that means "table" higher up. */
    b = builder();
    struct pt_region one_page = {0x40000000UL, PT_PAGE_L3, PT_NORMAL, PT_RW_XN};
    unit_expect_u32("a 4 KiB region builds", (uint32_t)pt_build(&b, &one_page, 1), PT_OK);
    d = pt_lookup(&b, 0x40000000UL, &level);
    unit_expect_u32("it maps at level 3", (uint32_t)level, 3);
    unit_expect_u32("a level 3 leaf is 0b11, not 0b01", (uint32_t)D_TYPE(d), 3);
    unit_expect_u32("root plus a level 2 and a level 3 table", b.used, 3);

    /* The access flag: clear it and the first access faults, with no
     * handler in this firmware to say why. */
    unit_expect_u32("the access flag is set on a leaf", (uint32_t)D_AF(d), 1);

    /* Attributes and permissions. */
    b = builder();
    struct pt_region regions[] = {
        {0x40000000UL, PT_BLOCK_L1, PT_NORMAL, PT_RO_X},  /* text-like */
        {0x80000000UL, PT_BLOCK_L1, PT_NORMAL, PT_RO_XN}, /* rodata-like */
        {0x09000000UL, PT_BLOCK_L2, PT_DEVICE, PT_RW_XN}, /* the UART */
    };
    unit_expect_u32("a mixed map builds", (uint32_t)pt_build(&b, regions, 3), PT_OK);

    d = pt_lookup(&b, 0x40000000UL, &level);
    unit_expect_u32("executable memory is Normal (attr 0)", (uint32_t)D_ATTRIDX(d), PT_MAIR_NORMAL);
    unit_expect_u32("Normal memory is inner shareable", (uint32_t)D_SH(d), 3);
    unit_expect_u32("read-only sets AP[7:6] to 0b10", (uint32_t)D_AP(d), 2);
    unit_expect_u32("executable memory clears PXN", (uint32_t)D_PXN(d), 0);
    unit_expect_u32("executable memory clears UXN", (uint32_t)D_UXN(d), 0);

    d = pt_lookup(&b, 0x80000000UL, &level);
    unit_expect_u32("read-only data sets PXN", (uint32_t)D_PXN(d), 1);
    unit_expect_u32("read-only data sets UXN too, not just one of them",
                    (uint32_t)D_UXN(d), 1);

    d = pt_lookup(&b, 0x09000000UL, &level);
    unit_expect_u32("the UART is Device (attr 1)", (uint32_t)D_ATTRIDX(d), PT_MAIR_DEVICE);
    unit_expect_u32("Device memory is not marked shareable", (uint32_t)D_SH(d), 0);
    unit_expect_u32("Device memory is execute-never", (uint32_t)D_UXN(d), 1);

    /* Nothing maps an address outside the regions. */
    unit_expect_true("an unmapped address looks up as 0",
                     pt_lookup(&b, 0xc0000000UL, &level) == 0);

    /* Errors are returned, not walked past. */
    b = builder();
    struct pt_region unaligned = {0x40000800UL, PT_PAGE_L3, PT_NORMAL, PT_RW_XN};
    unit_expect_u32("a misaligned base is rejected",
                    (uint32_t)pt_build(&b, &unaligned, 1), (uint32_t)PT_E_ALIGN);

    b = builder();
    struct pt_region too_high = {(1UL << 39) - PT_PAGE_L3, PT_PAGE_L3 * 2, PT_NORMAL, PT_RW_XN};
    unit_expect_u32("a region past the 39-bit VA space is rejected",
                    (uint32_t)pt_build(&b, &too_high, 1), (uint32_t)PT_E_RANGE);

    /* Pool exhaustion must be reported rather than overrunning the array:
     * each 4 KiB page in a fresh 2 MiB span needs its own level 3 table. */
    struct pt_builder small = {.pool = pool, .pool_len = 2, .used = 0};
    struct pt_region spread[] = {
        {0x40000000UL, PT_PAGE_L3, PT_NORMAL, PT_RW_XN},
        {0x80000000UL, PT_PAGE_L3, PT_NORMAL, PT_RW_XN},
    };
    unit_expect_u32("running out of tables is reported",
                    (uint32_t)pt_build(&small, spread, 2), (uint32_t)PT_E_POOL);

    /* A page inside an existing block is an overlap, not a silent split. */
    b = builder();
    struct pt_region overlap[] = {
        {0x40000000UL, PT_BLOCK_L1, PT_NORMAL, PT_RW_XN},
        {0x40000000UL, PT_PAGE_L3, PT_DEVICE, PT_RW_XN},
    };
    unit_expect_u32("mapping inside an existing block is rejected",
                    (uint32_t)pt_build(&b, overlap, 2), (uint32_t)PT_E_OVERLAP);

    /* A span that is not block-aligned should use the largest block it can
     * and pages for the remainder, rather than falling back to all pages. */
    b = builder();
    struct pt_region mixed = {0x40000000UL, PT_BLOCK_L2 + PT_PAGE_L3, PT_NORMAL, PT_RW_XN};
    unit_expect_u32("a block plus a remainder builds", (uint32_t)pt_build(&b, &mixed, 1), PT_OK);
    pt_lookup(&b, 0x40000000UL, &level);
    unit_expect_u32("the aligned part still uses a level 2 block", (uint32_t)level, 2);
    pt_lookup(&b, 0x40000000UL + PT_BLOCK_L2, &level);
    unit_expect_u32("the remainder uses level 3 pages", (uint32_t)level, 3);
}
