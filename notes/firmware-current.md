# Firmware campaign: current queue

**Last updated:** 2026-10-05.
Batch 1 bounded every UART polling loop on branch `fix/uart-poll-timeout`.
Next batch: row 1, add `.clang-format` and a format check in CI.

This file is the work queue for the `campaign-loop` skill in `.claude/skills/campaign-loop/SKILL.md`.
Milestone specs live in `docs/ROADMAP.md`; this file only tracks order and state.
Parked rows and their unblock conditions are in `notes/firmware-future.md`.

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
| 1 | `build`: add `.clang-format` and a format check | Encode the style in `docs/CODING_STANDARDS.md`; reformat existing files in their own commit | CI job runs `clang-format --dry-run --Werror` on `src/` and `include/` |
| 2 | `feat(el3)`: known reset state and vector table (M1, part 1) | Set `SCTLR_EL3`, `SCR_EL3`, `CPTR_EL3` at reset; add `src/vectors.S` and install `VBAR_EL3` | New check prints `VBAR_EL3` equal to the `vectors` symbol |
| 3 | `feat(el3)`: trap frame and ESR decode (M1, part 2) | Save x0-x30, `ELR_EL3`, `SPSR_EL3`; decode EC, IL, ISS, DFSC; print `FAR_EL3` and a register dump | `brk #0` self-test reports `EC=0x3c` and boot continues |
| 4 | `test(el3)`: alignment fault self-test (M1, part 3) | Unaligned load recovered by advancing `ELR_EL3` | Check `EC=0x25 DFSC=0x21`; M1 marked done in `docs/ROADMAP.md`; tag `v0.1.0` |
| 5 | `test`: host unit test scaffolding | `tests/unit/` runner, `make unit`, built with ASan and UBSan; first tests cover `kprintf` formatting | `make unit` passes locally and in a new CI job |
| 6 | `feat(cpu)`: table-driven ID register decoder (M2) | Decode the ID registers listed in the roadmap into a printed feature table | Checks: SVE2, PAC, BTI, MTE present on `max` and absent on `cortex-a57`; tag `v0.2.0` |
| 7 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Next batch plan (row 1)

- Branch: `build/clang-format`.
- Ubuntu 24.04 ships `clang-format` 18, so CI pins `clang-format-18` and the config uses only options it supports.
- Start from `BasedOnStyle: LLVM`, then override to match `docs/CODING_STANDARDS.md`: `IndentWidth: 4`, `ColumnLimit: 100`, `BreakBeforeBraces: Linux`, `AllowShortFunctionsOnASingleLine: None`, `AlignConsecutiveMacros: Consecutive`, `AlignTrailingComments: true`.
- Commit 1 adds `.clang-format` and a `make format` / `make format-check` pair.
  Commit 2 is the mechanical reformat and nothing else.
  Commit 3 adds the CI job and updates this file.
- Gate: `make format-check` passes, CI runs it, and the `make test` matrix still passes.

## Batch log

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
