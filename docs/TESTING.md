# Testing

The firmware is tested the way it is used: by booting it.
Every behavior is proven by something it prints over the UART or by the exit status QEMU returns.
Sections marked **(planned)** describe test layers that do not exist yet.

## Rules

- Every new behavior adds at least one line to `CHECKS` in `tests/run_tests.py`.
- Every bug fix starts by reproducing the bug: add a check that fails, then make it pass.
- A flaky test is a bug.
  Never retry a test to get green, and never skip or disable one.
- Fix any warning, lint error, or flaky test you find, even if it is unrelated to your change, in its own commit.

## Test layers

### 1. Build checks

- `-Wall -Wextra -Werror` on every file.
- Two compilers: GCC and clang, because each catches undefined behavior the other misses.
- `make syntax-check` compiles the C sources with the *host* compiler and `-fsyntax-only`.
  It needs no cross toolchain and no QEMU, so an ordinary C error is caught in seconds rather than waiting on the boot matrix, and it runs on a machine that cannot build the firmware at all.
  `-fsyntax-only` stops before assembly, which is what lets a host compiler accept the AArch64 inline asm in `include/sysreg.h`.
  It checks syntax and semantics only: it proves nothing about what the firmware does, so it adds to the QEMU checks rather than replacing any of them.
- `make format-check` enforces `.clang-format`, pinned to clang-format 18.
- `make lint` runs `clang-tidy` over `src/` and `kernel/` at both `TRAP_EL` values, with `WarningsAsErrors` on, and CI runs it pinned to `clang-tidy-18`.
  It analyses for the *target*, not the host: `src/semihost.c` names AArch64 registers in inline assembly, which a host target rejects outright.
  `.clang-tidy` carries the check list, and every exclusion carries its reason. A suppression list without reasons is a worse artifact than no linter, because nobody can tell later which entries were judgement and which were shrugging.
- **(planned)** `cppcheck` as a second static-analysis job.

### 2. Host unit tests

`make unit` builds `tests/unit/` for the host with `-fsanitize=address,undefined` and runs it; CI runs it too.
Sanitizers are the reason these run on the host: QEMU gives us neither, and the logic under test is the same code the firmware ships.

The firmware sources are compiled unchanged.
The only substitution is `tests/unit/fake_uart.c`, which captures what `kprintf` writes instead of touching MMIO.
That seam is what makes the real `src/kprintf.c` testable off-target, and it is the pattern to follow: keep logic in functions that take values as arguments and return results, so the same code runs on the host and on the target.

Covered now:

- `kprintf` formatting, including its known limitations, which are pinned by tests so that changing one is deliberate.

Good candidates next:

- ESR and ID register decoders (M1, M2)
- The page table builder (M4)
- PSCI argument validation (M6)

### 3. QEMU integration tests

`tests/run_tests.py` builds the firmware, boots it in QEMU, and checks the UART output against `CHECKS`.
QEMU runs in its own process group and is killed after 30 seconds, so a hang fails the run instead of blocking it.

Each entry in `CHECKS` is `(what it proves, regex)`.

- Match values, not just labels.
  `EC=0x25 DFSC=0x21` proves more than `data abort`.
- Do not assert on timing or on how CPUs interleave.
  QEMU with multi-threaded TCG does not guarantee either.
  Assert invariants instead, such as the final value of a locked counter.
- For timer checks, assert within a tolerance.

### 4. Fault injection

Firmware has to handle faults, so the tests cause them on purpose.
Each milestone that adds a protection also adds a self-test that triggers it and checks the decoded report:

| Milestone | Injected fault | Expected report |
|---|---|---|
| M1 | `brk #0`, unaligned load | `EC=0x3c`, `EC=0x25 DFSC=0x21` |
| M4 | Write to `.rodata`, jump into `.data` | `EC=0x25 DFSC=0x0f`, `EC=0x21` |
| M7 | Corrupted return address, missing BTI landing pad, tag mismatch | `EC=0x1c`, `EC=0x0d`, `EC=0x25 DFSC=0x11` |

### 5. Configuration matrix

CI runs every combination of:

- Toolchain: GCC, LLVM
- CPU: `max` (Armv9 features), `cortex-a57` (Armv8.0, none of them)

Planned additions as milestones need them: `-smp 1` and `-smp 4`, `mte=on`, and several SVE vector lengths.

### 6. Real hardware

Only M8 uses real hardware, and only for performance numbers.
See `docs/PERFORMANCE.md`.

## Running tests

```sh
make test                      # GCC, CPU=max
make test LLVM=1 CPU=cortex-a57
```

Run the full four-way matrix before opening a PR.

## Debugging a failing test

1. Run `make run` and read the full UART output.
2. Run `make debug` in one terminal and `make gdb` in another.
   Break on the failing function, then step with `si` and inspect system registers with `p/x $SCTLR_EL3`.
3. Ask QEMU to log exceptions: add `-d int,guest_errors -D qemu.log` to the QEMU command.
   Avoid `-d in_asm` except for short runs, because it produces huge logs.
4. For a crash, decode `ESR_ELx` with the crash reporter from M1 before guessing.

## CI environment

- GitHub Actions on `ubuntu-24.04`, with `actions/checkout@v6`.
- Toolchains and QEMU come from Ubuntu packages: GCC 13, clang 18, and QEMU 8.2.
- QEMU 8.2 already emulates every feature the milestones use: SVE2, MTE, PAC, BTI, GICv3, and the PMU.
- The newest QEMU release is 11.1 (August 2026).
  If a milestone needs something 8.2 lacks, build that QEMU version in CI and cache it rather than changing distro.
