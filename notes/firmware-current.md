# Firmware campaign: current queue

**Last updated:** 2026-10-05.
The repo, docs, CI, and campaign queue are set up.
M0 is done.
Next batch: row 1, add timeouts to the PL011 polling loops.

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
| 1 | `fix(uart)`: add timeouts to PL011 polling loops | `src/uart.c` busy-wait loops get a bounded spin count; on timeout, drop the character and set an error flag readable by a test | Matrix passes; new `CHECKS` line shows the error flag is clear after boot |
| 2 | `build`: add `.clang-format` and a format check | Encode the style in `docs/CODING_STANDARDS.md`; reformat existing files in their own commit | CI job runs `clang-format --dry-run --Werror` on `src/` and `include/` |
| 3 | `feat(el3)`: known reset state and vector table (M1, part 1) | Set `SCTLR_EL3`, `SCR_EL3`, `CPTR_EL3` at reset; add `src/vectors.S` and install `VBAR_EL3` | New check prints `VBAR_EL3` equal to the `vectors` symbol |
| 4 | `feat(el3)`: trap frame and ESR decode (M1, part 2) | Save x0-x30, `ELR_EL3`, `SPSR_EL3`; decode EC, IL, ISS, DFSC; print `FAR_EL3` and a register dump | `brk #0` self-test reports `EC=0x3c` and boot continues |
| 5 | `test(el3)`: alignment fault self-test (M1, part 3) | Unaligned load recovered by advancing `ELR_EL3` | Check `EC=0x25 DFSC=0x21`; M1 marked done in `docs/ROADMAP.md`; tag `v0.1.0` |
| 6 | `test`: host unit test scaffolding | `tests/unit/` runner, `make unit`, built with ASan and UBSan; first tests cover `kprintf` formatting | `make unit` passes locally and in a new CI job |
| 7 | `feat(cpu)`: table-driven ID register decoder (M2) | Decode the ID registers listed in the roadmap into a printed feature table | Checks: SVE2, PAC, BTI, MTE present on `max` and absent on `cortex-a57`; tag `v0.2.0` |
| 8 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Batch log

### Batch 0 (2026-10-05): project setup

- Merged PR #1: M0 boot from the EL3 reset vector, UART, QEMU test harness, CI matrix.
- Merged PR #2: `CLAUDE.md`, contributing rules, PR template.
- PR #3: architecture, coding standards, testing, and performance guides.
- This PR: campaign queue and the `campaign-loop` skill.
