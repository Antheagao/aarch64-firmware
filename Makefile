# Bare-metal AArch64 firmware for the QEMU virt machine.
#
#   make            build build/fw.bin
#   make run        boot it in QEMU (it exits on its own; Ctrl-a x force-quits)
#   make debug      boot paused, waiting for GDB on localhost:1234
#   make gdb        attach GDB to a running `make debug`
#   make test       boot and assert on the UART output (what CI runs)
#   make format     reformat C sources and headers with clang-format
#   make format-check  fail if any C source or header is misformatted
#   make syntax-check  compile-check the C sources with the host compiler
#   make unit       run the host unit tests under ASan and UBSan
#   make LLVM=1     build with clang + lld instead of GCC + binutils
#   make CPU=cortex-a57 run   try a different core (default: max, i.e. Armv9 features)

BUILD := build
ELF   := $(BUILD)/fw.elf
BIN   := $(BUILD)/fw.bin

ifeq ($(LLVM),1)
CC      := clang --target=aarch64-none-elf
LD      := ld.lld
OBJCOPY := llvm-objcopy
OBJDUMP := llvm-objdump
else
ifndef CROSS_COMPILE
CROSS_COMPILE := $(shell for p in aarch64-none-elf- aarch64-elf- aarch64-linux-gnu-; do \
	if command -v $${p}gcc >/dev/null 2>&1; then echo $$p; break; fi; done)
endif
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
endif

# -mgeneral-regs-only: no FP/SIMD until we enable it ourselves (CPTR_EL3).
# -mstrict-align: with the MMU off all memory is Device, where unaligned
#                 accesses fault.
CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Werror \
          -ffreestanding -fno-builtin -fno-stack-protector -fno-pie \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -mgeneral-regs-only -mstrict-align -mno-outline-atomics \
          -Iinclude -MMD -MP
ASFLAGS := -g -Iinclude -MMD -MP
LDFLAGS := -nostdlib -static -T linker.ld

SRCS := $(wildcard src/*.c src/*.S)
KERNEL_SRCS := kernel/start.S kernel/main.c src/uart.c src/kprintf.c src/semihost.c src/vectors.S src/trap.c

# src/vectors.S and src/trap.c are built into both images. TRAP_EL picks the
# banked syndrome registers and the level the crash report names, so one
# crash reporter serves both rather than two copies drifting apart.
FW_DEFS     := -DTRAP_EL=3
KERNEL_DEFS := -DTRAP_EL=1
OBJS := $(patsubst src/%,$(BUILD)/%.o,$(SRCS))

QEMU      ?= qemu-system-aarch64
GDB       ?= gdb-multiarch
CPU       ?= max
SMP       ?= 4
MACHINE   := virt,secure=on,virtualization=on,gic-version=3
QEMUFLAGS := -M $(MACHINE) -cpu $(CPU) -smp $(SMP) -m 512M -nographic \
             -bios $(BIN) -semihosting-config enable=on,target=native

.PHONY: all run debug gdb test format format-check syntax-check unit kernel qemu-cmd disasm clean

all: $(BIN)

kernel: $(BUILD)/kernel.bin

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.c.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(FW_DEFS) -c $< -o $@

$(BUILD)/%.S.o: src/%.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) $(FW_DEFS) -c $< -o $@

# The EL1 image: a second link of the same compiler flags against its own
# linker script, so it lands at DRAM_BASE instead of flash. Its objects go
# under build/kernel/ keyed by source path, so src/uart.c can be built once
# for each image without the two colliding.
KERNEL_OBJS := $(addprefix $(BUILD)/kernel/,$(addsuffix .o,$(KERNEL_SRCS)))

$(BUILD)/kernel/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(KERNEL_DEFS) -c $< -o $@

$(BUILD)/kernel/%.S.o: %.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) $(KERNEL_DEFS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJS) kernel/kernel.ld
	$(LD) -nostdlib -static -T kernel/kernel.ld -o $@ $(KERNEL_OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# src/kernel_image.S pulls the blob in with .incbin, so it cannot be
# assembled until the blob exists.
$(BUILD)/kernel_image.S.o: $(BUILD)/kernel.bin

run: $(BIN)
	$(QEMU) $(QEMUFLAGS)

debug: $(BIN)
	$(QEMU) $(QEMUFLAGS) -S -gdb tcp::1234

gdb:
	$(GDB) $(ELF) -ex 'target remote :1234'

test: $(BIN)
	python3 tests/run_tests.py

# Formatting. CI pins clang-format-18, the version Ubuntu 24.04 ships, because
# the output differs between major versions. Assembly is not covered:
# clang-format has no AArch64 asm support, so src/*.S follows review alone.
CLANG_FORMAT ?= clang-format
FORMAT_SRCS  := $(wildcard src/*.c kernel/*.c include/*.h)

format:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SRCS)

# Host syntax check. Catches C errors without a cross toolchain, so a typo is
# caught in seconds instead of waiting for a CI build. -fsyntax-only stops
# before assembly, so the AArch64 inline asm in sysreg.h is parsed as a string
# and never assembled; that is what lets a host compiler check this code at
# all. Target-specific flags (-mstrict-align, -mgeneral-regs-only) are
# deliberately absent: they are not valid for the host.
HOST_CC ?= cc
HOST_CFLAGS := -fsyntax-only -std=gnu11 -Wall -Wextra -Werror -ffreestanding -Iinclude

syntax-check:
# Both configurations are compiled: src/trap.c and src/vectors.S are built
# into each image with a different TRAP_EL, so checking only one leaves half
# the code unchecked. A guarded block going unused is exactly what CI caught.
	@for el in 3 1; do for f in $(wildcard src/*.c kernel/*.c); do echo "  SYNTAX  TRAP_EL=$$el $$f"; $(HOST_CC) $(HOST_CFLAGS) -DTRAP_EL=$$el $$f || exit 1; done; done

# Host unit tests. Logic that takes values as arguments and returns results
# can be compiled for the host and tested in milliseconds, with sanitizers
# that QEMU cannot give us. The firmware sources are compiled unchanged; the
# only substitution is tests/unit/fake_uart.c in place of the PL011 driver,
# so the code under test is the code that ships.
UNIT_BIN    := $(BUILD)/unit
UNIT_SRCS   := src/kprintf.c src/cpuid.c tests/unit/fake_uart.c tests/unit/unit.c tests/unit/main.c tests/unit/test_kprintf.c tests/unit/test_cpuid.c
UNIT_CFLAGS := -std=gnu11 -g -O1 -Wall -Wextra -Werror -Iinclude -Itests/unit -DTRAP_EL=3                -fsanitize=address,undefined -fno-sanitize-recover=all

unit: $(UNIT_BIN)
	$(UNIT_BIN)

$(UNIT_BIN): $(UNIT_SRCS)
	@mkdir -p $(BUILD)
	$(HOST_CC) $(UNIT_CFLAGS) $^ -o $@

# Used by the test harness so it can run (and kill) QEMU directly.
qemu-cmd:
	@echo $(QEMU) $(QEMUFLAGS)

disasm: $(ELF)
	$(OBJDUMP) -d $<

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d) $(KERNEL_OBJS:.o=.d)
