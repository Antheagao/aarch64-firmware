# aarch64-firmware

Bare-metal AArch64 firmware written from the reset vector up.
It boots at EL3 on QEMU `virt` (`secure=on,virtualization=on`) and works up through the milestones in `docs/ROADMAP.md`.
It is a portfolio project for CPU firmware roles, so the code must be explainable line by line, not just working.

## Commands

| Command | Use |
|---|---|
| `make` | Build `build/fw.bin` with the GCC cross toolchain |
| `make LLVM=1` | Build with clang and lld |
| `make run` | Boot in QEMU; the firmware exits by itself |
| `make test` | Build, boot, and assert on UART output; must pass before any commit |
| `make test CPU=cortex-a57` | Same on an Armv8.0 core with no Armv9 features |
| `make debug` + `make gdb` | Stop at the reset vector with GDB attached on port 1234 |
| `make disasm` | Disassemble `build/fw.elf` |

Run the full matrix before opening a PR: GCC and LLVM, each on `CPU=max` and `CPU=cortex-a57`.
That is exactly what CI runs.

## Layout

- `src/boot.S`: reset vector, CPU parking, C runtime setup
- `src/*.c`, `include/*.h`: firmware C code and headers
- `linker.ld`: code in secure flash at 0x0, data and stack in secure SRAM at 0x0E000000
- `tests/run_tests.py`: boots QEMU and matches UART output against the `CHECKS` list
- `docs/ROADMAP.md`: milestone specs with "done when" tests
- `docs/CONTRIBUTING.md`: branch, commit, and PR rules

## Hard rules

- The firmware runs with the MMU off, so all memory is Device memory.
  Keep `-mstrict-align` and `-mgeneral-regs-only` until a milestone deliberately enables Normal memory or FP/SIMD.
- No libc and no dynamic allocation at EL3.
- Every new behavior adds a line to `CHECKS` in `tests/run_tests.py`.
  A feature without a check is not done.
- Fix a bug by reproducing it first: add a failing check or a QEMU run that shows it, then fix it.
- Base every register write on the *Arm Architecture Reference Manual* (DDI 0487).
  Never guess a bit position.
  Name the register and field in a comment, and say why the value is needed.
- `-Werror` stays on.
  If you see a warning, a lint error, or a flaky test, fix it, even if it is unrelated to the current task, in its own commit.
- Prefer quality, simplicity, and long-term maintainability over development speed.

## Git workflow

Full rules are in `docs/CONTRIBUTING.md`.
The short version:

- Never commit to `main` directly.
  Branch from `main` as `<type>/<short-topic>`, for example `feat/m1-el3-vectors`.
- Commit messages follow Conventional Commits: `type(scope): imperative summary`.
- Keep commits small, and make sure each one builds and passes `make test`.
- Never add AI co-author trailers or "Generated with" lines to commits, PRs, or comments.
- Open a PR early, merge it once CI is green with a merge commit, then delete the branch.
  Merge often: one milestone step per PR, not a whole milestone.

## Writing style

- Never use the em dash character.
  Use a plain dash, a colon, or a new sentence.
- In Markdown files, put each full sentence on its own line.
- Use clear, simple wording with short sentences, concrete nouns, and no filler.
