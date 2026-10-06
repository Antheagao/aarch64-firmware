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

/* VBAR_EL3 holds the vector base address; bits [10:0] are RES0, so the table
 * must be aligned to 2 KiB. Arm ARM (DDI 0487), "Exception vectors". */
#define VBAR_ALIGN      0x800
#define VBAR_ALIGN_MASK (VBAR_ALIGN - 1)

#endif
