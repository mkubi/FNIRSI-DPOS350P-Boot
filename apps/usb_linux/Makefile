CROSS_COMPILE ?= arm-none-eabi-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
NM      := $(CROSS_COMPILE)nm
SIZE    := $(CROSS_COMPILE)size

ROOT  := ../..
BUILD := $(ROOT)/build/apps/usb_linux
TARGET := $(BUILD)/USB_LINUX

INCLUDES := -I$(ROOT)/arch/r11 -I$(ROOT)/drivers -I.
CFLAGS := -O2 -mcpu=cortex-a7 -mtune=cortex-a7 -marm \
          -mno-thumb-interwork -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common -fno-stack-protector \
          -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -nostdlib -Wall -Wextra -Werror $(INCLUDES)

SOURCES_C := \
    main.c \
    touch_reboot.c \
    $(ROOT)/arch/r11/irq.c \
    $(ROOT)/arch/r11/timer.c \
    $(ROOT)/drivers/gpio.c \
    $(ROOT)/drivers/gt911.c \
    $(ROOT)/drivers/sdmmc0.c

SOURCES_S := start.S

OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) \
           $(patsubst %.S,$(BUILD)/%.o,$(SOURCES_S))

.PHONY: all clean check

all: $(TARGET).bin $(TARGET).lst check
	$(SIZE) $(TARGET).elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) link.ld
	$(LD) -T link.ld -Map=$(TARGET).map $(OBJECTS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $@

check: $(TARGET).elf
	@if $(NM) $(TARGET).elf | grep -q ' U '; then \
	    $(NM) $(TARGET).elf | grep ' U '; \
	    exit 1; \
	fi

clean:
	rm -rf $(BUILD)
