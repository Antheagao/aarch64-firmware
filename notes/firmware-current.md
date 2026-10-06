# Firmware campaign: current queue

**Last updated:** 2026-10-05.
Batch 2 added `.clang-format`, `make format` / `make format-check`, and a CI format job on branch `build/clang-format`.
Next batch: row 1, put `SCTLR_EL3`, `SCR_EL3` and `CPTR_EL3` into a known reset state and install a vector table in `VBAR_EL3` (M1, part 1).

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
| 1 | `feat(el3)`: known reset state and vector table (M1, part 1) | Set `SCTLR_EL3`, `SCR_EL3`, `CPTR_EL3` at reset; add `src/vectors.S` and install `VBAR_EL3` | New check prints `VBAR_EL3` equal to the `vectors` symbol |
| 2 | `feat(el3)`: trap frame and ESR decode (M1, part 2) | Save x0-x30, `ELR_EL3`, `SPSR_EL3`; decode EC, IL, ISS, DFSC; print `FAR_EL3` and a register dump | `brk #0` self-test reports `EC=0x3c` and boot continues |
| 3 | `test(el3)`: alignment fault self-test (M1, part 3) | Unaligned load recovered by advancing `ELR_EL3` | Check `EC=0x25 DFSC=0x21`; M1 marked done in `docs/ROADMAP.md`; tag `v0.1.0` |
| 4 | `test`: host unit test scaffolding | `tests/unit/` runner, `make unit`, built with ASan and UBSan; first tests cover `kprintf` formatting | `make unit` passes locally and in a new CI job |
| 5 | `feat(cpu)`: table-driven ID register decoder (M2) | Decode the ID registers listed in the roadmap into a printed feature table | Checks: SVE2, PAC, BTI, MTE present on `max` and absent on `cortex-a57`; tag `v0.2.0` |
| 6 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Next batch plan (row 1)

- Branch: `feat/m1-el3-reset-state`.
- At reset, before anything else in `boot.S`, put the EL3 control registers into a known state.
  Hardware resets many of their bits to UNKNOWN values, so firmware must write them rather than assume.
  `SCTLR_EL3`: start from the architected reset value with `A` (alignment check) and `SA` (stack alignment check) set, `M`, `C` and `I` clear while the MMU is off.
  `SCR_EL3`: `NS` clear (secure), `RW` set (lower EL is AArch64), `EA`/`FIQ`/`IRQ` routed as the roadmap needs.
  `CPTR_EL3`: `TFP` clear is not needed yet because the build is `-mgeneral-regs-only`, so set it to trap and revisit in M7.
- Add `src/vectors.S`: 16 entries, each `0x80` bytes, aligned to 2 KiB (`.balign 2048`), in the four groups the Arm ARM lists under "Exception vectors".
  Each entry can branch to a shared stub for now; the trap frame and decode land in row 2.
- Install it with `VBAR_EL3` and `isb()`.
- Gate: a new `CHECKS` line prints `VBAR_EL3` and the harness asserts it equals the `vectors` symbol address, plus the full `make test` matrix.

## Batch log

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
