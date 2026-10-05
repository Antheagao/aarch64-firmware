#ifndef SEMIHOST_H
#define SEMIHOST_H

/* Arm semihosting SYS_EXIT: asks the debugger/emulator to stop and report
 * `code` as the process exit status. Lets CI tell pass from fail without
 * scraping a hung QEMU. Only meaningful under QEMU with -semihosting. */
void semihost_exit(int code) __attribute__((noreturn));

#endif
