/*
 * ID register decoding. One table of fields, one function that walks it, as
 * docs/CODING_STANDARDS.md requires: adding a feature is a row, not another
 * branch.
 *
 * Field positions are from the Arm ARM (DDI 0487), "AArch64 System Register
 * Descriptions", one section per ID register.
 */
#include <stdbool.h>
#include <stdint.h>

#include "cpuid.h"
#include "kprintf.h"

#ifdef __aarch64__
#include "sysreg.h"
#endif

uint32_t cpuid_field(uint64_t reg, unsigned shift, unsigned width)
{
    return (uint32_t)((reg >> shift) & ((1UL << width) - 1));
}

bool cpuid_signed_present(uint32_t value)
{
    return value != 0xf;
}

bool cpuid_has_sve(const struct cpu_id *id)
{
    return cpuid_field(id->pfr0, 32, 4) != 0; /* ID_AA64PFR0_EL1.SVE */
}

/* Which raw register a row reads. */
enum id_reg {
    REG_PFR0,
    REG_PFR1,
    REG_ISAR0,
    REG_ISAR1,
    REG_ISAR2,
    REG_MMFR0,
    REG_DFR0,
    REG_ZFR0,
};

/* How a field's value becomes text. The variety is not decoration: the ID
 * scheme genuinely encodes "not present" three different ways. */
enum id_kind {
    KIND_NONZERO, /* 0 means absent, anything else present at that version */
    KIND_SIGNED,  /* 0b1111 means absent (Arm ARM signed-field rule) */
    KIND_EL,      /* exception level: 0 absent, 1 AArch64, 2 AArch64+AArch32 */
    KIND_PARANGE, /* physical address range lookup */
    KIND_TGRAN,   /* 0 supported, 0b1111 not: the 4K and 64K granule rule */
    KIND_TGRAN16, /* 0 NOT supported, non-zero supported: the 16K exception */
    KIND_RAW,     /* print the number and let the reader judge */
};

struct id_field {
    const char *name;
    enum id_reg reg;
    uint8_t shift;
    uint8_t width;
    enum id_kind kind;
};

/* The features docs/ROADMAP.md asks M2 to report, in reading order. */
static const struct id_field FIELDS[] = {
    {"EL1", REG_PFR0, 4, 4, KIND_EL},
    {"EL2", REG_PFR0, 8, 4, KIND_EL},
    {"EL3", REG_PFR0, 12, 4, KIND_EL},
    {"FP", REG_PFR0, 16, 4, KIND_SIGNED},
    {"AdvSIMD", REG_PFR0, 20, 4, KIND_SIGNED},
    {"SVE", REG_PFR0, 32, 4, KIND_NONZERO},
    {"AMU", REG_PFR0, 44, 4, KIND_NONZERO},
    {"RME", REG_PFR0, 52, 4, KIND_NONZERO},
    {"BTI", REG_PFR1, 0, 4, KIND_NONZERO},
    {"MTE", REG_PFR1, 8, 4, KIND_NONZERO},
    {"SME", REG_PFR1, 24, 4, KIND_NONZERO},
    {"LSE atomics", REG_ISAR0, 20, 4, KIND_NONZERO},
    {"PAC (APA)", REG_ISAR1, 4, 4, KIND_NONZERO},
    {"PAC (API)", REG_ISAR1, 8, 4, KIND_NONZERO},
    {"PAC generic (GPA)", REG_ISAR1, 24, 4, KIND_NONZERO},
    {"PAC generic (GPI)", REG_ISAR1, 28, 4, KIND_NONZERO},
    {"PAC (APA3)", REG_ISAR2, 12, 4, KIND_NONZERO},
    {"PMU version", REG_DFR0, 8, 4, KIND_RAW},
    {"SPE (PMSVer)", REG_DFR0, 32, 4, KIND_NONZERO},
    {"PA range", REG_MMFR0, 0, 4, KIND_PARANGE},
    {"granule 4K", REG_MMFR0, 28, 4, KIND_TGRAN},
    {"granule 64K", REG_MMFR0, 24, 4, KIND_TGRAN},
    {"granule 16K", REG_MMFR0, 20, 4, KIND_TGRAN16},
};

static uint64_t reg_value(const struct cpu_id *id, enum id_reg reg)
{
    switch (reg) {
    case REG_PFR0:
        return id->pfr0;
    case REG_PFR1:
        return id->pfr1;
    case REG_ISAR0:
        return id->isar0;
    case REG_ISAR1:
        return id->isar1;
    case REG_ISAR2:
        return id->isar2;
    case REG_MMFR0:
        return id->mmfr0;
    case REG_DFR0:
        return id->dfr0;
    case REG_ZFR0:
        return id->zfr0;
    }
    return 0;
}

/* ID_AA64MMFR0_EL1.PARange encoding, in bits. */
static const uint16_t PARANGE_BITS[] = {32, 36, 40, 42, 44, 48, 52, 56};

static void print_field(const struct id_field *f, uint32_t v)
{
    switch (f->kind) {
    case KIND_NONZERO:
        if (v)
            kprintf("yes (v%u)", v);
        else
            kprintf("no");
        break;
    case KIND_SIGNED:
        if (cpuid_signed_present(v))
            kprintf("yes (v%u)", v);
        else
            kprintf("no");
        break;
    case KIND_EL:
        if (v == 0)
            kprintf("no");
        else if (v == 1)
            kprintf("yes (AArch64)");
        else
            kprintf("yes (AArch64+AArch32)");
        break;
    case KIND_PARANGE:
        if (v < sizeof(PARANGE_BITS) / sizeof(PARANGE_BITS[0]))
            kprintf("%u bits", PARANGE_BITS[v]);
        else
            kprintf("reserved (0x%x)", v);
        break;
    case KIND_TGRAN:
        kprintf("%s", v == 0xf ? "no" : "yes");
        break;
    case KIND_TGRAN16:
        kprintf("%s", v == 0 ? "no" : "yes");
        break;
    case KIND_RAW:
        kprintf("0x%x", v);
        break;
    }
}

void cpuid_print(const struct cpu_id *id)
{
    for (unsigned i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); i++) {
        const struct id_field *f = &FIELDS[i];
        kprintf("cpu: %18s = ", f->name);
        print_field(f, cpuid_field(reg_value(id, f->reg), f->shift, f->width));
        kprintf("\n");
    }

    /* SVE2 lives in ID_AA64ZFR0_EL1, which is only architecturally valid to
     * read when SVE is implemented, so it is reported outside the table. */
    kprintf("cpu: %18s = ", "SVE2");
    if (!cpuid_has_sve(id))
        kprintf("no (no SVE)\n");
    else
        kprintf("%s", cpuid_field(id->zfr0, 0, 4) >= 1 ? "yes\n" : "no\n");
}

#ifdef __aarch64__
void cpuid_read(struct cpu_id *id)
{
    id->pfr0 = read_sysreg(id_aa64pfr0_el1);
    id->pfr1 = read_sysreg(id_aa64pfr1_el1);
    id->isar0 = read_sysreg(id_aa64isar0_el1);
    id->isar1 = read_sysreg(id_aa64isar1_el1);
    id->isar2 = read_sysreg(id_aa64isar2_el1);
    id->mmfr0 = read_sysreg(id_aa64mmfr0_el1);
    id->dfr0 = read_sysreg(id_aa64dfr0_el1);
    /* Reading ID_AA64ZFR0_EL1 without SVE is not architecturally valid, so
     * it stays zero unless the CPU says SVE is there. */
    id->zfr0 = cpuid_has_sve(id) ? read_sysreg(S3_0_C0_C4_4) : 0;
}
#endif
