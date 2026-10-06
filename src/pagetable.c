/*
 * Stage 1 translation tables. See include/pagetable.h for the shape; this
 * file is the descriptor encoding and the walk.
 *
 * Arm ARM (DDI 0487), "Armv8 translation table level 1, level 2, and level 3
 * descriptor formats" for every bit position below.
 */
#include <stddef.h>
#include <stdint.h>

#include "pagetable.h"

/* Descriptor type, bits [1:0]. The encoding is level-dependent, which is the
 * trap in this format: at level 1 and 2, 0b11 means a *table* and 0b01 means
 * a block. At level 3 it inverts, 0b11 means a *page* and 0b01 is invalid. */
#define DESC_INVALID   0x0UL
#define DESC_BLOCK     0x1UL
#define DESC_TABLE     0x3UL
#define DESC_PAGE      0x3UL
#define DESC_TYPE_MASK 0x3UL

/* Output address, bits [47:12]. The same field carries the next-level table
 * address in a table descriptor and the output address in a leaf. */
#define DESC_ADDR_MASK 0x0000fffffffff000UL

/* Lower attributes. */
#define DESC_ATTRINDX(i) ((uint64_t)(i) << 2)
#define DESC_AP_RW       (0UL << 6) /* read/write at EL1 */
#define DESC_AP_RO       (2UL << 6) /* read-only at EL1 */
#define DESC_SH_NONE     (0UL << 8)
#define DESC_SH_INNER    (3UL << 8)
#define DESC_AF          (1UL << 10) /* access flag */

/* Upper attributes. */
#define DESC_PXN (1UL << 53)
#define DESC_UXN (1UL << 54)

/* 39-bit VA: levels 1, 2 and 3 take nine bits each from bit 30 down. */
#define VA_BITS  39
#define VA_MAX   (1UL << VA_BITS)
#define IDX_MASK 0x1ffUL

static unsigned index_at(uint64_t va, int level)
{
    static const unsigned shift[] = {0, 30, 21, 12};
    return (unsigned)((va >> shift[level]) & IDX_MASK);
}

static uint64_t *alloc_table(struct pt_builder *b)
{
    if (b->used >= b->pool_len)
        return NULL;

    uint64_t *table = b->pool[b->used++];
    for (unsigned i = 0; i < PT_ENTRIES; i++)
        table[i] = 0;
    return table;
}

/* The access flag is the omission that costs an afternoon: a descriptor with
 * AF clear faults on first access even when every other bit is right, and
 * this firmware has no AF-fault handler to explain it. Setting it here, in
 * one place, means no caller can forget. */
static uint64_t leaf_desc(uint64_t pa, int level, enum pt_type type, enum pt_perm perm)
{
    uint64_t desc = (pa & DESC_ADDR_MASK) | DESC_AF;

    desc |= (level == 3) ? DESC_PAGE : DESC_BLOCK;

    if (type == PT_DEVICE) {
        desc |= DESC_ATTRINDX(PT_MAIR_DEVICE);
        /* Shareability is ignored for Device memory, so it is left at 0
         * rather than given a value that would imply it means something. */
        desc |= DESC_SH_NONE;
    } else {
        desc |= DESC_ATTRINDX(PT_MAIR_NORMAL);
        desc |= DESC_SH_INNER;
    }

    desc |= (perm == PT_RW_XN || perm == PT_RW_X) ? DESC_AP_RW : DESC_AP_RO;

    /* Execute-never is two bits, and clearing only one leaves the memory
     * executable from the other privilege level. */
    if (perm != PT_RO_X && perm != PT_RW_X)
        desc |= DESC_UXN | DESC_PXN;

    return desc;
}

/* Install `desc` as the entry for `va` at `target_level`, creating the
 * intermediate tables on the way down. Iterative: docs/CODING_STANDARDS.md
 * forbids recursion, and the depth is three. */
static int set_entry(struct pt_builder *b, uint64_t va, int target_level, uint64_t desc)
{
    uint64_t *table = b->pool[0];

    for (int level = 1; level < target_level; level++) {
        unsigned idx = index_at(va, level);
        uint64_t entry = table[idx];

        if ((entry & DESC_TYPE_MASK) == DESC_BLOCK)
            return PT_E_OVERLAP; /* a larger block already covers this VA */

        if ((entry & DESC_TYPE_MASK) != DESC_TABLE) {
            uint64_t *next = alloc_table(b);
            if (next == NULL)
                return PT_E_POOL;
            table[idx] = ((uint64_t)(uintptr_t)next & DESC_ADDR_MASK) | DESC_TABLE;
            entry = table[idx];
        }

        table = (uint64_t *)(uintptr_t)(entry & DESC_ADDR_MASK);
    }

    table[index_at(va, target_level)] = desc;
    return PT_OK;
}

static int map_region(struct pt_builder *b, const struct pt_region *r)
{
    uint64_t va = r->base;
    uint64_t end = r->base + r->size;

    if ((r->base | r->size) % PT_PAGE_L3 != 0)
        return PT_E_ALIGN;
    if (end > VA_MAX || end < r->base)
        return PT_E_RANGE;

    while (va < end) {
        uint64_t left = end - va;
        int level;
        uint64_t step;

        /* Largest block that fits, as docs/CODING_STANDARDS.md asks: fewer
         * tables to build and fewer TLB entries to fill. */
        if (va % PT_BLOCK_L1 == 0 && left >= PT_BLOCK_L1) {
            level = 1;
            step = PT_BLOCK_L1;
        } else if (va % PT_BLOCK_L2 == 0 && left >= PT_BLOCK_L2) {
            level = 2;
            step = PT_BLOCK_L2;
        } else {
            level = 3;
            step = PT_PAGE_L3;
        }

        int err = set_entry(b, va, level, leaf_desc(va, level, r->type, r->perm));
        if (err != PT_OK)
            return err;

        va += step;
    }

    return PT_OK;
}

int pt_build(struct pt_builder *b, const struct pt_region *regions, unsigned n)
{
    if (b->pool_len < 1)
        return PT_E_POOL;

    b->used = 0;
    if (alloc_table(b) == NULL) /* pool[0] is the root */
        return PT_E_POOL;

    for (unsigned i = 0; i < n; i++) {
        int err = map_region(b, &regions[i]);
        if (err != PT_OK)
            return err;
    }

    return PT_OK;
}

const uint64_t *pt_root(const struct pt_builder *b)
{
    return b->used > 0 ? b->pool[0] : NULL;
}

uint64_t pt_lookup(const struct pt_builder *b, uint64_t va, int *level)
{
    const uint64_t *table = b->pool[0];

    if (b->used == 0)
        return 0;

    for (int l = 1; l <= 3; l++) {
        uint64_t entry = table[index_at(va, l)];
        uint64_t type = entry & DESC_TYPE_MASK;

        if (type == DESC_INVALID)
            return 0;

        /* A leaf: a block at level 1 or 2, a page at level 3. */
        if (l == 3 || type == DESC_BLOCK) {
            *level = l;
            return entry;
        }

        table = (const uint64_t *)(uintptr_t)(entry & DESC_ADDR_MASK);
    }

    return 0;
}
