# Firmware campaign: current queue

**Last updated:** 2026-10-05.
Batch 17 added the translation table builder on branch `feat/m4-page-tables`, with 33 host unit tests and nothing yet programmed into the MMU.
Next batch: row 1, program `MAIR_EL1`, `TCR_EL1` and `TTBR0_EL1` and turn the MMU on at EL1.

This file is the work queue for the `campaign-loop` skill in `.claude/skills/campaign-loop/SKILL.md`.
Milestone specs live in `docs/ROADMAP.md`; this file only tracks order and state.
Parked rows and their unblock conditions are in `notes/firmware-future.md`, and finished batch detail is archived in `notes/firmware-previous.md`.

## How a batch runs here

- One batch is one branch, one PR, and one merge.
  The branch name comes from the row, for example `fix/uart-poll-timeout`.
- Update this file in the same commit as the code that completes the row.
- Merge with a merge commit once CI is green, then start the next batch from the updated `main`.
- Each row's gate is the set of checks that ratify it.
  Every row also requires the full `make test` matrix: GCC and LLVM, `CPU=max` and `CPU=cortex-a57`.

## Queue

| # | Row | Scope | Gate |
|---|---|---|---|
| 1 | `feat(mmu)`: turn the MMU on at EL1 | Program `MAIR_EL1` (attr0 Normal write-back `0xff`, attr1 Device-nGnRnE `0x00`), `TCR_EL1` (T0SZ, IRGN/ORGN, SH, TG0, IPS) and `TTBR0_EL1`, then set `SCTLR_EL1.M/C/I` with the `dsb`/`isb`/`tlbi` sequence the Arm ARM requires | Check EL1 still reaches its exit with the MMU on; the barriers are the risk, so a hang here is the expected failure mode |
| 2 | `feat(mmu)`: per-section permissions | `.text` RX, `.rodata` R, `.data`/`.bss`/stack RW and XN, UART and GIC as Device. Align the EL1 linker sections to page boundaries so the permissions can differ | Check the printed map matches the linker symbols |
| 3 | `test(mmu)`: permission and execute-never self-tests | Write to `.rodata` and jump into `.data`, recovering from each | Check `EC=0x25 DFSC=0x0f` (permission fault, level 3) and `EC=0x21` (instruction abort); M4 marked done; tag `v0.4.0` |
| 4 | `build`: drop `-mstrict-align` for the EL1 image | Only once EL1 runs on Normal memory, and only for that image: the firmware keeps it while the MMU is off at EL3 | The matrix still passes with the flag removed from the EL1 build alone |
| 5 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Next batch plan (row 1)

- Branch: `feat/m4-mmu-on`.
- The builder exists and is tested, so this row is the register programming and the barriers, which is where a mistake becomes a hang rather than a message.
- `MAIR_EL1`: attr0 = `0xff` (Normal, inner and outer write-back non-transient), attr1 = `0x00` (Device-nGnRnE). The descriptors already carry those indices, so the two files have to agree and `include/pagetable.h` is where the agreement is written down.
- `TCR_EL1`: `T0SZ` = 25 for a 39-bit VA, `TG0` = 4 KiB, `IRGN0`/`ORGN0` write-back write-allocate, `SH0` inner shareable, `IPS` from `ID_AA64MMFR0_EL1.PARange`, which the M2 decoder already reads. Do not hardcode `IPS`: it is the one field that depends on the CPU.
- `TTBR0_EL1` gets `pt_root()`. `TTBR1_EL1` stays unused, since the map is identity and low.
- The sequence matters as much as the values: `dsb ish` after the table writes so they are visible to the table walker, `tlbi vmalle1`, `dsb ish`, then `isb`, then set `SCTLR_EL1.M`, then `isb` again. A missing barrier here usually works in QEMU and fails on hardware, so write the barrier comments for the hardware case.
- Keep the map minimal and correct rather than complete: the EL1 image, its stack, and the UART. The firmware's own flash and SRAM do not need mapping, because the MMU is only being turned on at EL1.
- Enable `SCTLR_EL1.C` and `.I` in the same step. Caches off with the MMU on is a configuration nobody wants and that hides bugs in the attributes.
- Gate: EL1 still prints and still exits 0 with the MMU on, plus a check of the enabled `SCTLR_EL1` read back. A hang is the expected failure mode, and the harness's 30 second timeout is what will catch it.

