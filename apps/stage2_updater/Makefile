CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size

ROOT := ../..
BUILD := $(ROOT)/build/apps/stage2_updater
TARGET := $(BUILD)/DPOSAPP
INCLUDES := -I$(ROOT)/arch/r11 -I$(ROOT)/board/dpos350p \
            -I$(ROOT)/drivers -I$(ROOT)/storage -I$(ROOT)/ui

CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common -fno-stack-protector \
          -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -nostdlib -Wall -Wextra -Werror $(INCLUDES)

SOURCES_C := main.c \
    $(ROOT)/arch/r11/cache.c $(ROOT)/arch/r11/timer.c \
    $(ROOT)/drivers/gpio.c $(ROOT)/drivers/display.c \
    $(ROOT)/drivers/sdmmc0.c $(ROOT)/drivers/spi0.c \
    $(ROOT)/drivers/spi_nor_ro.c $(ROOT)/drivers/spi_nor_stage2_rw.c \
    $(ROOT)/storage/fat.c \
    $(ROOT)/ui/font8x8.c $(ROOT)/ui/console.c
SOURCES_S := start.S
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) \
           $(patsubst %.S,$(BUILD)/%.o,$(SOURCES_S))

.PHONY: all clean check
all: $(TARGET).BIN $(TARGET).lst check
	$(SIZE) $(TARGET).elf
$(TARGET).elf: $(OBJECTS) link.ld
	@mkdir -p $(dir $@)
	$(LD) -T link.ld -Map=$(TARGET).map $(OBJECTS) -o $@
$(TARGET).BIN: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $@
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@
$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@
check: $(TARGET).BIN
	@test $$(od -An -tx4 -N4 $(TARGET).BIN | tr -d ' ') = e59fd08c
	@echo "Stage 2 updater U1: signature check OK"
clean:
	rm -rf $(BUILD)
