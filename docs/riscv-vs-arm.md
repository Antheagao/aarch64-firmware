# The same ideas in RISC-V and AArch64

Written alongside [xv6-riscv](https://github.com/Antheagao/xv6-riscv), where the same problems appear with different names.

The point is not that the two architectures are similar.
It is that privilege, traps, address translation, interrupts and atomics are problems every ISA has to answer, and seeing two answers makes the shape of the problem visible in a way one never does.

Where this firmware has not reached a topic yet, that is said rather than implied.

## Privilege levels

| RISC-V | AArch64 |
|---|---|
| M-mode | EL3 |
| S-mode | EL1 |
| U-mode | EL0 |
| HS-mode (H extension) | EL2 |

RISC-V names modes by what they are for: machine, supervisor, user.
AArch64 numbers them, and the number counts upward with privilege, which reads backwards the first time: EL3 is the most privileged, EL0 the least.

The deeper difference is that AArch64 splits privilege from security.
`SCR_EL3.NS` chooses a security state that is orthogonal to the exception level, so a non-secure EL1 and a secure EL1 are different execution contexts with separately banked registers.
RISC-V has no equivalent in the base ISA.
That banking is exactly what makes the ordering in `el-handoff.md` load-bearing.

## Dropping a privilege level

| RISC-V | AArch64 |
|---|---|
| `mret` | `eret` |
| `mstatus.MPP` selects the mode returned to | `SPSR_ELx.M[3:0]` selects the level and stack pointer |
| `mepc` holds the return address | `ELR_ELx` holds the return address |

Both are one-way, and both are "set up the saved state, then execute one instruction that installs it".
In xv6 this is how `usertrapret` returns to user code; here it is how the firmware enters the kernel.

## Why the trap cause registers differ in shape

| RISC-V | AArch64 |
|---|---|
| `scause` / `mcause` | `ESR_ELx.EC` |
| `stval` / `mtval` | `FAR_ELx` |
| `stvec` / `mtvec` | `VBAR_ELx` |

`scause` is one number: an interrupt bit plus a small exception code.
`ESR_ELx` is a packed syndrome: a 6-bit exception class in `EC`, an instruction-length bit, and 25 bits of `ISS` whose meaning depends on `EC`.

The practical consequence is that an AArch64 handler decodes in two stages.
`src/trap.c` reads `EC` first, then interprets `ISS` differently for an abort, where the fault status is in `DFSC`, than for a `BRK`, where it is a comment field.
A RISC-V handler switches once on `scause`.

Vectoring differs too.
`mtvec` can be a single entry point or a vectored table of jumps, four bytes per cause.
`VBAR_ELx` is always a table, always 2 KiB aligned, always sixteen entries of 128 bytes, grouped by whether the exception came from the current EL with `SP_EL0`, the current EL with `SP_ELx`, a lower EL in AArch64, or a lower EL in AArch32.
The CPU jumps *to* the entry, not through it, so each entry holds code rather than an address.
That is why `src/vectors.S` has room for a few instructions per slot and branches to a common path.

## Calling the more privileged level

| RISC-V | AArch64 |
|---|---|
| `ecall` | `svc`, `hvc`, `smc` depending on the target |
| SBI | PSCI, over SMCCC |

RISC-V has one instruction and infers the target from the current mode.
AArch64 has a different instruction per target level, so `smc` from EL1 goes to EL3 and `svc` goes to EL1 from EL0.

SBI and PSCI occupy the same position: a calling convention that lets the OS ask firmware for things it cannot do itself, such as starting another core or powering the system off.
This firmware answers `smc #0` as plumbing only; the real function table is M6.

One trap-handling detail is shared and easy to get wrong in both.
After `ecall`, RISC-V's `sepc` points *at* the `ecall`, so the handler must add 4 to resume.
After `smc`, AArch64's `ELR` points *after* the instruction, so adding 4 skips one.
Same idiom, opposite correction.

## Address translation

| RISC-V | AArch64 |
|---|---|
| `satp` | `TTBR0_EL1` and `TTBR1_EL1` |
| Sv39, three levels | 4 KiB granule, 39-bit VA, three levels |
| PTE permission bits | descriptor attributes plus `MAIR_ELx` |

Sv39 and the AArch64 4 KiB/39-bit configuration walk the same shape of tree.
The split is the visible difference: AArch64 has two base registers, so user and kernel mappings can live in separate halves of the address space without sharing a table, which is what makes `TTBR1_EL1` the natural home for a kernel.

Memory *type* is handled very differently.
RISC-V PTEs carry permissions, and memory type comes from the platform's physical memory attributes.
AArch64 descriptors carry an index into `MAIR_ELx`, which holds the actual memory types, so "this page is Device, that one is Normal write-back" is a property of the mapping.

This firmware has not reached any of it: the MMU is off at both EL3 and EL1, which is why every memory access so far is Device and why `-mstrict-align` is in the build flags.
M4 owns turning it on.

## Interrupts

| RISC-V | AArch64 |
|---|---|
| PLIC | GIC distributor |
| CLINT, software and timer interrupts | GIC redistributor and CPU interface |
| `mie` / `sie` | `ICC_IGRPEN1_EL1`, `ICC_PMR_EL1` |

The GIC is the more elaborate of the two, partly because it carries priority and grouping, and partly because GICv3 moved the CPU interface into system registers.
This firmware masks all of `DAIF` when it enters EL1 and has no interrupt handling at all yet; M5 owns the GIC and the generic timer.

## Atomics

| RISC-V | AArch64 |
|---|---|
| `lr.d` / `sc.d` | `ldxr` / `stxr` |
| A extension `amoadd` and friends | `FEAT_LSE`: `cas`, `swp`, `ldadd` |

Both ISAs offer a load-reserved/store-conditional pair and a set of single-instruction atomics, and in both the exclusive pair requires Normal memory rather than Device.
That is a real ordering constraint on this project: a spinlock cannot be written before the MMU exists, which is why M6's locks come after M4.

The xv6 spinlock is a test-and-set built on the A extension.
The AArch64 equivalent planned for M6 is a ticket lock, chosen for fairness under contention, using LSE atomics when `ID_AA64ISAR0_EL1.Atomic` reports them, which the M2 decoder already prints.

## What transfers

Register names and instruction mnemonics do not transfer, and are not worth memorising.
What transfers is the set of questions.

Which privilege level am I in, and what does that let me touch.
Where does a trap go, how do I find out why, and how do I resume.
What does the hardware leave UNKNOWN that software has to establish.
What is a request from below, and what is a fault.

Both projects are mostly answers to those four.
