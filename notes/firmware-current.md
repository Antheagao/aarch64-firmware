# Firmware campaign: current queue

**Last updated:** 2026-10-06.
Batch 21 rotated this file: the batch log had grown to 21 sections, so everything older than the last two moved to `notes/firmware-previous.md`.
M0 through M4 are done and tagged `v0.1.0` to `v0.4.0`; `main` is green on all eight CI checks.
Next batch: row 1, drop `-mstrict-align` from the EL1 image now that it runs on Normal memory.

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
| 1 | `build`: drop `-mstrict-align` for the EL1 image | Only once EL1 runs on Normal memory, and only for that image: the firmware keeps it while the MMU is off at EL3 | The matrix still passes with the flag removed from the EL1 build alone |
| 2 | [USER-GATED] Turn on "Automatically delete head branches" in the GitHub repo settings | The Claude session cannot delete remote branches, so merged branches stay until the owner deletes them | Merged branches disappear after merge |

## Next batch plan (row 1)

- Branch: `build/el1-strict-align`.
- Remove `-mstrict-align` from the EL1 image's flags only. The firmware keeps it: the MMU is still off at EL3, so all of its memory is Device, where unaligned access faults regardless of what the compiler assumes.
- That asymmetry is the content of the row, so make it visible in the Makefile rather than leaving a bare flag difference: the two images genuinely run under different memory attributes, and anyone reading it later should see why.
- Expect the generated code to change: without the flag the compiler may merge adjacent loads and stores into unaligned accesses, which is the point, and `SCTLR_EL1.A` must therefore be reconsidered. Alignment checking on with unaligned accesses allowed is a contradiction, so decide deliberately whether to keep `A` set and leave this flag alone instead.
- If `SCTLR_EL1.A` stays set, the honest outcome of this row may be "do not do it, and record why". That is a legitimate result and better than a change that passes CI because nothing happens to generate an unaligned access yet.
- Gate: the full matrix, and either the flag is gone with `SCTLR_EL1.A` handled, or the row is closed with the reasoning written down.

## Batch log

The two most recent batches live here; everything older is in `notes/firmware-previous.md`.

### Batch 21 (2026-10-06): rotate the batch log

- No code. The batch log had reached 21 sections and 229 lines, against a queue file that is supposed to be short enough to read at the start of every batch. Everything older than the last two batches moved to `notes/firmware-previous.md`, newest first, leaving 53 lines here.
- Batch 0 had been sitting in both files since batch 2: it was copied into the archive but never removed from here. It now appears once.
- The **Last updated** date still read 2026-10-05 after six batches on the 6th. It is the one line the next session reads first, so a stale date there is worse than a stale paragraph further down.
- Learning: `.claude/skills/campaign-loop/SKILL.md` calls the archive rotation the most commonly skipped step, and skipping it for nineteen batches is what that looks like in practice. It survived that long because nothing fails when it is missed; the file just quietly stops being readable.

### Batch 20 (2026-10-06): permission and execute-never faults, M4 done

- Two self-tests prove the map is enforced rather than merely written: a write to `.rodata` gives `EC=0x25` with `DFSC=0x0f`, a permission fault at level 3, and a call into `.data` gives `EC=0x21`, an instruction abort.
- Recovering from the instruction abort needed a rule the earlier self-tests did not. `ELR` points at the address that could not be fetched, so advancing it by 4 stays inside non-executable memory and the fault repeats forever. The faulting fetch was a call, so the handler returns to `x30`, which makes a failed call behave like one that returned.
- That is now the third distinct resume rule in one handler, after `+4` for a `brk` and no adjustment at all for an `smc`. Each is written next to its branch, because the right correction depends entirely on what the exception was.
- The `.data` buffer holds a real `nop; ret`, which would execute perfectly well if the page allowed it. The fault is the mapping's doing and not the data's, which is worth being able to say.
- M4 is complete: tables built and tested on the host, the MMU on with the right barriers, per-section permissions, and both faults caught and reported rather than hanging.

### Batch 19 (2026-10-06): per-section permissions

- One region per section replaced the single writable-and-executable block: `.text` RX, `.rodata` RO, `.data` through the stack RW and XN, the UART Device. `PT_RW_X` now has no users.
- `kernel/kernel.ld` aligns every section to 4 KiB, because a page is the MMU's smallest unit of permission.
- Nothing outside the image and the UART is mapped at all now, where before a whole 1 GiB block was.
- `clang-tidy` objected to the linker symbols using the reserved double-underscore namespace, which is correct C. The names stayed, since that is the universal convention for linker-defined symbols, and the suppression is scoped to those three declarations rather than disabling the check.
