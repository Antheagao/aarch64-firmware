/*
 * CPU feature discovery from the AArch64 ID registers.
 *
 * Firmware has to ask the CPU what it is before enabling anything, and the
 * ID registers are the only honest answer. The decode is kept separate from
 * the reads so it can be unit-tested on the host against known values: the
 * field rules have enough exceptions that testing them matters more than the
 * reading does.
 */
#ifndef CPUID_H
#define CPUID_H

#include <stdbool.h>
#include <stdint.h>

/* Raw ID register values, read once at boot. */
struct cpu_id {
    uint64_t pfr0;  /* ID_AA64PFR0_EL1 */
    uint64_t pfr1;  /* ID_AA64PFR1_EL1 */
    uint64_t isar0; /* ID_AA64ISAR0_EL1 */
    uint64_t isar1; /* ID_AA64ISAR1_EL1 */
    uint64_t isar2; /* ID_AA64ISAR2_EL1 */
    uint64_t mmfr0; /* ID_AA64MMFR0_EL1 */
    uint64_t dfr0;  /* ID_AA64DFR0_EL1 */
    uint64_t zfr0;  /* ID_AA64ZFR0_EL1, only valid when SVE is implemented */
};

/* Extract an ID register field. Fields are 4 bits unless stated otherwise in
 * the Arm ARM. */
uint32_t cpuid_field(uint64_t reg, unsigned shift, unsigned width);

/* True when a *signed* ID field reports the feature present. Arm ARM
 * (DDI 0487), "Principles of the ID scheme for fields in ID registers": in a
 * signed field 0b1111 means not implemented, and other values are versions.
 * Getting this backwards reports FP and AdvSIMD as absent on every CPU that
 * has them, which is why it is its own function with its own tests. */
bool cpuid_signed_present(uint32_t value);

/* True when SVE is implemented, so ID_AA64ZFR0_EL1 may be read at all. */
bool cpuid_has_sve(const struct cpu_id *id);

/* Print the feature table over kprintf. Pure: it only reads the struct. */
void cpuid_print(const struct cpu_id *id);

/* Read the ID registers. Declared unconditionally so target-only callers
 * still compile under the host syntax check; only the definition in
 * src/cpuid.c is guarded, because the host has no system registers. */
void cpuid_read(struct cpu_id *id);

#endif
