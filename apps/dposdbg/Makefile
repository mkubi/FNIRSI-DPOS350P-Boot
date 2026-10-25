CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size

ROOT := ../..
BUILD := $(ROOT)/build/apps/dposdbg
TARGET := $(BUILD)/DPOSDBG

CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common -fno-stack-protector \
          -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -nostdlib -Wall -Wextra -Werror

OBJECTS := $(BUILD)/header.o $(BUILD)/hook.o \
           $(BUILD)/primitive_hooks.o $(BUILD)/plugin.o

.PHONY: all clean check

all: $(TARGET).BIN $(TARGET).lst check
	$(SIZE) $(TARGET).elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/header.o: header.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/hook.o: hook.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/primitive_hooks.o: primitive_hooks.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/plugin.o: plugin.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) link.ld
	$(LD) -T link.ld -Map=$(TARGET).map $(OBJECTS) -o $@

$(TARGET).BIN: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $@

check: $(TARGET).BIN
	@test $$(od -An -tx4 -N4 $(TARGET).BIN | tr -d ' ') = 30474244
	@test $$(od -An -tu4 -j4 -N4 $(TARGET).BIN | tr -d ' ') = 1
	@echo "DPOSDBG header: OK"

clean:
	rm -rf $(BUILD)
