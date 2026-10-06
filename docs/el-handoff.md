# The EL3 to EL1 hand-off

What `src/handoff.c` writes before `eret`, and why each bit is there.
Every register field below is named as the *Arm Architecture Reference Manual for A-profile* (DDI 0487) names it.
Section titles are cited rather than numbers, because numbers move between issues.

This is M3 of `ROADMAP.md`.
The firmware boots at EL3, loads an image into non-secure DRAM, then hands control to it at EL1.

## Order matters more than the values

The temptation is to configure EL1, configure EL2, then flip to non-secure last, as if `SCR_EL3.NS` were a switch you throw on the way out the door.
On this machine that is wrong, and it fails silently.

`-cpu max` implements `FEAT_SEL2`, Secure EL2.
When Secure EL2 exists, the EL1 and EL2 registers are *banked by security state*: there is a Secure `SCTLR_EL1` and a non-secure `SCTLR_EL1`, and likewise for `HCR_EL2`.
Which bank EL3 sees when it writes them is selected by `SCR_EL3.NS`.

So `SCR_EL3.NS` is set **first**.
Only then are `HCR_EL2` and `SCTLR_EL1` written, because only then do those writes land in the bank EL1 will actually run with.
Doing it the other way configures the Secure copies, leaves the non-secure ones UNKNOWN, and produces no diagnostic at all: the `eret` succeeds and EL1 starts executing with whatever those bits happened to reset to.

An `isb` separates the `SCR_EL3` write from the writes that depend on it, so the banked view has changed before they are issued.

Note that EL3 itself stays Secure throughout.
`SCR_EL3.NS` describes the lower exception levels, not the one doing the writing, so the firmware keeps running and keeps printing over the same UART after the flip.

## SCR_EL3, Secure Configuration Register

Written as `SCR_EL3_RES1 | SCR_EL3_RW | SCR_EL3_NS`.

| Field | Value | Why |
|---|---|---|
| `NS`, bit [0] | 1 | Lower exception levels are non-secure. Also selects the non-secure bank for the register writes that follow. |
| `RW`, bit [10] | 1 | The next lower exception level is AArch64. With this clear, the `eret` would drop to AArch32. |
| bits [5:4] | RES1 | The architecture requires them written as 1. |

Left at 0 deliberately:

- `IRQ`, `FIQ`, `EA`, bits [3:1], so interrupts and external aborts are **not** routed to EL3. M5 owns interrupt routing, and this firmware has no interrupt handling yet.
- `SMD`, bit [7], so `SMC` is **not** disabled. Setting it would make the `smc #0` self-test take an Undefined Instruction exception instead of reaching EL3.
- `HCE`, bit [8], since nothing issues `HVC`.

## HCR_EL2, Hypervisor Configuration Register

Written as `HCR_EL2_RW`, and only when `ID_AA64PFR0_EL1.EL2` reports EL2 implemented.

| Field | Value | Why |
|---|---|---|
| `RW`, bit [31] | 1 | Non-secure EL1 is AArch64. |

Two things are worth stating plainly.

First, the firmware never executes at EL2 and never intends to, but `HCR_EL2` still applies to non-secure EL1.
Skipping an exception level does not skip its controls.

Second, the write is guarded by a feature check rather than assumed.
Accessing `HCR_EL2` on a CPU without EL2 is not architecturally valid, and the ID register is the only honest way to ask.
The check uses the decoder from M2, which is the first time that milestone pays for itself.

## SCTLR_EL1, System Control Register (EL1)

Written as `SCTLR_EL1_RES1 | SCTLR_EL1_A | SCTLR_EL1_SA`, which is `0x30D0080A`.

| Field | Value | Why |
|---|---|---|
| `M`, bit [0] | 0 | MMU off. M4 turns it on. |
| `A`, bit [1] | 1 | Alignment checking on. With the MMU off all memory is Device, where unaligned access faults anyway; this makes the fault explicit and matches the `-mstrict-align` build. |
| `C`, bit [2] | 0 | Data cache off while the MMU is off. |
| `SA`, bit [3] | 1 | Stack alignment checking on, so a misaligned SP faults at the point of use. |
| `I`, bit [12] | 0 | Instruction cache off. |
| bits [29:28], [23:22], [20], [11] | RES1 | Required by the architecture. |

The reason this is written at all is the same reason `SCTLR_EL3` is written at reset: the architecture leaves many of these bits UNKNOWN out of reset.
Firmware that does not write them is not inheriting zeroes, it is inheriting whatever the implementation chose, which may differ between silicon revisions.

## SPSR_EL3: the PSTATE that `eret` installs

Written as `SPSR_EL3_M_EL1H | SPSR_EL3_D | SPSR_EL3_A | SPSR_EL3_I | SPSR_EL3_F`, which is `0x3C5`.

| Field | Value | Why |
|---|---|---|
| `M[3:0]` | `0b0101` | EL1h: EL1 using `SP_EL1` rather than `SP_EL0`. |
| `F`, bit [6] | 1 | FIQ masked. |
| `I`, bit [7] | 1 | IRQ masked. |
| `A`, bit [8] | 1 | SError masked. |
| `D`, bit [9] | 1 | Debug exceptions masked. |

All four of `DAIF` are masked so EL1 starts with interrupts off and decides for itself when to take them.
Handing a kernel an exception level with interrupts already live, before it has installed `VBAR_EL1`, is a way to take an interrupt into a vector table that does not exist yet.

`ELR_EL3` holds the entry point, `0x4000_0000`, and `src/boot.S` provides `el3_eret_to` because `eret` has no C spelling.

## What EL1 is not given

`CNTHCTL_EL2` and the timer registers are untouched: M5 owns the generic timer.
`VBAR_EL1` is deliberately left to the kernel rather than set by the firmware, because the vector table lives in the kernel's own image and only the kernel knows its address.
`TTBR0_EL1`, `TCR_EL1` and `MAIR_EL1` are untouched because the MMU stays off until M4.

## Who reports the security state

EL3, as `el3: entering EL1 (Non-secure)`.

EL1 cannot read `SCR_EL3`.
A kernel printing "non-secure" would be repeating what it was told rather than observing anything, so the claim is made by the only exception level in a position to know it.

## The return path

`eret` is one-way.
EL3 does not get control back unless a lower level calls down, so the firmware says everything it needs to say before the `eret`, and the EL1 image is what ends the QEMU run through semihosting.

When EL1 does call down with `smc #0`, EL3 reports it and returns **without** advancing `ELR_EL3`.
That is the opposite of what the `brk` self-tests do, and the distinction is in the Arm ARM under "Exception return": for an exception taken from `SVC`, `HVC` or `SMC`, `ELR` already holds the address *after* the instruction, because the call completed.
`BRK` is a debug exception and `ELR` points at the `BRK` itself, so resuming past it requires the +4.
Adding 4 to an SMC return skips a real instruction; it cost a CI cycle to find, and EL1 came back as a data abort with `DFSC=0x10` immediately after the call.
