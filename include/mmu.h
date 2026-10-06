/* Turning the MMU on at EL1. */
#ifndef MMU_H
#define MMU_H

#include <stdbool.h>

/* Build the translation tables and enable the MMU, data cache and
 * instruction cache. Returns false, having said why, if the tables could not
 * be built; if the tables are wrong, this does not return at all. */
bool mmu_enable(void);

#endif
