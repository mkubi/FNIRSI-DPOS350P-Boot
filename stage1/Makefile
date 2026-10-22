CROSS_COMPILE ?= arm-none-eabi-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
NM      := $(CROSS_COMPILE)nm
SIZE    := $(CROSS_COMPILE)size

BUILD := ../build/stage1
TARGET := $(BUILD)/dpos_stage1b2_exe2_load
XFEL := xfel-v3s

CFLAGS := -O2 -mcpu=cortex-a7 -mtune=cortex-a7 -marm \
          -mno-thumb-interwork -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common -fno-stack-protector \
          -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -nostdlib -Wall -Wextra -Werror \
          -I../arch/r11 -I../drivers -I. -I$(XFEL)/include

DRAM_CFLAGS := $(CFLAGS) \
    -Wno-error=array-bounds \
    -Wno-error=sign-compare \
    -Wno-error=unused-parameter

OBJS := \
    $(BUILD)/start.o \
    $(BUILD)/main.o \
    $(BUILD)/gpio.o \
    $(BUILD)/spi0.o \
    $(BUILD)/spi_nor_ro.o \
    $(BUILD)/sys-clock.o \
    $(BUILD)/sys-dram.o \
    $(BUILD)/memcpy.o \
    $(BUILD)/memset.o

.PHONY: all clean check import-xfel verify-import

all: verify-import $(TARGET).bin $(TARGET).lst check
	$(SIZE) $(TARGET).elf

import-xfel:
	./import-xfel-v3s.sh

verify-import:
	@test -f $(XFEL)/source/sys-clock.c || \
	  { echo "Run: make -C stage1 import-xfel"; exit 1; }
	@test -f $(XFEL)/source/sys-dram.c || \
	  { echo "Run: make -C stage1 import-xfel"; exit 1; }

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/main.o: main.c result.h ../drivers/spi_nor_ro.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gpio.o: ../drivers/gpio.c ../drivers/gpio.h ../arch/r11/regs.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/spi0.o: ../drivers/spi0.c ../drivers/spi0.h ../drivers/gpio.h ../arch/r11/regs.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/spi_nor_ro.o: ../drivers/spi_nor_ro.c ../drivers/spi_nor_ro.h ../drivers/spi0.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sys-clock.o: $(XFEL)/source/sys-clock.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sys-dram.o: $(XFEL)/source/sys-dram.c | $(BUILD)
	$(CC) $(DRAM_CFLAGS) -c $< -o $@

$(BUILD)/memcpy.o: $(XFEL)/source/memcpy.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/memset.o: $(XFEL)/source/memset.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) link.ld
	$(LD) -T link.ld -Map=$(TARGET).map $(OBJS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -D $< > $@

check: $(TARGET).elf
	@if $(NM) $(TARGET).elf | grep -q ' U '; then \
		echo "Unexpected unresolved symbols:"; \
		$(NM) $(TARGET).elf | grep ' U '; \
		exit 1; \
	fi

clean:
	rm -rf $(BUILD)
