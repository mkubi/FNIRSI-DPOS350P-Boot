CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size

ROOT := ../..
BUILD := $(ROOT)/build/apps/csi_diag
TARGET := $(BUILD)/DPOSAPP

INCLUDES := \
    -I$(ROOT)/arch/r11 \
    -I$(ROOT)/board/dpos350p \
    -I$(ROOT)/drivers \
    -I$(ROOT)/ui

CSI_DIAG_STAGE ?= 25

CFLAGS := \
    -O2 \
    -mcpu=cortex-a7 \
    -marm \
    -mno-unaligned-access \
    -ffreestanding \
    -fno-builtin \
    -fno-common \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -nostdlib \
    -Wall \
    -Wextra \
    -Werror \
    -DCSI_DIAG_STAGE=$(CSI_DIAG_STAGE) \
    $(INCLUDES)

SOURCES_C := \
    main.c \
    capture_test.c \
    job2_test.c \
    job2_recovery_test.c \
    stage24_fpga_preinit_test.c \
    stage25_stock_id_raw4_test.c \
    stock_parity_test.c \
    stage27a_capture_test.c \
    stage27b_smec_capture_test.c \
    stage29_verified_job1_test.c \
    fpga_primitive_test.c \
    ui_adapter.c \
    $(ROOT)/arch/r11/cache.c \
    $(ROOT)/arch/r11/timer.c \
    $(ROOT)/drivers/gpio.c \
    $(ROOT)/drivers/display.c \
    $(ROOT)/drivers/fpga_bus_ro.c \
    $(ROOT)/drivers/ms5351_stock.c \
    $(ROOT)/drivers/r11_csi.c \
    $(ROOT)/ui/font8x8.c \
    $(ROOT)/ui/console.c

SOURCES_S := \
    start.S

OBJECTS := \
    $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) \
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
	@echo "Custom App signature: OK (E59FD08C)"

clean:
	rm -rf $(BUILD)
