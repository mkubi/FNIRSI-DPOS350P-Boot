# FNIRSI DPOS350P boot manager
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

CFLAGS  := -mcpu=cortex-a7 -marm -O2 -ffreestanding -nostdlib \
           -Wall -Wextra \
           -Iarch/r11 -Iboard/dpos350p -Idrivers -Istorage -Iloader -Iui

LDSCRIPT := link.ld
TARGET   := dpos-boot

OBJS := arch/r11/start.o \
        arch/r11/cache.o \
        drivers/gpio.o \
        drivers/display.o \
        drivers/gt911.o \
        main.o

all: $(TARGET).bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(LD) -T $(LDSCRIPT) $(OBJS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	find . -name '*.o' -o -name '*.elf' -o -name '*.bin' | xargs rm -f

.PHONY: all clean
