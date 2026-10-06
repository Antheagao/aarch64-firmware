/*
 * System register field definitions.
 *
 * This header is included from assembly as well as C, so it holds plain
 * integer macros and nothing else: no types, no functions, no casts. Field
 * names follow the Arm ARM (DDI 0487) so a value can be checked against the
 * manual without translation.
 */
#ifndef SYSREG_BITS_H
#define SYSREG_BITS_H

/* The assembler has no integer suffixes, so the suffix is added only for C.
 * Same trick as Trusted Firmware-A's U()/UL() macros. */
#ifdef __ASSEMBLER__
#define UL(x) x
#else
#define UL(x) x##UL
#endif

/* SCTLR_EL3, System Control Register (EL3). */
#define SCTLR_EL3_M  (UL(1) << 0)  /* MMU enable */
#define SCTLR_EL3_A  (UL(1) << 1)  /* alignment check enable */
#define SCTLR_EL3_C  (UL(1) << 2)  /* data cache enable */
#define SCTLR_EL3_SA (UL(1) << 3)  /* SP alignment check enable */
#define SCTLR_EL3_I  (UL(1) << 12) /* instruction cache enable */
/* Bits the architecture defines as RES1: writing 0 to them is not allowed. */
#define SCTLR_EL3_RES1                                                               \
    ((UL(1) << 29) | (UL(1) << 28) | (UL(1) << 23) | (UL(1) << 22) | (UL(1) << 18) | \
     (UL(1) << 16) | (UL(1) << 11) | (UL(1) << 5) | (UL(1) << 4))

/* SCR_EL3, Secure Configuration Register. */
#define SCR_EL3_NS   (UL(1) << 0)  /* lower ELs are Non-secure */
#define SCR_EL3_IRQ  (UL(1) << 1)  /* route IRQ to EL3 */
#define SCR_EL3_FIQ  (UL(1) << 2)  /* route FIQ to EL3 */
#define SCR_EL3_EA   (UL(1) << 3)  /* route External Abort and SError to EL3 */
#define SCR_EL3_SMD  (UL(1) << 7)  /* disable SMC */
#define SCR_EL3_HCE  (UL(1) << 8)  /* enable HVC */
#define SCR_EL3_SIF  (UL(1) << 9)  /* Secure state cannot fetch from Non-secure memory */
#define SCR_EL3_RW   (UL(1) << 10) /* next lower EL is AArch64 */
#define SCR_EL3_ST   (UL(1) << 11) /* Secure EL1 may access the timer control registers */
#define SCR_EL3_RES1 ((UL(1) << 5) | (UL(1) << 4))

/* CPTR_EL3, Architectural Feature Trap Register (EL3). */
#define CPTR_EL3_EZ    (UL(1) << 8)  /* 0 traps SVE to EL3 */
#define CPTR_EL3_TFP   (UL(1) << 10) /* 1 traps FP and Advanced SIMD to EL3 */
#define CPTR_EL3_TAM   (UL(1) << 30) /* trap activity monitor access */
#define CPTR_EL3_TCPAC (UL(1) << 31) /* trap CPACR_EL1 and CPTR_EL2 access */

/* SCTLR_EL1, System Control Register (EL1). Same argument as SCTLR_EL3: the
 * architecture resets many of these bits to UNKNOWN, so firmware writes them
 * before handing EL1 over. */
#define SCTLR_EL1_M  (UL(1) << 0)  /* MMU enable */
#define SCTLR_EL1_A  (UL(1) << 1)  /* alignment check enable */
#define SCTLR_EL1_C  (UL(1) << 2)  /* data cache enable */
#define SCTLR_EL1_SA (UL(1) << 3)  /* SP alignment check enable */
#define SCTLR_EL1_I  (UL(1) << 12) /* instruction cache enable */
#define SCTLR_EL1_RES1 \
    ((UL(1) << 29) | (UL(1) << 28) | (UL(1) << 23) | (UL(1) << 22) | (UL(1) << 20) | (UL(1) << 11))

/* HCR_EL2, Hypervisor Configuration Register. Its controls apply to
 * Non-secure EL1 even when EL2 is skipped entirely. */
#define HCR_EL2_RW (UL(1) << 31) /* EL1 is AArch64 */

/* SPSR_EL3: the PSTATE eret installs. Arm ARM (DDI 0487), "Saved Program
 * Status Registers". M[3:0] selects the level and stack pointer. */
#define SPSR_EL3_M_EL1H 0x5          /* EL1 using SP_EL1 */
#define SPSR_EL3_F      (UL(1) << 6) /* FIQ masked */
#define SPSR_EL3_I      (UL(1) << 7) /* IRQ masked */
#define SPSR_EL3_A      (UL(1) << 8) /* SError masked */
#define SPSR_EL3_D      (UL(1) << 9) /* debug exceptions masked */

/* MAIR_EL1 attribute bytes. The descriptors carry an index into this
 * register, so these two must agree with PT_MAIR_* in include/pagetable.h.
 * Arm ARM (DDI 0487), "MAIR_EL1, Memory Attribute Indirection Register". */
#define MAIR_ATTR_NORMAL_WB 0xffUL /* Normal, inner and outer write-back RW-allocate */
#define MAIR_ATTR_DEVICE    0x00UL /* Device-nGnRnE */
#define MAIR_EL1_VALUE      ((MAIR_ATTR_DEVICE << 8) | MAIR_ATTR_NORMAL_WB)

/* TCR_EL1, Translation Control Register (EL1). */
#define TCR_EL1_T0SZ(n)   ((uint64_t)(n) << 0)
#define TCR_EL1_IRGN0_WB  (UL(1) << 8) /* walker reads TTBR0 tables write-back cacheable */
#define TCR_EL1_ORGN0_WB  (UL(1) << 10)
#define TCR_EL1_SH0_INNER (UL(3) << 12)
#define TCR_EL1_TG0_4K    (UL(0) << 14)
#define TCR_EL1_TG1_4K    (UL(2) << 30) /* TG1 encodes 4 KiB as 0b10, unlike TG0 */
#define TCR_EL1_EPD1      (UL(1) << 23) /* no TTBR1 walks: the map is low and identity */
#define TCR_EL1_IPS(n)    ((uint64_t)(n) << 32)

/* VBAR_EL3 holds the vector base address; bits [10:0] are RES0, so the table
 * must be aligned to 2 KiB. Arm ARM (DDI 0487), "Exception vectors". */
#define VBAR_ALIGN      0x800
#define VBAR_ALIGN_MASK (VBAR_ALIGN - 1)

#endif
