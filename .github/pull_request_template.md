## What

<!-- One or two sentences on what this PR changes. -->

## Why

<!-- The problem it solves or the milestone step it completes. -->

## How it was tested

<!-- Commands you ran and what they showed. Name any new CHECKS lines. -->

## Checklist

- [ ] `make test` passes with GCC and LLVM on `CPU=max` and `CPU=cortex-a57`
- [ ] New behavior has a matching line in `CHECKS` in `tests/run_tests.py`
- [ ] Register writes name the register, the field, and the reason in a comment
- [ ] `docs/ROADMAP.md` status and any affected docs are updated
- [ ] Each commit builds on its own and follows Conventional Commits