## Batch log

### Batch 17 (2026-10-06): translation table builder

- `src/pagetable.c` builds an identity map with a 4 KiB granule and a 39-bit VA, choosing the largest block that fits: a 1 GiB block at level 1, a 2 MiB block at level 2, otherwise 4 KiB pages. Tables come from a caller-provided pool by bump pointer, so there is no heap and the worst case is known at link time.
- Deliberately pure: it writes no system registers, which is what lets all 33 of its tests run on the host. That is the argument for doing it this way round, because on the target a wrong descriptor bit is a hang with nothing to read.
- The encodings worth testing, all of which are easy to get backwards: a level 3 leaf is `0b11`, the same value that means *table* at levels 1 and 2, where a leaf is `0b01`; the access flag must be set or the first access faults with no handler here to explain it; execute-never is two bits, and clearing only one leaves the memory executable from the other privilege level; and shareability is meaningful for Normal memory but ignored for Device.
- Errors are returned rather than walked past: a misaligned base, a range past the 39-bit VA space, an exhausted pool, and a region mapped inside an existing block, which is an overlap rather than a silent split.
- Nothing is programmed into the MMU this row, so nothing can hang. 75 unit checks in total now.
- Note for the next row: `src/pagetable.c` is in `SRCS`, so it is linked into the firmware as dead code until the EL1 image starts using it. Harmless, and it resolves when row 1 moves it into `KERNEL_SRCS`.

### Batch 16 (2026-10-06): refill the queue for M4

- No code. M3 merging and tagging `v0.3.0` met M4's unblock condition, so M4 moved out of `notes/firmware-future.md` into the queue as five PR-sized rows: build the tables, turn the MMU on, set per-section permissions, prove the faults, then drop `-mstrict-align` for the EL1 image.
- The split puts the page table builder first and keeps it pure, writing no system registers, so it can be unit-tested on the host before anything depends on it. A wrong descriptor bit shows up on the target as a hang with nothing to read, which is the worst kind of failure to debug and the easiest to prevent.
- Dropping `-mstrict-align` is its own row rather than a footnote on the permissions row, because it only becomes safe once EL1 is actually running on Normal memory, and only for that image: the firmware keeps the flag while the MMU is off at EL3.
- The user-gated row stays in the queue rather than being parked. It is not blocked on a condition that will arrive on its own; it needs the owner to click something.

### Batch 15 (2026-10-06): clang-tidy

- `make lint` runs `clang-tidy` over `src/` and `kernel/` at both `TRAP_EL` values with `WarningsAsErrors` on, and a CI job runs it pinned to `clang-tidy-18` for the same reason `clang-format` is pinned to 18.
- It analyses for the target rather than the host. Without `--target=aarch64-none-elf` the run produced four errors, all `unknown register name 'x0' in asm` from `src/semihost.c`: a host target simply does not have those registers. Linting freestanding code against the wrong target measures the wrong thing.
- `.clang-tidy` enables `bugprone`, `clang-analyzer`, `misc`, `performance`, `portability` and `readability` wholesale, and every exclusion carries its reason in the file. The bulk of the noise was `readability-braces-around-statements`, `readability-magic-numbers` and `readability-identifier-length`, none of which fit register-level firmware written to this project's own style.
- One finding was real and was fixed rather than suppressed: `bugprone-casting-through-void` on the loader's `(const uint64_t *)(const void *)` cast. The symbols are declared `uint64_t[]` now, which states the alignment the assembler already guarantees instead of laundering it past the compiler, and it let the "size is a multiple of 8" check become a structural property rather than a runtime one.
- `cppcheck` was split out into a parked row. It cannot be installed on this machine, so it could only be iterated through CI, and pairing it with work that is fully checkable locally would have meant blind pushes.
- Gate, verified rather than assumed: a clean tree exits 0, and a redundant expression makes `make lint` exit 2 with `misc-redundant-expression`.
- Learning, twice over now: the first negative test reported a false pass because the script that was supposed to insert the defect never matched, and nothing checked that it had. Every negative test in this repo must first prove it changed something. The second attempt asserted the anchor and caught a real propagation question immediately.

