# Firmware campaign: current queue

**Last updated:** 2026-10-05.
Batch 9 refilled the queue: M2 shipped and tagged `v0.2.0`, which unblocked M3, so M3 is split into PR-sized rows below.
Next batch: row 1, embed an EL1 image in the firmware and copy it to DRAM.

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
| 1 | `feat(el1)`: embed an EL1 image and copy it to DRAM | A minimal second image linked at 0x4000_0000, embedded with `.incbin`, copied to DRAM by the firmware the way TF-A BL2 loads BL33 | Check that the copied image's first words match the embedded ones; no EL change yet |
| 2 | `feat(el3)`: drop to EL1 | `SCR_EL3` (NS, RW), `HCR_EL2.RW`, reset state for `SCTLR_EL1`, `SPSR_EL3` = EL1h with interrupts masked, entry in `ELR_EL3`, then `eret` | Check `kernel: running at EL1 (Non-secure)` |
| 3 | `feat(el1)`: EL1 vector table and crash reporter | Reuse the M1 trap frame and decode behind `VBAR_EL1`; a deliberate fault at EL1 is reported by EL1, not EL3 | Check an EL1 trap report, and that the boot continues |
| 4 | `feat(el3)`: handle `smc #0` from EL1 | EL3 synchronous handler for the lower-EL AArch64 vector, decoding `EC=0x17` | Check an EL3 trap report with `EC=0x17`; tag `v0.3.0` |
| 5 | `docs`: `el-handoff.md` and `riscv-vs-arm.md` | Every bit set in `SCR_EL3`, `HCR_EL2` and `SPSR_EL3` and why; then map the same ideas onto RISC-V from the xv6-riscv work | Both files exist and the roadmap links them |
| 6 | `ci`: `clang-tidy` and `cppcheck` job | Static analysis over `src/` and `include/`, with the checks that fire on this code either fixed or explicitly disabled with a reason | New CI job passes, and a deliberately introduced defect makes it fail |
| 7 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Next batch plan (row 1)

- Branch: `feat/m3-el1-image`.
- Build a second, minimal image linked at `0x4000_0000` (non-secure DRAM) with its own linker script and entry point, printing one line over the same PL011 so it is visible without any EL1 UART work.
- Embed it in the firmware with `.incbin` and copy it to DRAM at boot, the way TF-A BL2 loads BL33. The copy is necessary, not ceremonial: non-secure EL1 cannot fetch from secure flash.
- Keep this row to the load. No exception level change yet, so a failure here is a copy bug and nothing else.
- Watch the build: two images in one repo means a second link step and a second set of flags, so keep the Makefile honest rather than adding a special case for every file.
- Gate: a check that the first words at the DRAM address match the embedded image, plus the full matrix.

## Batch log

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
