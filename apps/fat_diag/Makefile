CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size

ROOT := ../..
BUILD := $(ROOT)/build/apps/fat_diag
TARGET := $(BUILD)/DPOSLAB

INCLUDES := -I$(ROOT) -I$(ROOT)/arch/r11 -I$(ROOT)/loader -I$(ROOT)/drivers -I$(ROOT)/ui
CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access \
          -ffreestanding -fno-builtin -fno-common -fno-stack-protector \
          -fno-unwind-tables -fno-asynchronous-unwind-tables -nostdlib \
          -Wall -Wextra -Werror $(INCLUDES)

OBJECTS := \
        $(BUILD)/main.o \
        $(BUILD)/cache.o \
        $(BUILD)/mmu.o \
        $(BUILD)/irq.o \
        $(BUILD)/timer.o \
        $(BUILD)/gpio.o \
        $(BUILD)/sdmmc0.o \
        $(BUILD)/display.o \
        $(BUILD)/console.o \
        $(BUILD)/font8x8.o \
        $(BUILD)/fat_diag_patch.o \
        $(BUILD)/loader_jump.o \
        $(BUILD)/start.o

.PHONY: all clean
all: $(TARGET).BIN $(TARGET).lst
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJECTS) link.ld
	@mkdir -p $(dir $@)
	$(LD) -T link.ld -Map=$(TARGET).map $(OBJECTS) -o $@

$(TARGET).BIN: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $@

$(BUILD)/main.o: main.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/cache.o: $(ROOT)/arch/r11/cache.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/mmu.o: $(ROOT)/arch/r11/mmu.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/irq.o: $(ROOT)/arch/r11/irq.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/timer.o: $(ROOT)/arch/r11/timer.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/gpio.o: $(ROOT)/drivers/gpio.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/sdmmc0.o: $(ROOT)/drivers/sdmmc0.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/display.o: $(ROOT)/drivers/display.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/console.o: $(ROOT)/ui/console.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/font8x8.o: $(ROOT)/ui/font8x8.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/fat_diag_patch.o: fat_diag_patch.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/loader_jump.o: $(ROOT)/loader/jump.S
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/start.o: start.S
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)
