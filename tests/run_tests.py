#!/usr/bin/env python3
"""Boot the firmware in QEMU and assert on what it prints over the UART.

Every milestone in docs/ROADMAP.md adds the lines it must produce to CHECKS,
so CI proves on every push that each milestone still works. Stdlib only.

    python3 tests/run_tests.py              # uses the Makefile's QEMU command
    make test CPU=cortex-a57                # same, different core
"""
import os
import re
import shlex
import signal
import subprocess
import sys

BOOT_TIMEOUT_S = 30

# (what it proves, regex that must appear in the UART output)
CHECKS = [
    ("boots from the reset vector at EL3", r"reset vector reached at EL3"),
    ("reads MIDR_EL1", r"MIDR_EL1  = 0x[0-9a-f]{16}"),
    ("primary CPU is affinity 0.0.0", r"MPIDR_EL1 = 0x[0-9a-f]{10}000000"),
    ("prints VBAR_EL3", r"VBAR_EL3  = 0x[0-9a-f]{16}"),
    ("installs the EL3 vector table in VBAR_EL3", r"el3: vector table installed: ok"),
    ("vector table is 2 KiB aligned as VBAR_EL3 requires",
     r"el3: vector table 2 KiB aligned: ok"),
    ("EL3 alignment and stack-alignment checks are on",
     r"el3: alignment and stack checks on: ok"),
    ("bounded poll gives up on a stuck bit",
     r"selftest: poll gives up on a stuck bit after \d+ spins: ok"),
    ("bounded poll returns on a clear bit", r"selftest: poll returns at once on a clear bit: ok"),
    ("brk #0 traps to EL3 and decodes as BRK",
     r"selftest: brk #0 trapped: EC=0x3c: ok"),
    ("the trap report names the exception class", r"trap: EL3 .*EC=0x3c \(BRK instruction\)"),
    ("the trap report dumps the saved registers", r"trap:   x 0=0x[0-9a-f]{16}"),
    ("the handler steps over an expected fault", r"trap: expected, stepping over it"),
    ("an unaligned load traps as a same-EL data abort",
     r"selftest: unaligned load trapped: EC=0x25 DFSC=0x21: ok"),
    ("the abort report decodes the fault status and address",
     r"DFSC=0x21 \(alignment fault\)"),
    ("the EL1 image is embedded in the firmware", r"kernel: image \d+ bytes at 0x[0-9a-f]{16}"),
    ("the EL1 image is copied into DRAM and reads back identical",
     r"kernel: copied to 0x0000000040000000: ok"),
    ("no UART waits timed out during boot", r"uart: tx timeouts = 0\b"),
    ("milestone 0 completes", r"milestone 0: boot OK"),
    ("EL3 reports the security state it configured, which only EL3 can know",
     r"el3: entering EL1 \(Non-secure\) at 0x0000000040000000"),
    ("the EL1 image runs at EL1 after the eret", r"kernel: running at EL1"),
    ("EL1 builds its translation tables", r"kernel: page tables built, \d+ of \d+ tables used"),
    ("EL1 runs with the MMU on", r"kernel: MMU enabled: ok"),
    ("each EL1 section is mapped with its own permissions",
     r"kernel: text 0x[0-9a-f]+-0x[0-9a-f]+ RX, rodata 0x[0-9a-f]+-0x[0-9a-f]+ RO, "
     r"data 0x[0-9a-f]+-0x[0-9a-f]+ RW\+XN"),
    ("EL1 installs its own vector table in VBAR_EL1",
     r"kernel: vector table installed: ok"),
    ("a fault at EL1 is reported by EL1, not escalated to EL3",
     r"trap: EL1 vector=4 \(sync_cur_spx\) EC=0x3c"),
    ("an smc from EL1 is taken at EL3 through the lower-EL vector",
     r"trap: EL3 vector=8 \(sync_lower_a64\) EC=0x17"),
    ("EL3 returns control to EL1 after the smc", r"kernel: returned from smc: ok"),
    ("writing to rodata is refused by the MMU",
     r"kernel: rodata write trapped: EC=0x25 DFSC=0x0f: ok"),
    ("executing from data is refused by the MMU",
     r"kernel: execute from data trapped: EC=0x21: ok"),
    ("EL1 recovers from its own fault and keeps running",
     r"kernel: brk #0 trapped at EL1: EC=0x3c: ok"),
]


# Checks that depend on which core QEMU is emulating. Reporting features the
# hardware does not have is the failure mode M2 exists to prevent, so the
# same firmware is asserted against both an Armv9 core and an Armv8.0 one.
CPU_CHECKS = {
    "max": [
        ("an Armv9 core reports SVE2", r"SVE2 = yes"),
        ("an Armv9 core reports BTI", r"BTI = yes"),
        ("an Armv9 core reports PAC", r"PAC \(APA\) = yes"),
        ("an Armv9 core reports 4K granule support", r"granule 4K = yes"),
    ],
    "cortex-a57": [
        ("an Armv8.0 core reports no SVE2", r"SVE2 = no"),
        ("an Armv8.0 core reports no BTI", r"BTI = no"),
        ("an Armv8.0 core reports no PAC", r"PAC \(APA\) = no"),
        ("an Armv8.0 core reports no MTE", r"MTE = no"),
    ],
}


def cpu_from(cmd):
    """Which core the Makefile is pointing QEMU at, so the right expectations
    are applied. Unknown cores run the common checks only."""
    return cmd[cmd.index("-cpu") + 1] if "-cpu" in cmd else None


def qemu_command():
    out = subprocess.run(["make", "-s", "qemu-cmd"], check=True,
                         capture_output=True, text=True).stdout
    return shlex.split(out.strip())


def boot(cmd):
    """Run QEMU in its own process group so a hang can be killed cleanly."""
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True,
                            start_new_session=True)
    try:
        output, _ = proc.communicate(timeout=BOOT_TIMEOUT_S)
        return output, proc.returncode
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        output, _ = proc.communicate()
        return output, None


def main():
    subprocess.run(["make", "-s"], check=True)
    cmd = qemu_command()
    cpu = cpu_from(cmd)
    output, status = boot(cmd)

    print("----- UART -----")
    print(output.replace("\r", ""), end="")
    print("----------------")

    failures = 0
    checks = CHECKS + CPU_CHECKS.get(cpu, [])
    print(f"checks for -cpu {cpu}: {len(checks)}")
    for what, pattern in checks:
        ok = re.search(pattern, output) is not None
        failures += not ok
        print(f"{'PASS' if ok else 'FAIL'}  {what}")

    if status is None:
        failures += 1
        print(f"FAIL  firmware exits on its own (QEMU killed after {BOOT_TIMEOUT_S}s)")
    elif status != 0:
        failures += 1
        print(f"FAIL  firmware exits with status 0 (got {status})")
    else:
        print("PASS  firmware exits with status 0 via semihosting")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
