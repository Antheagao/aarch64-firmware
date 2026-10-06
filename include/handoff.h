/* Handing control from EL3 down to EL1. */
#ifndef HANDOFF_H
#define HANDOFF_H

#include <stdint.h>

#include "cpuid.h"

/* Configure the Non-secure EL1 state and eret to `entry`. Does not return:
 * the drop to a lower exception level is one-way. */
void el3_enter_el1(uint64_t entry, const struct cpu_id *id) __attribute__((noreturn));

#endif