### Batch 14 (2026-10-06): the M3 write-ups

- `docs/el-handoff.md` documents every bit written in `src/handoff.c`, with the Arm ARM section titles rather than numbers, since numbers move between issues. It leads with the ordering rather than the values: with `FEAT_SEL2` the EL1 and EL2 registers are banked by security state, so `SCR_EL3.NS` has to be set before them, and getting it wrong produces no diagnostic at all.
- It also records what is deliberately *not* set, which is the part a reader cannot recover from the code: `SCR_EL3.IRQ/FIQ/EA` left clear because M5 owns routing, `SMD` left clear or the `smc` self-test would take an Undefined Instruction exception, and the whole MMU set left alone until M4.
- `docs/riscv-vs-arm.md` maps privilege, trap causes, vectoring, the call-upward instructions, translation, interrupts and atomics onto the xv6-riscv work. The useful pairing turned out to be `ecall` against `smc`: `sepc` points at the `ecall` and `ELR` points after the `smc`, so the same resume idiom needs opposite corrections, which is the bug batch 13 hit.
- Gate: checked mechanically rather than by eye. Every register macro the documents claim is set was confirmed present in `src/handoff.c`, and the two quoted values recomputed: `SCTLR_EL1` is 0x30D0080A and `SPSR_EL3` is 0x3C5. Documentation that disagrees with the code is worse than none.

### Batch 13 (2026-10-06): SMC from EL1, M3 done

- The EL1 kernel executes `smc #0`, which lands at EL3 in the lower-EL AArch64 synchronous vector, index 8, decoding as `EC=0x17`.
- The EL3 handler now distinguishes a request from a fault. A synchronous exception from a lower EL means EL1 called down on purpose, so EL3 reports it and steps over the `smc`, returning control; parking it, which is what an unexpected fault gets, would have hung the run.
- Guarded with `#if TRAP_EL == 3`, because the EL1 copy of the same file must not treat its own lower-EL vector that way.
- Deliberately plumbing only: the SMCCC argument convention and a real function table are M6's, with PSCI. The queue row said to resist implementing PSCI here, and that was the right call to keep.
- The expected-trap flag the previous plan flagged as a worry turned out not to collide: EL3's self-tests all run before the `eret`, so the flag is clear by the time an SMC can arrive. Worth remembering when M6 adds real SMC traffic alongside it.
- Learning: the first push failed all four boot jobs on `unused variable 'ec'`. The variable is only read inside the `#if TRAP_EL == 3` block, so the EL1 build had it unused, and the local gate compiled only `TRAP_EL=3` and never saw that configuration. A gate that checks one of two build configurations is checking half the code.
  `make syntax-check` now compiles every file at both levels. Verified by reintroducing the variable: the gate exits 2 and names `src/trap.c` under `TRAP_EL=1`, where before it passed.
- Learning, and the real content of this batch: the SMC arrived correctly the first time, and the bug was in the return. The handler advanced `ELR_EL3` by 4, copying the idiom the self-tests use, and EL1 then took a data abort with `DFSC=0x10` immediately after the call.
  Arm ARM (DDI 0487), "Exception return": for an exception taken from `SVC`, `HVC` or `SMC`, `ELR` already holds the address *after* the instruction, because the call completed. `BRK` is the opposite: it is a debug exception and `ELR` points at the `BRK` itself, which is why stepping over it needs the +4. Adding 4 to an SMC return skips a real instruction.
  The two cases sit four lines apart in the same function and want opposite handling, so the reason is written next to both rather than in a commit message nobody will reread.
- M3 is complete: the image is loaded into DRAM, EL1 is entered Non-secure with a known state, EL1 handles its own exceptions, and a call from EL1 reaches EL3 and returns.

### Batch 12 (2026-10-06): EL1 vectors, one crash reporter for both levels

