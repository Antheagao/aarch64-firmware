# Firmware campaign: current queue

**Last updated:** 2026-10-05.
Batch 3 put the EL3 control registers into a known reset state and installed the vector table in `VBAR_EL3` on branch `feat/m1-el3-reset-state`.
Next batch: row 1, save a trap frame and decode `ESR_EL3` so a crash prints EC, IL, ISS and a register dump (M1, part 2).

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
| 1 | `feat(el3)`: trap frame and ESR decode (M1, part 2) | Save x0-x30, `ELR_EL3`, `SPSR_EL3`; decode EC, IL, ISS, DFSC; print `FAR_EL3` and a register dump | `brk #0` self-test reports `EC=0x3c` and boot continues |
| 2 | `test(el3)`: alignment fault self-test (M1, part 3) | Unaligned load recovered by advancing `ELR_EL3` | Check `EC=0x25 DFSC=0x21`; M1 marked done in `docs/ROADMAP.md`; tag `v0.1.0` |
| 3 | `build`: host syntax-check target | `make syntax-check` runs `gcc -fsyntax-only -Wall -Wextra -Werror` over `src/*.c` on the host, so C errors are caught without a cross toolchain | New CI job, and the target fails on a deliberately broken string literal |
| 4 | `test`: host unit test scaffolding | `tests/unit/` runner, `make unit`, built with ASan and UBSan; first tests cover `kprintf` formatting | `make unit` passes locally and in a new CI job |
| 5 | `feat(cpu)`: table-driven ID register decoder (M2) | Decode the ID registers listed in the roadmap into a printed feature table | Checks: SVE2, PAC, BTI, MTE present on `max` and absent on `cortex-a57`; tag `v0.2.0` |
| 6 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Next batch plan (row 1)

- Branch: `feat/m1-trap-frame`.
- Replace the `b el3_park` stub in each `vector_entry` with a save sequence: push x0-x30 plus `ELR_EL3` and `SPSR_EL3` onto the stack as a trap frame, pass its address in x0, and call a C handler.
  Use `stp`/`ldp` pairs, keep SP 16-byte aligned, and pass the vector index so the handler can name which of the 16 entries fired.
- Define the frame as a C struct and check its layout against the assembly with `_Static_assert` on `sizeof` and `offsetof`, as `docs/CODING_STANDARDS.md` requires.
- Decode `ESR_EL3` in C: EC bits [31:26], IL bit [25], ISS bits [24:0].
  For EC 0x24/0x25 (data abort) also print `FAR_EL3` and the DFSC in ISS bits [5:0].
  Name the common EC values from the Arm ARM "ESR_ELx, Exception Syndrome Register" table rather than printing the raw number alone.
- Print the register dump through `kprintf`, four registers per line.
- Self-test: execute `brk #0`, which raises EC 0x3c, then recover by advancing `ELR_EL3` past the instruction (`elr += 4`) so the boot continues.
  `brk` is the cheapest exception to raise deliberately and needs no MMU.
- Gate: a `CHECKS` line matching `EC=0x3c` and the existing `milestone 0: boot OK` line still printing afterwards, proving the handler returned rather than hung, plus the full `make test` matrix.

## Batch log

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
