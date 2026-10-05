---
paths: "src/**/*.c, include/**/*.h, tests/unit/**"
---

# C code rules

Full rules: `docs/CODING_STANDARDS.md`. Module boundaries: `docs/ARCHITECTURE.md`.

- Build cleanly under `-Wall -Wextra -Werror` with both GCC and clang.
- Use `read_sysreg()`/`write_sysreg()` for system registers and accessor functions for MMIO.
  Never scatter `volatile` casts through driver logic.
- Comment every register write with the register field and the reason, citing the Arm ARM or the peripheral TRM.
- Give every polling loop on a hardware status bit a timeout.
- Use `1UL << n` for 64-bit masks and fixed-width types for registers and addresses.
- Keep functions `static` unless another module needs them, and keep them short.
- No libc, no recursion, no variable-length arrays, no heap at EL3.
- Treat every value from the non-secure world as untrusted and validate it.