- The row asked for a deliberate decision between reusing the M1 code and copying it. Reuse won, by parameterising: `src/vectors.S` and `src/trap.c` are built into both images with `-DTRAP_EL=3` for the firmware and `-DTRAP_EL=1` for the kernel. The two differ only in which banked syndrome registers they read, so a copy would have been two crash reporters drifting apart.
- The report now names the level it was taken at. That is not cosmetic: a fault at EL1 showing up in an EL3 report means it went to the wrong level, and the check asserts `trap: EL1 vector=4`, which is EL1's own current-EL-with-SPx vector rather than EL3's lower-EL one.
- `el3_trap` became `trap_handler` and `el3_trap_common` became `trap_common`, since neither is EL3-specific any more.
- The kernel installs `VBAR_EL1`, reads it back, and takes a deliberate `brk #0`. The trap frame header needed no change: the layout is the same at every exception level, which is why it was worth putting the offsets in a shared header in the first place.
- The host gates compile this code too, so `-DTRAP_EL=3` had to reach `HOST_CFLAGS` and `UNIT_CFLAGS`. The `#error` in `src/trap.c` made that a loud failure rather than a silent one.

### Batch 11 (2026-10-06): drop to EL1

- `src/handoff.c` configures the state EL1 inherits and erets into the image batch 10 loaded. `src/boot.S` gained `el3_eret_to`, because `eret` has no C spelling.
- The ordering was the real content, and the pre-seeded plan for this row had it backwards. It said to set `SCR_EL3.NS` last. With `FEAT_SEL2`, which `-cpu max` implements, `HCR_EL2` and `SCTLR_EL1` are banked by security state and `SCR_EL3.NS` selects the bank EL3 sees, so NS has to be set *first*: writing those registers before flipping NS configures the Secure copies and leaves the Non-secure ones UNKNOWN. The corrected reasoning is in the file, the roadmap and this log, because the failure it causes would be silent.
- `HCR_EL2.RW` is written only when `ID_AA64PFR0_EL1.EL2` says EL2 exists, using the M2 decoder. Writing it on a CPU without EL2 is not architecturally valid, and the check costs one branch.
- `SPSR_EL3` is EL1h with all four DAIF bits masked, so the kernel starts with interrupts off and chooses when to take them. `SCTLR_EL1` gets the same known-reset treatment as `SCTLR_EL3`, with the MMU and caches off and alignment checking on.
- The eret is one-way, so the run can no longer end at EL3: the EL1 image now calls `semihost_exit(0)` and `src/semihost.c` joins the EL1 link. Without that the harness would hang and fail on its 30 second timeout rather than on anything real.
- The security state is reported by EL3, not by the kernel. EL1 cannot read `SCR_EL3`, so a kernel printing "Non-secure" would be repeating what it was told rather than observing anything.
- Verified the constants before pushing: `SCTLR_EL1_RES1` computes to 0x30D00800, matching TF-A, and the `SPSR_EL3` value to 0x3C5.

### Batch 10 (2026-10-06): embed and load the EL1 image

- `kernel/` is a second image with its own linker script at `0x4000_0000`, its own entry point, and no flash/SRAM split: the firmware copies the whole thing to DRAM, so load and run addresses are the same and `.data` needs no relocation. `.bss` is `NOLOAD`, so it stays out of the binary and `kernel_start` zeroes it.
- It shares `src/uart.c` and `src/kprintf.c` with the firmware rather than duplicating them. The PL011 is at the same address either side of the hand-off, and a second copy of a driver is a second place for a bug.
- `src/kernel_image.S` carries the blob with `.incbin`, and `src/loader.c` copies it to `DRAM_BASE`. The copy is necessary rather than ceremonial: Non-secure EL1 cannot fetch from secure flash, which is exactly why TF-A's BL2 loads BL33 this way.
- The loader reads the image back and compares every word instead of trusting the loop. A copy into the wrong memory otherwise surfaces much later as a CPU executing rubbish with nothing to point at.
- The image is deliberately not entered yet. Keeping the load in its own row means a failure here is a copy bug and nothing else.
- The Makefile gained a second link step keyed by source path (`build/kernel/<path>.o`), so `src/uart.c` builds once per image without the two objects colliding, and `make kernel` builds the image alone.
- `make syntax-check` and `make format-check` now cover `kernel/*.c` too, so the new image is held to the same gates as the firmware.
- Gate: two new `CHECKS` lines, one for the embedded size and one for the verified copy, plus the full matrix.

### Batch 9 (2026-10-06): refill the queue for M3

