# Firmware campaign: parked rows

Each row says what unblocks it.
When the condition is met, move the row into the queue in `notes/firmware-current.md` and split it into PR-sized rows.
A parked row without an unblock condition is a row nobody ever picks up again.

| Row | Unblocked when |
|---|---|
| M4: MMU and caches at EL1, permission and execute-never fault self-tests | M3 is merged and tagged `v0.3.0` |
| M5: GICv3 and the generic timer | M4 is merged and tagged `v0.4.0` |
| M6: PSCI over SMC and multi-core bring-up with ticket locks | M5 is merged and tagged `v0.5.0` |
| M7: SVE2, PAC/BTI, and MTE enablement with fault demos, `docs/feature-enablement.md` | M6 is merged and tagged `v0.6.0` |
| M7 follow-on: turn on `mte=on` for the QEMU machine and assert MTE present on `-cpu max` | M7 enables MTE, so the assertion tests firmware rather than a QEMU default |
| M8 (QEMU part): PMU instruction counts with `-icount shift=0` | M7 is merged and tagged `v0.7.0` |
| [USER-GATED] M8 (hardware part): `perf stat` runs on AWS Graviton4 | M8 QEMU part is merged and the owner approves the AWS cost |
| S1: boot Linux on this firmware | M6 is merged; it can run in parallel with M7 and M8 |
| CI: a `cppcheck` static-analysis job | Someone can run `cppcheck` locally, or the owner accepts that it can only be iterated through CI |
| CI: build a newer QEMU and cache it | A milestone needs an emulated feature that QEMU 8.2 lacks |
