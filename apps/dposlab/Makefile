CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size
ROOT := ../..
BUILD := $(ROOT)/build/apps/dposlab
TARGET := $(BUILD)/DPOSLAB
DPOSLAB_MODE ?= 1
DPOSLAB_ACQ_CHECKPOINT ?= 1
DPOSLAB_PRIMITIVE_TEST ?= 2
INCLUDES := -I$(ROOT)/arch/r11 -I$(ROOT)/board/dpos350p -I$(ROOT)/drivers -I$(ROOT)/ui -I$(ROOT)/loader
CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access -ffreestanding -fno-builtin -fno-common -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -nostdlib -Wall -Wextra -Werror -DDPOSLAB_MODE=$(DPOSLAB_MODE) -DDPOSLAB_ACQ_CHECKPOINT=$(DPOSLAB_ACQ_CHECKPOINT) -DDPOSLAB_PRIMITIVE_TEST=$(DPOSLAB_PRIMITIVE_TEST) $(INCLUDES)
SOURCES_C :=     acq_prereq_probe.c     stock_primitive_smec_test.c     main.c     stock_api.c     runtime_patch.c     runtime_snapshot.c     runtime_trace.c     runtime_export.c     $(ROOT)/arch/r11/cache.c     $(ROOT)/drivers/gpio.c     $(ROOT)/drivers/display.c     $(ROOT)/drivers/fpga_bus_ro.c     $(ROOT)/ui/font8x8.c     $(ROOT)/ui/console.c
SOURCES_S := start.S stock_jump.S runtime_hook.S
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) $(patsubst %.S,$(BUILD)/%.o,$(SOURCES_S))
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
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@
$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@
clean:
	rm -rf $(BUILD)