- No code. M2 merging and tagging `v0.2.0` met M3's unblock condition, so M3 moved out of `notes/firmware-future.md` and into the queue as five PR-sized rows: load the EL1 image, drop to EL1, give EL1 its own vectors, handle `smc #0` at EL3, then the two write-ups.
- The `clang-tidy` and `cppcheck` row is also unblocked now that `.clang-format` is merged, so it joins the queue after M3 rather than staying parked with a condition that is already met.
- Added a parked follow-on row for turning `mte=on` on the machine and asserting MTE present, with the condition that M7 enables MTE first. Batch 8 deliberately did not assert it; recording why keeps that from reading as an oversight later.
- `notes/firmware-future.md` gained the rule that a parked row without an unblock condition is one nobody picks up again, because two of its rows had conditions written against row numbers that have since shifted.

### Batch 8 (2026-10-06): ID register decoder, M2 done

- `src/cpuid.c` decodes the ID registers through one table of `{name, register, shift, width, kind}` rows walked by one function, as `docs/CODING_STANDARDS.md` asks. Adding a feature is a row.
- The decode is split from the register reads on purpose: `cpuid_print` is pure and runs on the host, and only `cpuid_read` is `#ifdef __aarch64__`. That is what makes the field rules testable.
- 24 host unit tests cover the rules the Arm ARM warns about, because each one fails silently rather than loudly: a signed field where `0b1111` means absent and `0` means present, the 4K and 64K granule fields where `0b1111` means unsupported, and the 16K field where `0` means unsupported instead.
- `tests/run_tests.py` now applies per-CPU expectations. The same firmware is asserted to report SVE2, BTI and PAC present on `-cpu max` and absent on `-cpu cortex-a57`, which is the actual claim M2 makes.
- `kprintf` gained width support for `%s`, which it had parsed and ignored, so the feature table lines up. Covered by two new unit tests.
- MTE is asserted absent on `cortex-a57` only. The machine line does not set `mte=on`, so asserting it present on `max` would assert a QEMU default rather than a decode; M7 owns turning it on.
- Learning: the host compile gate caught `cpuid_read` being declared inside `#ifdef __aarch64__`, which left `main.c` calling an undeclared function on the host. The guard belongs on the definition, not the declaration.

### Batch 7 (2026-10-06): host unit tests

- `make unit` builds `tests/unit/` for the host with `-fsanitize=address,undefined -fno-sanitize-recover=all` and runs it, with a CI job alongside. Sanitizers are the reason to run on the host at all: QEMU gives us neither.
- `tests/unit/fake_uart.c` is the seam. It captures what `kprintf` writes instead of touching MMIO, so `src/kprintf.c` is compiled unchanged and the code under test is the code that ships.
- 18 checks over `kprintf`: every conversion, width and zero padding, `%d` at INT_MIN, `%s` with NULL, and the known limitations pinned deliberately, including that there is no `-` flag, which is the wart batch 5 tripped over.
- Gate: verified the runner fails rather than only passing. Flipping one expected string to `DEADBEEF` made `make unit` exit 2 and print `FAIL  %x prints lowercase hex` with `18 checks, 1 failures`; restoring it returned exit 0.
- Learning: `include/kprintf.h` carries `__attribute__((format(printf, 1, 2)))`, so GCC format-checks every call. That is valuable in the firmware and awkward in tests that pass malformed formats on purpose, so those formats go through a variable, which the checker cannot see, and the NULL argument is `volatile` so it cannot be folded away. One case also needed a dummy argument for `-Wformat-security`.

### Batch 6 (2026-10-06): alignment fault self-test, M1 done

- `selftest_unaligned` reads a deliberately misaligned `uint64_t`, which faults because `SCTLR_EL3.A` is set and the MMU is off, so all memory is Device. The address is built through `uintptr_t` and read through a `volatile` pointer so the compiler cannot assume alignment, fold the load away, or split it into byte accesses that would not fault.
- This exercises the abort path rather than the breakpoint path, so `FAR_EL3` and the DFSC decoding added last batch are now covered by a check: `EC=0x25` (data abort, same EL) with `DFSC=0x21` (alignment fault).
- M1 is complete: vectors installed, a known EL3 reset state, a trap frame, a syndrome decode, a register dump, and recovery from two different deliberate faults.
- Gate: two new `CHECKS` lines plus the full matrix, then `docs/ROADMAP.md` marks M1 done and `main` is tagged `v0.1.0`.
- Learning: the first attempt wrote the misaligned access in C, and CI split on compiler: clang faulted as intended, GCC did not. Under `-mstrict-align` GCC lowers the access to byte loads, which never fault, and the self-test then read the stale ESR from the earlier `brk` and reported `EC=0x3c`. Writing the load as a single `ldr` in inline assembly makes it the hardware's decision rather than the compiler's.
  This is exactly what the two-compiler matrix is for. A clang-only CI would have merged a test that proved nothing on the other toolchain.

