/*
 * The trap frame: the register state src/vectors.S pushes on an exception,
 * and the view src/trap.c decodes it through. The layout is the same at
 * every exception level, so both images share this header.
 *
 * The byte offsets below are what the assembly uses. The C struct below
 * must agree with them exactly, and the _Static_asserts make a mismatch a
 * build error rather than a crash inside the crash handler.
 */
#ifndef TRAPFRAME_H
#define TRAPFRAME_H

/* Offsets into the frame, in bytes. x0..x30 come first, in order. */
#define TF_X0     0
#define TF_X30    240
#define TF_ELR    248
#define TF_SPSR   256
#define TF_ESR    264
#define TF_FAR    272
#define TF_VECTOR 280
#define TF_SIZE   288

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>

struct trap_frame {
    uint64_t x[31];  /* x0 through x30 */
    uint64_t elr;    /* ELR_EL3: where execution resumes */
    uint64_t spsr;   /* SPSR_EL3: saved PSTATE */
    uint64_t esr;    /* ESR_EL3: why the exception was taken */
    uint64_t far;    /* FAR_EL3: the faulting address, for aborts */
    uint64_t vector; /* which of the 16 vector slots was entered */
};

_Static_assert(sizeof(struct trap_frame) == TF_SIZE, "frame size must match vectors.S");
_Static_assert(TF_SIZE % 16 == 0, "SP must stay 16-byte aligned");
_Static_assert(offsetof(struct trap_frame, x) == TF_X0, "TF_X0 must match vectors.S");
_Static_assert(offsetof(struct trap_frame, x[30]) == TF_X30, "TF_X30 must match vectors.S");
_Static_assert(offsetof(struct trap_frame, elr) == TF_ELR, "TF_ELR must match vectors.S");
_Static_assert(offsetof(struct trap_frame, spsr) == TF_SPSR, "TF_SPSR must match vectors.S");
_Static_assert(offsetof(struct trap_frame, esr) == TF_ESR, "TF_ESR must match vectors.S");
_Static_assert(offsetof(struct trap_frame, far) == TF_FAR, "TF_FAR must match vectors.S");
_Static_assert(offsetof(struct trap_frame, vector) == TF_VECTOR, "TF_VECTOR must match vectors.S");

/* Called from src/vectors.S with the frame it just pushed. One copy is
 * built into each image; TRAP_EL says which exception level it serves. */
void trap_handler(struct trap_frame *tf);

/* Arm the handler to recover from the next exception instead of parking:
 * the deliberate faults in the self-tests are the only expected ones. */
void trap_expect_next(void);

/* ESR_EL3 of the last expected trap, or 0 if none has been taken. */
uint64_t trap_last_esr(void);

/* ESR_ELx field accessors. Arm ARM (DDI 0487), "ESR_ELx, Exception Syndrome
 * Register". */
#define ESR_EC(esr)  ((uint32_t)(((esr) >> 26) & 0x3f))
#define ESR_IL(esr)  ((uint32_t)(((esr) >> 25) & 0x1))
#define ESR_ISS(esr) ((uint32_t)((esr) & 0x1ffffff))

#define ESR_EC_BRK         0x3c
#define ESR_EC_DATA_ABORT  0x25
#define ESR_DFSC(iss)      ((iss) & 0x3f)
#define ESR_DFSC_ALIGNMENT 0x21

#endif /* __ASSEMBLER__ */
#endif
