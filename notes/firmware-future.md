# Firmware campaign: parked rows

Each row says what unblocks it.
When the condition is met, move the row into the queue in `notes/firmware-current.md` and split it into PR-sized rows.

| Row | Unblocked when |
|---|---|
| M3: EL3 to EL1 hand-off, `docs/el-handoff.md`, `docs/riscv-vs-arm.md` | M2 is merged and tagged `v0.2.0` |
| M4: MMU and caches at EL1, permission and execute-never fault self-tests | M3 is merged and tagged `v0.3.0` |
| M5: GICv3 and the generic timer | M4 is merged and tagged `v0.4.0` |
| M6: PSCI over SMC and multi-core bring-up with ticket locks | M5 is merged and tagged `v0.5.0` |
| M7: SVE2, PAC/BTI, and MTE enablement with fault demos, `docs/feature-enablement.md` | M6 is merged and tagged `v0.6.0` |
| M8 (QEMU part): PMU instruction counts with `-icount shift=0` | M7 is merged and tagged `v0.7.0` |
| [USER-GATED] M8 (hardware part): `perf stat` runs on AWS Graviton4 | M8 QEMU part is merged and the owner approves the AWS cost |
| S1: boot Linux on this firmware | M6 is merged; it can run in parallel with M7 and M8 |
| CI: `clang-tidy` and `cppcheck` job | Row 2 (`.clang-format`) is merged, so style and analysis land in separate PRs |
| CI: build a newer QEMU and cache it | A milestone needs an emulated feature that QEMU 8.2 lacks |
