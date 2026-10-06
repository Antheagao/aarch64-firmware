# Bare-metal AArch64 firmware for the QEMU virt machine.
#
#   make            build build/fw.bin
#   make run        boot it in QEMU (it exits on its own; Ctrl-a x force-quits)
#   make debug      boot paused, waiting for GDB on localhost:1234
#   make gdb        attach GDB to a running `make debug`
#   make test       boot and assert on the UART output (what CI runs)
#   make format     reformat C sources and headers with clang-format
#   make format-check  fail if any C source or header is misformatted
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
OBJS := $(patsubst src/%,$(BUILD)/%.o,$(SRCS))

QEMU      ?= qemu-system-aarch64
GDB       ?= gdb-multiarch
CPU       ?= max
SMP       ?= 4
MACHINE   := virt,secure=on,virtualization=on,gic-version=3
QEMUFLAGS := -M $(MACHINE) -cpu $(CPU) -smp $(SMP) -m 512M -nographic \
             -bios $(BIN) -semihosting-config enable=on,target=native

.PHONY: all run debug gdb test format format-check qemu-cmd disasm clean

all: $(BIN)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.c.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.S.o: src/%.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

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
FORMAT_SRCS  := $(wildcard src/*.c include/*.h)

format:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SRCS)

# Used by the test harness so it can run (and kill) QEMU directly.
qemu-cmd:
	@echo $(QEMU) $(QEMUFLAGS)

disasm: $(ELF)
	$(OBJDUMP) -d $<

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
