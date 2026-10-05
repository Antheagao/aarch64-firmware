---
paths: "tests/**"
---

# Test rules

Full rules: `docs/TESTING.md`.

- Each `CHECKS` entry is `(what it proves, regex)`, and the regex matches values, not just labels.
- Never assert on timing or on CPU interleaving; assert invariants and use tolerances for timers.
- Never retry, skip, or disable a test to get green.
  A flaky test is a bug to fix.
- Reproduce a bug with a failing check before fixing it.
- Keep the harness stdlib-only Python and keep QEMU in its own process group so a hang is killed.
