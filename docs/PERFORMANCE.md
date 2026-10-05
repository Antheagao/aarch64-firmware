# Performance

This file describes how performance is measured and reported.
The numbers themselves go in `docs/perf.md` when M8 produces them.

## Principles

- Measure before optimizing, and optimize the thing the measurement points at.
- Name the metric before the experiment: instructions, cycles, IPC, cache misses, or latency.
- Report the environment with every number, because a number without its setup cannot be reproduced.
- Prefer a better algorithm or data layout over a clever instruction sequence.

## What QEMU can and cannot measure

QEMU's TCG translates guest code to host code, so it does not model the guest pipeline or caches.

| Metric | Meaningful on QEMU? | How |
|---|---|---|
| Instructions retired | Yes, and deterministic | PMU event `0x08` with `-icount shift=0` |
| Cycles | No | QEMU derives them from host time |
| Cache misses, branch mispredicts | No | Not modeled |
| Functional correctness of SIMD and SVE code | Yes | Compare results against a scalar reference |

Without `-icount`, QEMU does not advertise the instructions-retired event: check bit 8 of `PMCEID0_EL0`.

## Real hardware

Use real Arm hardware for anything about time, caches, or the pipeline.

| Machine | Core | Notes |
|---|---|---|
| AWS Graviton4 | Neoverse V2, Armv9.0 | SVE2 with 128-bit vectors |
| AWS Graviton3 | Neoverse V1, Armv8.4 | SVE with 256-bit vectors |
| Raspberry Pi 5 | Cortex-A76, Armv8.2 | No SVE, good for NEON |

Run the benchmark kernels as a Linux user-space program, built from the same source as the firmware version.

Tools:

- `perf stat -e cycles,instructions,cache-misses,branch-misses` for counts.
- `perf record` and `perf report` to find hot spots.
- Arm's top-down methodology for Neoverse cores, with Arm's `topdown-tool`, to see whether a kernel is front-end, back-end, or memory bound.

## Benchmark protocol

1. Pin the process to one core with `taskset`.
2. Set the CPU frequency governor to `performance` where you can.
3. Run a warm-up pass before measuring.
4. Run at least 30 repetitions and report the median and the spread (interquartile range or min and max).
5. Record the compiler and its version, every flag (`-O2`, `-mcpu=neoverse-v2`), the CPU model, and the kernel version.
6. Check the result for correctness on every run, so a fast wrong answer never counts.

## Firmware-level metrics

These can be measured in firmware with the generic timer or the PMU:

- Time from reset to `fw_main()` (`CNTPCT_EL0`)
- SMC round trip from EL1 to EL3 and back
- Interrupt latency from timer expiry to handler entry
- `CPU_ON` latency from request to secondary entry at EL1

On QEMU, report these as instruction counts only.

## Optimization order

1. Algorithm and data structure (see `docs/CODING_STANDARDS.md`).
2. Memory layout: alignment, 64-byte cache lines, and avoiding false sharing between CPUs.
3. Instruction selection: `LDP`/`STP` pairs, LSE atomics instead of exclusives under contention, NEON or SVE for data-parallel loops.
4. Compiler flags: `-mcpu` for the target core, then link-time optimization.

## Results format

Each experiment in `docs/perf.md` gets a table like this one:

| Kernel | Size | Instructions (QEMU) | Cycles (HW) | IPC (HW) | Speedup vs. baseline |
|---|---|---|---|---|---|
| memcpy byte loop | 64 KiB | | | | 1.0x |

Below the table, write one paragraph that explains the result, including any gap between the QEMU and hardware numbers.
