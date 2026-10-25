CROSS_COMPILE ?= arm-none-eabi-
.DEFAULT_GOAL := all
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size
BUILD := build
TARGET := $(BUILD)/dpos_display_spl_clone

MKSTG2 := tools/mkstg2.py
STAGE2_SLOT := $(BUILD)/STAGE2.BIN

INCLUDES := -I. -Iarch/r11 -Iboard/dpos350p -Idrivers -Iloader -Istorage -Iui

CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common \
          -fno-unwind-tables -fno-asynchronous-unwind-tables \
          -nostdlib -Wall -Wextra $(INCLUDES)

SOURCES_C := \
    arch/r11/cache.c \
    arch/r11/timer.c \
    drivers/gpio.c \
    drivers/display.c \
    drivers/spi0.c \
    drivers/spi_nor_ro.c \
    drivers/sdmmc0.c \
    drivers/gt911.c \
    storage/fat.c \
    loader/custom.c \
    loader/debug_exe2.c \
    loader/dposdbg.c \
    loader/original.c \
    loader/start.c \
    loader/boot_targets.c \
    ui/font8x8.c \
    ui/console.c \
    ui/boot_menu.c \
    main.c
SOURCES_S := \
    arch/r11/start.S \
    loader/jump.S

OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) $(patsubst %.S,$(BUILD)/%.o,$(SOURCES_S))
.PHONY: all clean check stage1 stage1-clean dposdbg dposdbg-clean

stage1:
	$(MAKE) -C stage1

stage1-clean: 
	$(MAKE) -C stage1 clean

dposdbg:
	$(MAKE) -C apps/dposdbg

dposdbg-clean:
	$(MAKE) -C apps/dposdbg clean

all: check $(TARGET).bin $(TARGET).lst $(STAGE2_SLOT)
	$(SIZE) $(TARGET).elf
	@echo
	@echo "Stage 2 deployment image ready:"
	@echo "  $(STAGE2_SLOT)"

check:
	@! grep -R '"platform.h"' arch board drivers loader storage ui --include='*.c' --include='*.h' || (echo 'ERROR: stale platform.h include'; exit 1)
$(TARGET).elf: $(OBJECTS) link.ld
	@mkdir -p $(dir $@)
	$(LD) -T link.ld -Map=$(TARGET).map $(OBJECTS) -o $@
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $@
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@
$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@
$(STAGE2_SLOT): $(TARGET).bin $(MKSTG2)
	python3 $(MKSTG2) $(TARGET).bin $@
clean:
	rm -rf $(BUILD)