### Batch 5 (2026-10-06): trap frame and ESR decode

- `include/trapframe.h` defines the frame twice over: byte offsets for `src/vectors.S` and a C struct for `src/trap.c`, with ten `_Static_assert`s tying them together so a layout mismatch is a build error instead of a crash inside the crash handler.
- `src/vectors.S` now saves x0-x30, `ELR_EL3`, `SPSR_EL3`, `ESR_EL3`, `FAR_EL3` and the vector index, calls the C handler with the frame, then restores and `eret`s. Each 0x80-byte entry only saves x0/x1 and its index before branching to the common path, because that is all a vector slot has room for.
- `src/trap.c` decodes EC, IL and ISS from table lookups named after the Arm ARM, prints `FAR_EL3` and the DFSC for aborts only, and dumps all 31 registers four per line.
- The handler recovers from an armed, expected fault by advancing `ELR_EL3` past the faulting instruction; anything unexpected is reported and parked.
- `selftest_brk` executes `brk #0`, which exercises the whole path at once: table installed, frame saved and restored, syndrome decoded, execution resumed.
- Gate: four new `CHECKS` lines, run by the PR's CI matrix.
- Learning: `kprintf` has no `-` flag, so a `%-2u` in the register dump would have printed literally as text rather than aligning. Caught by reading the parser in `kprintf.c` rather than by any tool, which is an argument for the planned host unit tests of its formatting.
- Learning: the host compile gate added last batch paid for itself immediately. It caught the C side of this batch, including the frame-layout asserts, before anything reached CI.

### Batch 4 (2026-10-06): host compile gate

- `make syntax-check` compiles every `src/*.c` with the host compiler and `-fsyntax-only`, plus a CI job that runs it. `-fsyntax-only` stops before assembly, so the AArch64 inline asm in `sysreg.h` is parsed as a string and a host compiler can check this code at all. Target-only flags are deliberately absent, since they are not valid for the host.
- Promoted ahead of the trap frame deliberately: batch 3 lost a full CI cycle to a plain C error, and this gate catches that class in seconds on a machine that cannot build the firmware. Recorded here because reordering a queue without saying why is how a queue stops being trustworthy.
- `docs/TESTING.md` gains the layer, with the limit stated: it checks syntax and semantics only and proves nothing about behavior, so it adds to the QEMU checks rather than replacing any.
- Gate: verified the target actually fails, which is the whole point of adding it. A file with a bad return statement and a file with an unterminated string literal each made `make syntax-check` exit 2; with both removed it exits 0.
- Learning: the first attempt at that negative test reported a false pass. The script meant to corrupt a string literal never matched its target, so it checked a healthy tree and `exit=0` looked like a broken gate. A negative test that does not first prove it changed something is not a test. The same shape of bug as the `run_tests.sh` that always printed success, which batch 1 replaced.

### Batch 3 (2026-10-05): EL3 known reset state and vector table

