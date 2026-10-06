/*
 * Stage 1 translation table builder: 4 KiB granule, 39-bit VA, three levels.
 *
 * Deliberately pure. It fills a caller-provided pool and writes no system
 * registers, so the whole thing runs under the host unit tests. That matters
 * more here than elsewhere: a wrong descriptor bit shows up on the target as
 * a hang with nothing to read, and the Arm ARM encodes "leaf" three
 * different ways depending on the level.
 *
 * Field positions are from the Arm ARM (DDI 0487), "Armv8 translation table
 * level 1, level 2, and level 3 descriptor formats".
 */
#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <stdint.h>

#define PT_ENTRIES     512
#define PT_TABLE_BYTES 4096

/* With a 39-bit VA the walk starts at level 1; there is no level 0. */
#define PT_BLOCK_L1 (1024UL * 1024 * 1024) /* 1 GiB */
#define PT_BLOCK_L2 (2UL * 1024 * 1024)    /* 2 MiB */
#define PT_PAGE_L3  4096UL

typedef uint64_t pt_table[PT_ENTRIES];

/* MAIR_EL1 attribute indices. attr0 is Normal write-back (0xff) and attr1 is
 * Device-nGnRnE (0x00); the descriptor carries the index, not the type. */
#define PT_MAIR_NORMAL 0
#define PT_MAIR_DEVICE 1

enum pt_type {
    PT_NORMAL,
    PT_DEVICE,
};

/* Permissions as this firmware needs them. AP[7:6] has four encodings, but
 * EL0 never runs here, so only the EL1 read/write distinction is exposed. */
enum pt_perm {
    PT_RW_XN, /* data, stack: writable, never executable */
    PT_RO_XN, /* rodata: read-only, never executable */
    PT_RO_X,  /* text: read-only, executable */
};

struct pt_region {
    uint64_t base;
    uint64_t size;
    enum pt_type type;
    enum pt_perm perm;
};

struct pt_builder {
    pt_table *pool;    /* caller's tables; pool[0] becomes the root */
    unsigned pool_len; /* how many tables are available */
    unsigned used;     /* bump pointer into the pool */
};

/* Negative error codes, as docs/CODING_STANDARDS.md requires. */
#define PT_OK        0
#define PT_E_POOL    (-1) /* ran out of tables */
#define PT_E_ALIGN   (-2) /* base or size not a multiple of 4 KiB */
#define PT_E_OVERLAP (-3) /* a block already covers this range */
#define PT_E_RANGE   (-4) /* outside the 39-bit VA space */

/* Build an identity map for `n` regions. Returns PT_OK or a negative code.
 * The pool is zeroed as tables are handed out, so the caller need not. */
int pt_build(struct pt_builder *b, const struct pt_region *regions, unsigned n);

/* The root table to put in TTBR0_EL1, or NULL before a successful build. */
const uint64_t *pt_root(const struct pt_builder *b);

/* Walk the built tables for `va` and return the leaf descriptor, plus the
 * level it was found at. Returns 0 if nothing maps it. For tests and for
 * printing the map, not for the translation path. */
uint64_t pt_lookup(const struct pt_builder *b, uint64_t va, int *level);

#endif
