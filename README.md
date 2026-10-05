# aarch64-firmware

[![CI](https://github.com/Antheagao/aarch64-firmware/actions/workflows/ci.yml/badge.svg)](https://github.com/Antheagao/aarch64-firmware/actions/workflows/ci.yml)

Bare-metal firmware for 64-bit Arm, written from the reset vector up.
It runs on QEMU's `virt` board and starts where real boot firmware starts: EL3, MMU off, executing out of flash.
From there the roadmap works up to an EL1 kernel with an MMU, GICv3 interrupts, PSCI multi-core bring-up, and Armv9 feature enablement (SVE2, PAC/BTI, MTE).

It uses no vendor SDK, no libc, and no borrowed boot code.
It is C, AArch64 assembly, a linker script, and the Arm Architecture Reference Manual.

## Status

| ID | Milestone | Status |
|---|---|---|
| M0 | Reset vector at EL3, C runtime from flash, PL011 UART, CI boot test | Done |
| M1 | EL3 exception vectors and crash reporter (ESR/ELR/FAR decode) | Not started |
| M2 | CPU feature discovery from the ID registers | Not started |
| M3 | EL3 to EL1 hand-off to a kernel in non-secure DRAM | Not started |
| M4 | MMU: page tables, memory attributes, caches | Not started |
| M5 | GICv3 and generic timer interrupts | Not started |
| M6 | PSCI over SMC, multi-core bring-up, spinlocks | Not started |
| M7 | Armv9 feature enablement: SVE2, PAC/BTI, MTE | Not started |
| M8 | Performance measurement with the PMU | Not started |
| S1 | Stretch: boot an unmodified Linux kernel on this firmware | Not started |

Each milestone's scope and "done when" test is in [docs/ROADMAP.md](docs/ROADMAP.md).

## Run it

**Ubuntu / WSL**

```sh
sudo apt install gcc-aarch64-linux-gnu qemu-system-arm gdb-multiarch
make run
```

**macOS**

```sh
brew install aarch64-elf-gcc qemu
make run
```

The Makefile finds the `aarch64-none-elf-`, `aarch64-elf-`, or `aarch64-linux-gnu-` toolchain prefix on its own.

Expected output:

```
aarch64-firmware: reset vector reached at EL3
  MIDR_EL1  = 0x00000000000f0510
  MPIDR_EL1 = 0x0000000080000000
selftest: poll gives up on a stuck bit after 1000 spins: ok
selftest: poll returns at once on a clear bit: ok
uart: tx timeouts = 0
milestone 0: boot OK
```

| Command | What it does |
|---|---|
| `make run` | Boot in QEMU. The firmware exits on its own, and `Ctrl-a x` force-quits. |
| `make test` | Boot and assert on the UART output and exit status. This is what CI runs. |
| `make debug`, then `make gdb` in a second terminal | Start paused at the reset vector with GDB attached. |
| `make LLVM=1` | Build with clang and lld instead of GCC and binutils. |
| `make run CPU=cortex-a57` | Run on a plain Armv8.0 core instead of `max` (Armv9 features). |
| `make disasm` | Disassemble the firmware. |

CI builds with both GCC and LLVM and boots on both `-cpu max` and `-cpu cortex-a57`.

## How it boots

```
reset: every CPU, EL3, MMU and caches off, PC = 0x0 in secure flash
  boot.S  _start
    MPIDR_EL1 affinity != 0.0.0  ->  park in WFE (until PSCI CPU_ON, M6)
    SP = top of a 16 KiB stack in secure SRAM
    copy .data from flash to SRAM, zero .bss
    fw_main()                                         main.c
      uart_init(): PL011 baud divisor, 8N1, FIFOs     uart.c
      print CurrentEL, MIDR_EL1, MPIDR_EL1
      semihosting SYS_EXIT(0)  ->  QEMU exits with status 0
```

QEMU runs as `-M virt,secure=on,virtualization=on,gic-version=3 -bios build/fw.bin`.
`secure=on` adds EL3 and `virtualization=on` adds EL2.
With a `-bios` image loaded, QEMU also starts all CPUs at the reset vector and turns off its built-in PSCI.
That leaves the firmware responsible for everything, as it would be on real silicon.

## Memory map (QEMU `virt`, `secure=on`)

| Address | Region | Used for |
|---|---|---|
| `0x0000_0000` | Secure flash, 64 MiB | Firmware image, code executes in place |
| `0x0800_0000` | GICv3 distributor | M5 |
| `0x080A_0000` | GICv3 redistributors | M5 |
| `0x0900_0000` | PL011 UART0 | Console |
| `0x0E00_0000` | Secure SRAM, 16 MiB | `.data`, `.bss`, EL3 stack |
| `0x4000_0000` | Non-secure DRAM | EL1 kernel (M3) |

## Layout

```
src/boot.S          reset vector: CPU parking, stack, .data/.bss setup
src/main.c          fw_main(): first C code
src/uart.c          PL011 driver
src/kprintf.c       minimal printf for register dumps
src/semihost.c      SYS_EXIT so tests get a real exit status
include/platform.h  board memory map
include/mmio.h      MMIO accessors and bounded polling
include/sysreg.h    read_sysreg()/write_sysreg(), barriers
linker.ld           flash vs. SRAM placement
tests/run_tests.py  boots QEMU, checks UART output against per-milestone patterns
docs/ROADMAP.md     milestones, acceptance criteria, references
```

## References

- *Arm Architecture Reference Manual for A-profile architecture* (DDI 0487, issue M.d or later)
- Arm *Learn the architecture* guides: exception model, memory management, memory model, GIC
- *PrimeCell UART (PL011) Technical Reference Manual* (DDI 0183)
- *GICv3 and GICv4 Architecture Specification* (IHI 0069)
- *Arm Power State Coordination Interface* (DEN 0022) and *SMC Calling Convention* (DEN 0028)
- [Trusted Firmware-A](https://www.trustedfirmware.org/projects/tf-a/), for how production firmware does all of this
- QEMU's `hw/arm/virt.c`, the source for the board's memory map
