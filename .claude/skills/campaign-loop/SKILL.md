---
name: campaign-loop
description: Run one batch of a long multi-session campaign from a committed queue file - take the top workable row, gate it, rotate the notes, commit state and code together, pre-seed the next batch. Use when work spans many sessions or machines and its state must be reviewable git history rather than local scratch. Argument controls looping ("once" default, a number N, or "continuous").
---

# Campaign Loop

The committed-state sibling of `checkpoint-loop`.
`checkpoint-loop` pursues one objective with untracked state under `.git/`; that state dies with the clone and is invisible to review.
Use this skill instead when the work is a campaign - it spans days, sessions, or machines - so any session anywhere can resume from `git pull` alone, and the state's own history is auditable.

## The queue file is the campaign's memory

State lives in a committed notes file, for example `notes/<campaign>-current.md`, holding:

- a **Last updated** line - the single source of truth for what happened last and what is next.
- the **queue**: one row per unit of work, ranked, each row carrying its own scope and its own gate (the specific tests or checks that ratify it).
- at most about two recent batch sections describing what landed.

Two optional siblings keep it lean: `<campaign>-previous.md` (archive of finished batch detail) and `<campaign>-future.md` (parked rows, each with the explicit condition that would unblock it).
A parked row without an unblock condition is a row nobody ever picks up again.

## One invocation = one batch

1. **Load state.** Read the queue file and `git status --short --branch`.
   Reconstruct from git plus notes, never from an old transcript summary.
   Take the top workable row; skip rows marked `[BLOCKED: ...]` or `[USER-GATED]`.
2. **Work only the row's scope.** A survey or a design decision can be a batch too; its deliverable is a notes update rather than code.
3. **Gate it - scoped, not global.** Run the checks the row names, not the whole world.
   A failure gets two evidence-driven attempts; on the second failure, back out only this batch's changes, mark the row `[BLOCKED: reason]`, and take the next unaffected row.
   A blocker that affects a class of rows blocks that class, never the run.
4. **Rotate the notes.** Update the queue row (done rows leave the queue), refresh **Last updated**, add a short batch section, and archive older sections into the previous file.
   Re-measure any live number you carry forward; never copy a figure from the previous batch, because stale numbers survive longest exactly when nothing forces anyone to look at them.
5. **Commit code and notes in the SAME commit**, one commit per batch, then push.
   Stage exact paths, never `git add -A`: a campaign tree often carries someone else's work in progress, and a whole-tree add has swallowed a collaborator's half-done edits before.
6. **Pre-seed the next batch** so the next invocation opens with its work already laid out.

Arguments: none or `once` = one batch then stop and report; `N` = N batches back-to-back, each committed separately; `continuous` = keep taking rows until a stop condition fires.

## Queue-empty close-out (run it; do not just stop)

An empty queue is a state that must be recorded, because the next session opens on these files and a silent stop reads identically to a crash.

1. Move every remaining row out with its unblock condition.
2. State plainly in the queue file that the queue is empty and what the file now holds.
3. Commit that as a real commit; queue state is project state.
4. Report what is blocked and on whom, ranked by what actually gates the goal.

Never manufacture low-value batches to avoid reporting an empty queue; a loop that does is worse than one that stops and says so.

## Stop conditions

- The queue is exhausted, or only blocked or user-gated rows remain (run the close-out).
- Two consecutive batches end blocked - something systemic; a human should look.
- A row needs an approval, a paid operation, or a destructive action the user has not granted. Ask, never assume.
- User interruption.

A context boundary is not a stop condition: state is already committed, so a fresh session resumes from the queue file.

## Why committed state

- The queue file survives context compaction, session crashes, and machine switches, and `git log` is the audit trail of every decision.
- Rotating notes in the same commit as the code prevents drift: code on the branch with a queue row still reading "untouched" makes the next session restart or collide with finished work.
- The most commonly skipped step is the archive rotation; verify it mechanically before committing (for example: the queue file must appear in `git status --short` whenever source changed this batch).
