# Coding standards

These rules exist to keep firmware correct, readable, and easy to explain.
When a rule and the hardware disagree, the hardware wins, and the reason goes in a comment.

## C

### Language and toolchain

- C11 with GNU extensions, pinned with `-std=gnu11`.
  GCC 15 and later default to `gnu23`, so the flag keeps toolchain upgrades from changing the language.
- Every file must build without warnings under `-Wall -Wextra -Werror` with both GCC and clang.
- No libc.
  The compiler's freestanding headers (`stdint.h`, `stddef.h`, `stdbool.h`, `stdarg.h`) are allowed.
- No floating point or SIMD until the code that uses it enables it at every exception level.

### Naming

- `snake_case` for functions and variables, `UPPER_CASE` for macros and constants.
- Prefix every external function with its module: `uart_init`, `gic_enable_ppi`, `psci_cpu_on`.
- Name register field macros after the Arm ARM: `SCR_EL3_NS`, `SCTLR_EL1_M`, `ESR_EC_SHIFT`.

### Types and data

- Use fixed-width types: `uint32_t` for 32-bit registers, `uint64_t` for system registers, `uintptr_t` for addresses.
- Use `1UL << n` for 64-bit bit masks.
  A plain `1 << 31` is undefined behavior when the shift reaches the sign bit.
- Make functions and file-scope variables `static` unless another module needs them.
- Use `const` for anything that is not written after initialization.
- Check the layout of structs shared with assembly, such as the trap frame, with `_Static_assert` on their size and field offsets.

### Hardware access

- Access memory-mapped registers only through small accessor functions that use `volatile` internally.
  Never scatter `volatile` pointer casts through driver logic.
- Access system registers only through `read_sysreg()` and `write_sysreg()`.
- After a system register write that must take effect before the next instruction, issue `isb()`.
- Every barrier gets a comment that says what it orders and why.
- Every polling loop on a hardware status bit has a timeout.
- Every register write gets a comment naming the register field and the reason for the value, with the Arm ARM or peripheral TRM as the source.

### Functions and control flow

- Keep functions short and single-purpose.
  If a comment is needed to separate "parts" of a function, split it.
- Prefer early returns over deep nesting.
- No recursion and no variable-length arrays.
  Stack size at EL3 is fixed and must stay bounded.
- Return negative error codes inside the firmware.
  Convert to PSCI or SMCCC return codes only at the SMC boundary.

### Comments

- Explain why, not what.
  The code already says what.
- Cite the source for architectural behavior by document and section title, for example "Arm ARM (DDI 0487), Exception entry".
  Section numbers change between issues, so titles last longer.
- Leave `TODO(Mn)` markers only for work that a roadmap milestone owns.

### Formatting

- 4-space indentation, no tabs in C, braces on the same line except for function definitions.
- Lines up to 100 columns.
- A `.clang-format` file will enforce this once it is added; until then, match the surrounding code.

## Assembly

- Follow the AAPCS64 calling convention: x0-x7 for arguments and results, x19-x28 callee-saved, x29 frame pointer, x30 link register.
  Do not use x18, which the platform ABI reserves.
- Keep SP 16-byte aligned at every public boundary.
- Keep assembly to what C cannot do: reset entry, exception vectors, context save and restore, and instructions with no compiler intrinsic.
- Use named labels for anything that is a branch target from another place.
  Numeric local labels (`1:`, `1b`) are fine for loops a few lines long.
- Comment every system register write with the field being set and why.
- Once BTI is enabled (M7), every indirect branch target needs a `bti` landing pad.

## Algorithms and data structures

Firmware needs predictable time and memory more than raw throughput.
Choose the simplest algorithm with a bounded worst case, then optimize only when a measurement says it matters.

| Problem | Choice | Why |
|---|---|---|
| ID register decoding (M2) | A table of `{register, shift, width, signed, name}` entries walked by one function | Adding a feature is one table row, not another `if` chain. |
| SMC dispatch (M6) | A `switch` on the function ID, or a sorted table with binary search if it grows past about 20 entries | O(1) or O(log n), easy to audit for unhandled IDs. |
| Per-CPU data (M6) | An array indexed by linear core ID | O(1) lookup with no locking. |
| Spinlock (M6) | Ticket lock, using LSE atomics when `FEAT_LSE` is present | Fair under contention, unlike a test-and-set lock. |
| Page tables (M4) | Map each region with the largest block that fits (1 GiB, then 2 MiB, then 4 KiB pages) | Fewer tables and fewer TLB entries. |
| Page table memory (M4) | A static pool of 4 KiB tables, allocated by bump pointer | No heap at boot, and the worst case is known at link time. |
| memcpy and memset | Aligned 64-bit or paired loads and stores, with a byte loop for the head and tail | Correct on Device memory and fast on Normal memory. |

## Review checklist

- Does every new behavior have a `CHECKS` line?
- Is every register write commented with its field and reason?
- Is every value from the non-secure world validated before use?
- Does every polling loop have a timeout?
- Does the code build cleanly with both GCC and LLVM?
