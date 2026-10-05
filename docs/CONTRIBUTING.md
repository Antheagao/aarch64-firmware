# Contributing

This file sets the rules for branches, commits, pull requests, and releases.
The goal is a history where every commit builds, every PR is small enough to review in one sitting, and `main` is always green.

## Branches

- `main` is always releasable: it builds with GCC and LLVM and passes `make test` on every CI CPU.
- Never commit to `main` directly.
  All changes land through a pull request.
- Branch from the latest `main` and name the branch `<type>/<short-topic>`, using the commit types below.
  Examples: `feat/m1-el3-vectors`, `fix/uart-baud-rounding`, `docs/el-handoff`.
- Keep branches short-lived.
  A branch should live for hours or a few days, not weeks.
  Split a milestone into several branches, for example vector table, then ESR decode, then self-test.
- If `main` moves while you work, rebase your own branch onto it with `git rebase main`.
  Push a rebased branch with `git push --force-with-lease`, and only for a branch nobody else uses.
  Never force-push `main`.

## Commits

Messages follow [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/):

```
type(scope): imperative summary in 72 characters or fewer

Body that explains why the change is needed and anything surprising
about how it works. Wrap at 72 columns. Reference the Arm ARM section
or register name when the change depends on architecture behavior.
```

| Type | Use for |
|---|---|
| `feat` | New firmware behavior |
| `fix` | A bug fix |
| `test` | Test harness or new checks only |
| `docs` | Documentation only |
| `refactor` | Code change with no behavior change |
| `perf` | A change that improves a measured number |
| `build` | Makefile, linker script, toolchain flags |
| `ci` | GitHub Actions workflows |
| `chore` | Repository housekeeping |

Scopes name the module, for example `boot`, `uart`, `el3`, `mmu`, `gic`, `timer`, `psci`, `sve`, `pac`, `mte`, `pmu`, or `tests`.

Rules:

- Write the summary in the imperative mood: "add ESR decoder", not "added" or "adds".
- One logical change per commit.
  Never mix a refactor with a behavior change.
- Every commit builds and passes `make test`, so `git bisect` always works.
- Stage exact paths rather than `git add -A`, so unrelated work in progress never slips in.
- Mark a breaking change with `!` after the type, for example `feat(boot)!: move SRAM base`, and explain it in the body.
- Never add AI co-author trailers or "Generated with" lines.

## Pull requests

- Open the PR as soon as the first commit is pushed.
  Use a draft if it is not ready for review.
- Keep PRs small: one milestone step, ideally under about 400 changed lines.
- Title the PR like a commit message: `feat(el3): install exception vector table`.
- Fill in the template in `.github/pull_request_template.md`: what, why, and how it was tested.
- Before asking for review, read the full diff yourself as a reviewer would.
- CI must be green before merging.
  Never merge over a red check, and never skip or disable a test to get green.
- Merge with a merge commit (`git merge --no-ff`, or "Create a merge commit" on GitHub).
  This keeps each small commit for `git bisect` and groups them under the PR.
- Delete the branch after merging.

## Releases

- Tag each completed milestone on `main` with an annotated tag: `v0.<milestone>.0`, for example `v0.1.0` for M1.
- The tag message lists what the milestone added and the CHECKS lines that prove it.

## Writing style

- Never use the em dash character.
- In Markdown, put each full sentence on its own line.
- Use clear, simple wording in code comments, commit messages, and PRs.
