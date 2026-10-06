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
    ("the trap report names the exception class", r"EC=0x3c \(BRK instruction\)"),
    ("the trap report dumps the saved registers", r"trap:   x 0=0x[0-9a-f]{16}"),
    ("the handler steps over an expected fault", r"trap: expected, stepping over it"),
    ("no UART waits timed out during boot", r"uart: tx timeouts = 0\b"),
    ("milestone 0 completes", r"milestone 0: boot OK"),
]


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
    output, status = boot(qemu_command())

    print("----- UART -----")
    print(output.replace("\r", ""), end="")
    print("----------------")

    failures = 0
    for what, pattern in CHECKS:
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
