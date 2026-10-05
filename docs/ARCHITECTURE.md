# Architecture

This file describes how the firmware is structured today and the structure it grows into through the milestones.
Sections marked **(target)** describe planned structure, not current code.

## Goals

- Boot from the reset vector at EL3 the way production firmware does, with no vendor code.
- Hand off cleanly to a non-secure EL1 kernel, and later to an unmodified Linux kernel.
- Enable Armv9 CPU features at every exception level that gates them.
- Stay small enough that every line can be explained in an interview.

## Non-goals

- Running on real silicon.
  QEMU `virt` is the only platform, but platform details stay behind one boundary so a port is possible.
- Feature parity with Trusted Firmware-A.
  TF-A is the reference for how things are done, not a checklist.
- A Secure world payload (S-EL1 or S-EL2).

## Exception level model

| Level | Security state | Owner | Responsibility |
|---|---|---|---|
| EL3 | Secure | This firmware | Reset, CPU state setup, SMC dispatch, PSCI, world switch |
| EL2 | Non-secure | This firmware (minimal) | Configured for pass-through so EL1 runs unhindered |
| EL1 | Non-secure | Test kernel in this repo, later Linux | MMU, interrupts, timers, SMP, feature demos |
| EL0 | Non-secure | Not used yet | |

EL3 is the only code that runs at reset.
Everything below EL3 runs only after EL3 configures it and `eret`s into it.

## Boot flow

Current (M0):

1. All CPUs start at `_start` in secure flash at EL3, MMU off.
2. Secondary CPUs park in `wfe`.
3. The primary CPU sets SP, copies `.data` to secure SRAM, zeroes `.bss`, and calls `fw_main()`.
4. `fw_main()` initializes the UART, prints CPU identification, and exits through semihosting.

Target (after M6):

1. EL3 reset: put `SCTLR_EL3`, `SCR_EL3`, and `CPTR_EL3` in a known state, install `VBAR_EL3`, set up the C runtime.
2. EL3 platform setup: console, GIC distributor, feature discovery.
3. EL3 loads the EL1 image from flash into non-secure DRAM.
4. EL3 configures EL2 and EL1 controls and `eret`s to EL1.
5. EL1 turns on the MMU, sets up the GIC CPU interface and timer, then starts secondaries with PSCI `CPU_ON`.
6. Secondaries leave `wfe` in EL3, configure their own EL3 state, and `eret` to the EL1 entry point.

## Memory map and image layout

| Address | Region | Contents |
|---|---|---|
| `0x0000_0000` | Secure flash | EL3 code and read-only data; embedded EL1 image **(target)** |
| `0x0800_0000` | GICv3 distributor | M5 |
| `0x080A_0000` | GICv3 redistributors | M5 |
| `0x0900_0000` | PL011 UART0 | Console |
| `0x0E00_0000` | Secure SRAM | EL3 `.data`, `.bss`, per-CPU stacks, PSCI mailboxes **(target)** |
| `0x4000_0000` | Non-secure DRAM | EL1 image, its page tables and stacks **(target)** |

The source of truth is QEMU's `hw/arm/virt.c` and `include/platform.h`.

## Module structure (target)

```
src/
  arch/             system registers, barriers, cache and TLB maintenance, exception entry helpers
  el3/              reset, vectors, SMC dispatcher, PSCI, EL hand-off
  el1/              test kernel: entry, vectors, MMU, timer, SMP, feature demos
  drivers/          pl011, gicv3, generic timer
  lib/              kprintf, string functions, bitfield helpers
  plat/qemu_virt/   memory map and platform hooks
include/            public headers, mirroring src/
tests/              QEMU integration harness, host unit tests (planned)
```

Dependency rules:

- `arch/` and `lib/` depend on nothing else in the repo.
- `drivers/` depend on `arch/` and `lib/` only.
- `el3/` and `el1/` depend on drivers through interfaces, never on each other.
- `plat/` provides data and hook implementations, and only `plat/` knows the board.
- The test harness depends on UART output only, never on internal symbols.

## Design principles

The SOLID principles come from object-oriented design, but they map well onto firmware written in C.
Apply them at module boundaries, not inside a 20-line function.

- **Single responsibility.**
  One module owns one hardware block or one concern.
  `uart.c` knows the PL011 registers; it does not format numbers.
- **Open/closed.**
  Extend through ops tables, not by editing core code.
  For example, a new platform supplies its own `struct plat_psci_ops`, the same pattern TF-A uses, and the PSCI core stays unchanged.
- **Liskov substitution.**
  Every implementation of an ops table honors the same documented contract.
  For example, any console `putc` must be safe to call with interrupts masked and from an exception handler.
- **Interface segregation.**
  Keep headers small.
  A caller that only prints should include a console header, not the whole UART driver.
- **Dependency inversion.**
  Core code depends on interfaces such as the console or the platform hooks.
  `plat/` wires in the concrete PL011 or GIC implementation.

Do not add an interface until there is a second implementation or a platform boundary to cross.
An abstraction with one user is extra code to explain and maintain.

## Concurrency model

- Until M6, only the primary CPU runs code after `_start`.
- From M6, each CPU has its own stack and exception stack, found from a linear core ID that the platform computes from `MPIDR_EL1`.
- Shared state is protected by spinlocks with acquire and release ordering.
  Every barrier has a comment that names the ordering it provides.
- EL3 handlers run with interrupts masked and never sleep.

## Security boundaries

EL3 is the most privileged code on the system, so it treats every value from the non-secure world as untrusted.

- Validate every SMC argument before use: function ID, MPIDR, entry point address, and context ID.
- Never dereference a pointer passed from the non-secure world.
- Return `SMC_UNKNOWN` (-1) for unimplemented SMCCC function IDs.
- Clear registers that SMCCC says must not leak when returning to the non-secure world.
- Keep secure SRAM unreachable from the non-secure world; QEMU enforces this for `secure=on`.

## Error handling

- Unexpected exceptions at any level print a decoded report and park the CPU.
- `panic()` prints a message and the caller's address, then parks.
- The only recoverable exceptions are the deliberate self-tests, which advance `ELR_ELx` past the faulting instruction.
- Polling loops on hardware status bits have a timeout and report a timeout as an error.

## Decisions

| # | Decision | Why |
|---|---|---|
| 1 | QEMU `virt` with `secure=on,virtualization=on` and `-bios` | Gives real EL3 and EL2, starts all CPUs at the reset vector, and turns off QEMU's built-in PSCI. |
| 2 | Semihosting `SYS_EXIT` for test results | CI gets a real exit status without parsing a hung QEMU. |
| 3 | C11 with GNU extensions, pinned with `-std=gnu11` | Statement expressions for register macros, and a toolchain upgrade cannot silently change the language. |
| 4 | Build with both GCC and LLVM in CI | Catches compiler-specific undefined behavior and keeps the code portable. |
| 5 | Merge commits for PRs | Keeps small bisectable commits and groups them by PR. |

Add a row when a decision constrains future work.
If a decision needs more than a sentence of reasoning, write `docs/adr/NNNN-title.md` with Context, Decision, and Consequences sections.
