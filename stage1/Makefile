CROSS_COMPILE ?= arm-none-eabi-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
NM      := $(CROSS_COMPILE)nm
SIZE    := $(CROSS_COMPILE)size

BUILD := ../build/stage1
TARGET := $(BUILD)/dpos_stage1a_xfel_return

CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common \
          -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -nostdlib -Wall -Wextra -Werror

OBJS := $(BUILD)/start.o

.PHONY: all clean check

all: $(TARGET).bin $(TARGET).lst check
	$(SIZE) $(TARGET).elf
	@bytes=$$(stat -c %s $(TARGET).bin); \
	if [ $$bytes -gt 256 ]; then \
		echo "Payload is $$bytes bytes and overlaps result block at +0x100"; \
		exit 1; \
	fi

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: start.S | $(BUILD)
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