- `include/sysreg_bits.h`: assembly-safe field definitions for `SCTLR_EL3`, `SCR_EL3`, `CPTR_EL3` and the `VBAR_EL3` alignment, named after the Arm ARM. It uses the TF-A `UL()` trick so one header serves both `.S` and `.c`.
- `src/boot.S` now writes the EL3 control registers before anything else, because hardware resets many of their bits to UNKNOWN: `SCTLR_EL3` = RES1 | A | SA (MMU, caches and icache off), `SCR_EL3` = RES1 | RW (lower EL is AArch64, still Secure), `CPTR_EL3` = TFP (FP and SIMD trap, matching `-mgeneral-regs-only`).
- `src/vectors.S`: the 16-entry, 0x80-stride, 2 KiB-aligned table, each entry named so a disassembly says which exception fired. Every entry parks for now; row 1 of the queue turns it into a real reporter.
- `linker.ld` aligns `.text.vectors` to 2048 in the link as well, so the table's alignment does not rest on the assembler alone.
- New checks: `VBAR_EL3` is printed, equals the `vectors` symbol, is 2 KiB aligned, and `SCTLR_EL3.A`/`.SA` read back set.
- Verified the constants against Trusted Firmware-A's published values before pushing: `SCTLR_EL3_RES1` computes to 0x30C50830, `SCR_EL3` to 0x430, `CPTR_EL3` to 0x400.
- Gate: the four new `CHECKS` lines, run by the PR's CI matrix (GCC and LLVM, `CPU=max` and `CPU=cortex-a57`) plus the `clang-format` job.
  This machine has no cross toolchain, QEMU or root, so CI is the only place the matrix runs; the merge is the record that it passed.
- Learning: the first push failed all four boot jobs. A heredoc turned the `\n` escapes in the new `kprintf` calls into real newlines, so every C string literal was unterminated. `clang-format` passed anyway, which is the point: a formatter is not a compiler.
  Found that this machine does have a host `gcc` in WSL, and `gcc -fsyntax-only -Wall -Wextra -Werror -ffreestanding -Iinclude src/*.c` catches exactly this class of error with no cross toolchain, because it stops before the AArch64 inline asm is assembled. Added as queue row 3.
- Learning: this closes the defect the M0 code documented in a TODO. A fault used to vector to offset 0x200 from a zero `VBAR_EL3`, which is the middle of `kprintf`, so the firmware's own crash path ran into unrelated code.

### Batch 2 (2026-10-05): clang-format and a CI format gate

- Added `.clang-format` encoding the Formatting section of `docs/CODING_STANDARDS.md`: 4-space indent, 100 columns, Linux braces, aligned macro columns and trailing comments.
- Added `make format` and `make format-check`, and a CI `clang-format` job pinned to `clang-format-18`, the version Ubuntu 24.04 ships, because output differs between major versions.
- Reformatted `src/*.c` and `include/*.h` in a commit of its own so the mechanical diff stays reviewable.
- Set `AlignEscapedNewlines: Left` after seeing the default right-alignment pad the `read_sysreg` continuations out to column 100 and bury the macro in whitespace.
- Assembly is deliberately out of scope: clang-format has no AArch64 asm support, so `src/*.S` stays under the review rules in `docs/CODING_STANDARDS.md`.
- Gate: `make format-check` clean locally with clang-format 18.1.8, and CI runs it on every push and PR.
- Learning: the machine running this batch had no cross toolchain, QEMU or root, so `make test` could not run locally; the PR's CI matrix was the gate instead.

### Batch 1 (2026-10-05): bounded UART polling

- Added `include/mmio.h` with `mmio_read32`, `mmio_write32`, and `mmio_poll_clear32`, which gives up after a fixed number of reads.
- Both PL011 waits (`BUSY` in `uart_init`, `TXFF` in `uart_putc`) now use it with a budget of 1,000,000 reads.
  After the first TX timeout the UART latches as dead and drops characters at once, and `uart_tx_timeouts()` counts every timeout and drop.
- Removed the one-level recursion in `uart_putc`, which the coding standards forbid.
- New checks: the poll helper gives up on a stuck bit, returns at once on a clear bit, and the boot ends with `uart: tx timeouts = 0`.
- Gate: the old unbounded loop was put back temporarily and made the harness fail with a QEMU timeout; with the fix, all 8 checks pass on GCC and LLVM, `CPU=max` and `CPU=cortex-a57`.
- Learning: QEMU's PL011 never sets `BUSY` or `TXFF`, so UART timeouts cannot be triggered on QEMU.
  Timeout paths are tested against a memory word that never clears instead.

### Batch 0 (2026-10-05): project setup

- Merged PR #1: M0 boot from the EL3 reset vector, UART, QEMU test harness, CI matrix.
- Merged PR #2: `CLAUDE.md`, contributing rules, PR template.
- Merged PR #3: architecture, coding standards, testing, and performance guides.
- Merged PR #4: campaign queue and the `campaign-loop` skill.
