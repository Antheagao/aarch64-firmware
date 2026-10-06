/*
 * ID register decoding tests.
 *
 * These matter more than most: the Arm ID scheme encodes "feature absent"
 * three different ways, and getting one backwards misreports a feature on
 * every CPU rather than failing loudly. Known register values go in, and the
 * decoded text is checked against what the Arm ARM says they mean.
 */
#include <stdint.h>

#include "cpuid.h"
#include "fake_uart.h"
#include "unit.h"

/* An Armv9 core as QEMU's -cpu max reports it: SVE2, PAC, BTI and MTE all
 * present. Only the fields under test are set. */
static struct cpu_id armv9(void)
{
    struct cpu_id id = {0};

    id.pfr0 = (1UL << 4)      /* EL1 AArch64 */
              | (1UL << 8)    /* EL2 AArch64 */
              | (1UL << 12)   /* EL3 AArch64 */
              | (0UL << 16)   /* FP present (signed field, 0 is a version) */
              | (0UL << 20)   /* AdvSIMD present */
              | (1UL << 32);  /* SVE */
    id.pfr1 = (1UL << 0)      /* BTI */
              | (2UL << 8);   /* MTE2 */
    id.isar1 = (1UL << 4)     /* PAC APA */
               | (1UL << 24); /* PAC generic GPA */
    id.mmfr0 = (5UL << 0)     /* PARange 48 bits */
               | (0UL << 28)  /* 4K granule supported */
               | (0UL << 24)  /* 64K granule supported */
               | (1UL << 20); /* 16K granule supported */
    id.zfr0 = 1UL;            /* SVEver 1 = SVE2 */
    return id;
}

/* An Armv8.0 core: no SVE, no PAC, no BTI, no MTE, and the granule fields
 * using the "not supported" encodings. */
static struct cpu_id armv8(void)
{
    struct cpu_id id = {0};

    id.pfr0 = (1UL << 4) | (1UL << 8) | (1UL << 12);
    id.mmfr0 = (2UL << 0)      /* PARange 40 bits */
               | (0UL << 28)   /* 4K granule supported */
               | (0xfUL << 24) /* 64K granule NOT supported */
               | (0UL << 20);  /* 16K granule NOT supported */
    return id;
}

void test_cpuid(void)
{
    struct cpu_id v9 = armv9();
    struct cpu_id v8 = armv8();

    unit_expect_u32("cpuid_field extracts a 4-bit field", cpuid_field(0xab0UL, 4, 4), 0xb);
    unit_expect_u32("cpuid_field masks to the requested width", cpuid_field(0xffffUL, 0, 4), 0xf);

    /* Arm ARM: in a signed ID field 0b1111 means not implemented, and every
     * other value including 0 is a version. Reading it as "0 means absent"
     * would report FP missing on every CPU that has it. */
    unit_expect_true("a signed field of 0 means present", cpuid_signed_present(0));
    unit_expect_true("a signed field of 1 means present", cpuid_signed_present(1));
    unit_expect_true("a signed field of 0b1111 means absent", !cpuid_signed_present(0xf));

    unit_expect_true("SVE is detected from ID_AA64PFR0_EL1", cpuid_has_sve(&v9));
    unit_expect_true("SVE absence is detected", !cpuid_has_sve(&v8));

    fake_uart_reset();
    cpuid_print(&v9);
    unit_expect_contains("Armv9 reports SVE2", "SVE2 = yes");
    unit_expect_contains("Armv9 reports BTI", "BTI = yes (v1)");
    unit_expect_contains("Armv9 reports MTE level 2", "MTE = yes (v2)");
    unit_expect_contains("Armv9 reports PAC", "PAC (APA) = yes (v1)");
    unit_expect_contains("FP reads as present from a signed field of 0", "FP = yes (v0)");
    unit_expect_contains("EL3 is reported as AArch64", "EL3 = yes (AArch64)");
    unit_expect_contains("PARange 5 decodes to 48 bits", "PA range = 48 bits");
    unit_expect_contains("the 16K granule uses its own encoding", "granule 16K = yes");

    fake_uart_reset();
    cpuid_print(&v8);
    unit_expect_contains("Armv8.0 reports no SVE2", "SVE2 = no (no SVE)");
    unit_expect_contains("Armv8.0 reports no BTI", "BTI = no");
    unit_expect_contains("Armv8.0 reports no MTE", "MTE = no");
    unit_expect_contains("Armv8.0 reports no PAC", "PAC (APA) = no");
    unit_expect_contains("PARange 2 decodes to 40 bits", "PA range = 40 bits");
    unit_expect_contains("a 0b1111 granule field means not supported", "granule 64K = no");
    unit_expect_contains("a zero 16K granule field means not supported", "granule 16K = no");
}
