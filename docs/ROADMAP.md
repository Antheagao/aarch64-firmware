# Roadmap

This file is the spec for each milestone: scope, the registers involved, and the "done when" test.
The work queue that tracks progress batch by batch lives in `notes/firmware-current.md`.

Each milestone ends with new lines in the `CHECKS` list in `tests/run_tests.py`.
That way CI proves on every push that every earlier milestone still works.
Estimates assume about 10 hours a week.

The primary reference is the *Arm Architecture Reference Manual for A-profile architecture* (DDI 0487, issue M.d or later, which covers up to Armv9.6).
For a gentler start, read the matching guide in Arm's free *Learn the architecture* series on developer.arm.com before each milestone.

| ID | Milestone | Est. | Status |
|---|---|---|---|
| M0 | [Reset vector, C runtime, UART, CI](#m0-reset-vector-c-runtime-uart-ci) | - | Done |
| M1 | [EL3 exceptions and crash reporter](#m1-el3-exceptions-and-crash-reporter) | 1 wk | Done |
| M2 | [CPU feature discovery](#m2-cpu-feature-discovery) | 3 days | Done |
| M3 | [EL3 to EL1 hand-off](#m3-el3-to-el1-hand-off) | 1 wk | Done |
| M4 | [MMU and caches](#m4-mmu-and-caches) | 1-2 wk | Done |
| M5 | [GICv3 and the generic timer](#m5-gicv3-and-the-generic-timer) | 1 wk | Not started |
| M6 | [PSCI and multi-core bring-up](#m6-psci-and-multi-core-bring-up) | 1-2 wk | Not started |
| M7 | [Armv9 feature enablement: SVE2, PAC/BTI, MTE](#m7-armv9-feature-enablement-sve2-pacbti-mte) | 2 wk | Not started |
| M8 | [Performance measurement with the PMU](#m8-performance-measurement-with-the-pmu) | 1 wk | Not started |
| S1 | [Stretch: boot Linux on this firmware](#s1-stretch-boot-linux-on-this-firmware) | 2+ wk | Not started |

---

## M0: Reset vector, C runtime, UART, CI

Done.
`src/boot.S` parks the secondary CPUs, sets up the stack, copies `.data` from flash to SRAM, and zeroes `.bss`.
`src/uart.c` is a polled PL011 driver.
`semihost_exit()` gives CI a real exit status.

Before moving on, be able to explain every line of `boot.S` and `linker.ld`.
In particular, know why `.data` has different load and run addresses, and why the build uses `-mstrict-align` and `-mgeneral-regs-only`.

## M1: EL3 exceptions and crash reporter

Before this milestone `VBAR_EL3` still held its reset value, 0.
A synchronous exception at EL3 vectors to offset 0x200 from it, which was the middle of `kprintf`, so a crash looked like garbage output or a hang.
You can see the old behavior in GDB with `p/x $VBAR_EL3` and `info symbol 0x200`.
This is fixed first, because every later milestone depends on readable crashes.

- [x] Write `src/vectors.S`: a 2 KiB-aligned table of 16 entries, 0x80 bytes each.
  The four groups are current EL with SP0, current EL with SPx, lower EL AArch64, and lower EL AArch32, each with sync, IRQ, FIQ and SError entries.
  Install it with `VBAR_EL3`.
- [x] At reset, put `SCTLR_EL3`, `SCR_EL3` and `CPTR_EL3` into a known state, because real hardware resets many of their bits to UNKNOWN values.
  Turn on alignment checking with `SCTLR_EL3.A`.
- [x] On entry, save x0-x30, `ELR_EL3` and `SPSR_EL3` into a trap frame on the stack, then call a C handler.
- [x] Decode `ESR_EL3`: EC (exception class), IL, and ISS.
  For aborts, also decode DFSC and print `FAR_EL3`.
  Print a register dump.
- [x] Add a self-test that triggers `brk #0` and an unaligned load, recovers from each by advancing `ELR_EL3`, and continues.

**Done when** the harness sees `EC=0x3c` (BRK) and `EC=0x25` with `DFSC=0x21` (alignment fault), and the firmware still reaches the end of the boot.

## M2: CPU feature discovery

Before firmware can enable a feature, it has to find out whether the CPU has it.
That is what the ID registers are for.

- Read and decode `ID_AA64PFR0_EL1`, `ID_AA64PFR1_EL1`, `ID_AA64ISAR0/1/2_EL1`, `ID_AA64MMFR0/1/2_EL1`, `ID_AA64DFR0_EL1`, and `ID_AA64ZFR0_EL1` (only when SVE is present).
- Print a feature table covering EL2/EL3, AdvSIMD, SVE/SVE2, SME, MTE level, PAC (APA/API), BTI, RME, SPE, AMU, PMU version, PA range, and the supported translation granules.
- Learn the ID field rules from the Arm ARM rather than guessing, including signed versus unsigned fields and what `0b1111` means.

**Done when** `CPU=max` reports SVE2, PAC, BTI and MTE, and `CPU=cortex-a57` reports them absent.
CI already boots both CPUs, so check both.
Also note what QEMU reports for MTE with and without `mte=on` on the machine.

Done.
`src/cpuid.c` decodes the ID registers through one table, and `tests/unit/test_cpuid.c` checks the field rules against known values on the host.
`tests/run_tests.py` now applies per-CPU expectations, so the same firmware is asserted to report SVE2, BTI and PAC present on `-cpu max` and absent on `-cpu cortex-a57`.
MTE is asserted absent on `cortex-a57` only: the machine line does not set `mte=on`, so asserting it present on `max` would be asserting a QEMU default rather than a decode. Turning `mte=on` on and checking it belongs to M7, which is where MTE is enabled.

## M3: EL3 to EL1 hand-off

This is what boot firmware is for: set up the lower exception levels and hand off to the next image.

- [x] Build a small EL1 kernel linked at `0x4000_0000` (non-secure DRAM).
  Embed it in the firmware image with `.incbin`.
  Firmware copies it to DRAM, the way TF-A BL2 loads BL33.
  Non-secure EL1 cannot fetch from secure flash, so it has to be copied.
- [x] Configure `SCR_EL3` (NS, RW, and the trap controls you need), `HCR_EL2.RW`, the reset state of `SCTLR_EL2` and `SCTLR_EL1`, and timer access in `CNTHCTL_EL2`.
  Set `SPSR_EL3` to EL1h with interrupts masked, put the entry point in `ELR_EL3`, then `eret`.
  Even if you skip EL2, its controls still apply to non-secure EL1, and finding that out is part of the exercise.

  The ordering turned out to be the real content.
  With `FEAT_SEL2`, which `-cpu max` implements, `HCR_EL2` and `SCTLR_EL1` are banked by security state and `SCR_EL3.NS` selects which bank EL3 sees.
  So `SCR_EL3.NS` is set *first* and those registers are written afterwards; doing it the other way configures the Secure copies and leaves the Non-secure ones UNKNOWN.
  `CNTHCTL_EL2` is not needed until M5 introduces the timer.
- [x] Give EL1 its own vector table (`VBAR_EL1`) and crash reporter.
  Most of the M1 code should be reusable.
  It was, by parameterising rather than copying: `src/vectors.S` and `src/trap.c` are built into both images with `TRAP_EL` selecting the banked syndrome registers and the level the report names.
- [x] Make an `smc #0` from EL1 land in the EL3 handler.
  EL3 treats a synchronous exception from a lower EL as a request rather than a fault: it reports it and returns, instead of parking as it does for an unexpected fault.
  The SMCCC argument convention and a real function table are M6's, with PSCI.

**Done when** the harness sees `kernel: running at EL1` and an EL3 trap report with `EC=0x17` (SMC from AArch64).

The security state is reported by EL3 rather than by the kernel, as `el3: entering EL1 (Non-secure)`.
EL1 cannot read `SCR_EL3`, so a kernel claiming to be Non-secure would be repeating what it was told rather than observing anything.

**Write-ups:**
- [`docs/el-handoff.md`](el-handoff.md) explains every bit set in `SCR_EL3`, `HCR_EL2`, `SCTLR_EL1` and `SPSR_EL3`, and why, with the register banking order that makes the sequence load-bearing.
- [`docs/riscv-vs-arm.md`](riscv-vs-arm.md) maps the same ideas onto RISC-V from the xv6-riscv work.
  Cover M/S/U modes vs EL3/EL1/EL0, SBI vs PSCI, `scause` vs `ESR_ELx.EC`, `stvec` vs `VBAR_EL1`, `satp` vs `TTBRn_EL1`, PLIC vs GIC, and LR/SC vs LDXR/STXR.

## M4: MMU and caches

- At EL1, use a 4 KiB granule, a 39-bit VA, and an identity map.
  Program `MAIR_EL1` (attr0 Normal write-back = `0xff`, attr1 Device-nGnRnE = `0x00`), `TCR_EL1` (T0SZ, IRGN/ORGN, SH, TG0, IPS), and `TTBR0_EL1`.
  Then set `SCTLR_EL1.M/C/I` with the correct `dsb`/`isb`/`tlbi` sequence.
- Map `.text` as RX, `.rodata` as R, `.data`/`.bss`/stack as RW + XN, and the UART and GIC as Device.
  Align the linker script sections to page boundaries so the permissions can differ.
- Once EL1 runs with the MMU on and Normal memory, drop `-mstrict-align` for the EL1 image.
- Optional: turn on the MMU at EL3 as well, as TF-A does.

**Done when** writing to `.rodata` produces `EC=0x25 DFSC=0x0f` (permission fault, level 3) and jumping into `.data` produces `EC=0x21` (instruction abort).
Both must be caught and reported, not hang.

Done.
Recovering from the instruction abort needed a rule the other self-tests do not: `ELR` points at the address that could not be fetched, so advancing it by 4 stays inside non-executable memory and the fault repeats.
The faulting fetch was a call, so the handler returns to `x30` instead, which makes a failed call behave like one that returned.

Dropping `-mstrict-align` for the EL1 image is tracked as its own queue row, since it only becomes safe now that EL1 runs on Normal memory.

## M5: GICv3 and the generic timer

- At EL3, set `ICC_SRE_EL3` (SRE and Enable) so lower ELs can use the system register CPU interface, and route IRQs with `SCR_EL3`.
- Distributor: set `GICD_CTLR` with ARE and Group 1 NS enabled.
- Redistributor: wake it by clearing `GICR_WAKER.ProcessorSleep`, wait for `ChildrenAsleep` to clear, and configure the PPI in the SGI frame.
- CPU interface: set `ICC_PMR_EL1` and `ICC_IGRPEN1_EL1`.
  Acknowledge interrupts with `ICC_IAR1_EL1` and finish them with `ICC_EOIR1_EL1`.
- Timer: use the EL1 physical timer (`CNTP_*`, PPI INTID 30) and `CNTFRQ_EL0` for the tick rate.

**Done when** EL1 prints 10 ticks at 100 Hz, and the elapsed `CNTPCT_EL0` matches 0.1 s within a tolerance the harness checks.

## M6: PSCI and multi-core bring-up

This is the part of firmware the OS talks to.
Read the PSCI specification (DEN 0022) and the SMC Calling Convention (DEN 0028) first.

- Write an EL3 SMC handler that follows SMCCC: function ID in w0, arguments in x1-x3, result in x0.
- Implement `PSCI_VERSION` (0x84000000), `PSCI_FEATURES` (0x8400000A, required for PSCI 1.x), `CPU_ON` (0xC4000003), `CPU_OFF` (0x84000002), `AFFINITY_INFO` (0xC4000004), and `SYSTEM_OFF` (0x84000008).
  Use the return codes from the spec.
  `SYSTEM_OFF` can use semihosting exit for now.
- Secondaries currently park in `boot.S`.
  Give each one a mailbox holding an entry point and a context ID.
  `CPU_ON` fills in the mailbox and runs `sev`.
  The secondary configures its own EL3 state, drops to EL1 at the requested entry point, and arrives with x0 set to the context ID.
- At EL1, give each CPU its own stack and exception stack.
- Write a spinlock with `LDAXR`/`STLXR`, plus an LSE version (`CASA`/`SWPAL`) used when the ID registers report `FEAT_LSE`.
  Exclusives need Normal memory, which is why this milestone comes after the MMU.
- Repeat the xv6 `threadtest` experiment: 4 CPUs increment a shared counter, with and without the lock.

**Done when** the harness sees `cpu 1 online`, `cpu 2 online` and `cpu 3 online` with their MPIDRs, sees the exact locked counter value, and sees `PSCI_VERSION` return the version you implemented.

## M7: Armv9 feature enablement: SVE2, PAC/BTI, MTE

This milestone is the closest match to CPU feature enablement work in industry.
For each feature:
- Detect it (M2).
- Enable it at every exception level that gates it.
- Show it working.
- Show it catching a bug.
- Document the control bits.

Skip each feature cleanly when the CPU does not have it.

**SVE / SVE2**
- Clear the traps: `CPTR_EL3.EZ`, `CPTR_EL2`, and `CPACR_EL1.ZEN` and `FPEN`.
  Set the vector length with `ZCR_EL3/EL2/EL1.LEN`.
- Read the vector length back with `rdvl`.
- Build one file with `-march=armv9-a+sve2` and without `-mgeneral-regs-only`.
  Run a vector-length-agnostic loop, and show the same binary producing correct results at 128, 256 and 512 bits.

**PAC and BTI**
- Build the EL1 image with `-mbranch-protection=standard`.
- Set `SCR_EL3.API/APK` and `HCR_EL2.API/APK` so the instructions and key registers do not trap.
  Program `APIAKeyLo/Hi_EL1` and set `SCTLR_EL1.EnIA`.
- For BTI, set the GP (guarded page) bit in the stage 1 descriptors for code pages.
  That bit is what turns enforcement on.
  Also read what `SCTLR_EL1.BT1` changes about `PACIASP` as a landing pad.
- Demo: corrupting a saved LR on the stack is caught on return (`EC=0x1c`, FPAC).
  An indirect branch to a function with no BTI landing pad is caught with `EC=0x0d`.

**MTE**
- Add `mte=on` to the machine.
  Set `SCR_EL3.ATA`, `HCR_EL2.ATA`, `SCTLR_EL1.ATA`, `SCTLR_EL1.TCF` (synchronous mode), and `TCR_EL1.TBI0`.
- Map the heap as Tagged Normal (MAIR attr `0xf0`).
  Set up `GCR_EL1` and `RGSR_EL1` for `irg`.
- Demo: tag a 64-byte buffer with `irg`/`stg`, write one granule past its end, and catch the synchronous tag check fault (`EC=0x25`, `DFSC=0x11`).

**Done when** the harness checks each "caught" line on `CPU=max` and each "not present, skipped" line on `CPU=cortex-a57`.

**Write-up:** `docs/feature-enablement.md`, a table of every control bit per exception level per feature.
If you show one file in an interview, make it this one.

## M8: Performance measurement with the PMU

- Open PMU access: `MDCR_EL3`, `MDCR_EL2.HPMN`, `PMCR_EL0.E`, and `PMCNTENSET_EL0`.
  Count cycles with `PMCCNTR_EL0` and instructions retired (event `0x08`) with `PMEVTYPERn_EL0`/`PMEVCNTRn_EL0`.
- Benchmark memcpy four ways: byte loop, 64-bit loads and stores, `LDP`/`STP` pairs, and SVE.
  Compare instruction counts.
  Run QEMU with `-icount shift=0`, because without it QEMU does not support the instructions-retired event (check `PMCEID0_EL0`).
- Be honest about QEMU's limits: TCG does not model pipeline timing, so its cycle counts mean little.
  Run the same four kernels as a Linux user-space program on real Arm hardware under `perf stat`.
  An AWS Graviton4 instance (Neoverse V2, Armv9 with SVE2) or a Raspberry Pi 5 both work.
  Report IPC, cache misses, and speedup.

**Done when** `docs/perf.md` has a table with QEMU instruction counts and real-hardware `perf stat` numbers, plus a paragraph explaining the differences.
`docs/PERFORMANCE.md` describes the method.

## S1: Stretch: boot Linux on this firmware

Show that the firmware does a real firmware's job by booting an unmodified arm64 Linux kernel.

- Get the board's device tree with `-machine dumpdtb=virt.dtb`.
  Add a `psci` node with `method = "smc"`.
  With a firmware image loaded, QEMU turns off its own PSCI, so Linux has to use yours.
- Load `Image` and the DTB into DRAM with `-device loader`.
  Enter the kernel at EL2 with the MMU off and x0 set to the DTB address, following `Documentation/arch/arm64/booting.rst`.

**Done when** Linux prints `smp: Brought up 1 node, 4 CPUs`, with the secondaries started through your `CPU_ON`.

---

## How the milestones map to industry CPU firmware work

| Skill | Where this repo shows it |
|---|---|
| ARM CPU architecture fundamentals, ARMv8/v9 ISA | M1, M3, M4, M5 |
| Latest Armv9 architecture features | M2, M7 |
| Firmware development and CPU feature enablement | M0, M3, M6, M7 |
| C and ARM assembly | Throughout: `boot.S`, `vectors.S`, SVE kernels |
| Performance evaluation and optimization on ARM | M8 |
| Open-source development tools | GCC and LLVM builds, QEMU, GDB, Make, GitHub Actions |
| Analytical and debugging skills | M1 crash reporter, GDB workflow, CI assertions |
| RISC-V fundamentals | `docs/riscv-vs-arm.md`, next to xv6-riscv |
