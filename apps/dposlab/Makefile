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
SMEC_S1 ?= 2
SMEC_S2 ?= 2
SMEC_S3 ?= 2
SMEC_S4 ?= 1
FPGA_F1 ?= 1
FPGA_F2 ?= 2
FPGA_F3 ?= 1
FPGA_F4 ?= 1
HORIZONTAL_H1 ?= 0
HORIZONTAL_H2 ?= 0
HORIZONTAL_H3 ?= 0
HORIZONTAL_H4 ?= 0
HORIZONTAL_H5 ?= 0
HORIZONTAL_H6 ?= 0
SYSCFG_G1 ?= 0
SYSCFG_G2 ?= 0
SYSCFG_G3 ?= 0
SYSCFG_SC1_1 ?= 0
SYSCFG_SC1_2 ?= 0
SYSCFG_SC1_3 ?= 0
SYSCFG_SC1_4 ?= 0
SYSCFG_SC1_5 ?= 0
INCLUDES := -I$(ROOT)/arch/r11 -I$(ROOT)/board/dpos350p -I$(ROOT)/drivers -I$(ROOT)/ui -I$(ROOT)/loader
CFLAGS := -O2 -mcpu=cortex-a7 -marm -mno-unaligned-access -ffreestanding -fno-builtin -fno-common -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -nostdlib -Wall -Wextra -Werror -DDPOSLAB_MODE=$(DPOSLAB_MODE) -DDPOSLAB_ACQ_CHECKPOINT=$(DPOSLAB_ACQ_CHECKPOINT) -DDPOSLAB_PRIMITIVE_TEST=$(DPOSLAB_PRIMITIVE_TEST) -DSMEC_S1=$(SMEC_S1) -DSMEC_S2=$(SMEC_S2) -DSMEC_S3=$(SMEC_S3) -DSMEC_S4=$(SMEC_S4) -DFPGA_F1=$(FPGA_F1) -DFPGA_F2=$(FPGA_F2) -DFPGA_F3=$(FPGA_F3) -DFPGA_F4=$(FPGA_F4) -DHORIZONTAL_H1=$(HORIZONTAL_H1) -DHORIZONTAL_H2=$(HORIZONTAL_H2) -DHORIZONTAL_H3=$(HORIZONTAL_H3) -DHORIZONTAL_H4=$(HORIZONTAL_H4) -DHORIZONTAL_H5=$(HORIZONTAL_H5) -DHORIZONTAL_H6=$(HORIZONTAL_H6) -DSYSCFG_G1=$(SYSCFG_G1) -DSYSCFG_G2=$(SYSCFG_G2) -DSYSCFG_G3=$(SYSCFG_G3) -DSYSCFG_SC1_1=$(SYSCFG_SC1_1) -DSYSCFG_SC1_2=$(SYSCFG_SC1_2) -DSYSCFG_SC1_3=$(SYSCFG_SC1_3) -DSYSCFG_SC1_4=$(SYSCFG_SC1_4) -DSYSCFG_SC1_5=$(SYSCFG_SC1_5) $(INCLUDES)
SOURCES_C := sysconfig_group_matrix.c     horizontal_matrix.c         acq_prereq_probe.c     stock_primitive_smec_test.c     main.c     stock_api.c     runtime_patch.c     runtime_snapshot.c     runtime_trace.c     runtime_export.c     $(ROOT)/arch/r11/cache.c     $(ROOT)/drivers/gpio.c     $(ROOT)/drivers/display.c     $(ROOT)/drivers/fpga_bus_ro.c     $(ROOT)/ui/font8x8.c     $(ROOT)/ui/console.c     custom_fpga_start.c     $(ROOT)/arch/r11/timer.c     $(ROOT)/drivers/ms5351_stock.c     $(ROOT)/drivers/r11_csi.c     smec_matrix.c     csi_bank0_probe.c
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
