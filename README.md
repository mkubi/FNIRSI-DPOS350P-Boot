# FNIRSI-DPOS350P-Boot

Custom boot manager for the FNIRSI DPOS350P oscilloscope (Allwinner R11/V3s).

> **Work in progress — FEL proof-of-concept can now chainload the original FNIRSI applications from SPI NOR.**

## Goal

Replace the original boot flow with a small boot manager that:

1. Initializes clocks and 64 MiB DDR2
2. Initializes the 1024×600 IPS display and GT911 touch controller
3. Shows a boot menu (touchscreen)
4. **Original firmware** — loads the FNIRSI application from SPI NOR and jumps to it
5. **Custom firmware** — loads a custom binary from SD-NAND and jumps to it
6. Preserves hardware FEL recovery

## Hardware

| Component | Detail |
|-----------|--------|
| SoC | Allwinner R11 / V3s (ARM Cortex-A7) |
| DRAM | 64 MiB DDR2 SIP |
| SPI NOR | Winbond, JEDEC `EF 40 15`, 2 MiB — original firmware |
| SD-NAND | CSNP1GCR01-B0W (1 GiB) — planned custom firmware storage |
| Display | 1024×600 IPS RGB565, 50-pin FFC |
| Touch | GT911 capacitive (SDA=PG6, SCL=PG7, INT=PE22, RST=PE24) |

See [FNIRSI-DPOS350P-Hack](https://github.com/mkubi/FNIRSI-DPOS350P-Hack) for
hardware documentation and reverse-engineering notes.

## Current status

The FEL development loader is linked at `0x41000000` and started after
`xfel ddr v3s` initializes DRAM.

Confirmed working on hardware:

- 1024×600 display initialization via DE2/TCON0
- RGB565 framebuffer and on-screen debug console
- SPI0 on PC0..PC3 at 6 MHz, mode 0, manual CS
- Winbond JEDEC read (`0x9F`) → `EF 40 15`
- read-only SPI NOR `0x03` reads
- chunked NOR reads larger than the current 64-byte SPI PIO transaction limit
- direct loading from SPI NOR into DRAM at `0x40000000`
- eGON container parsing and payload loading
- successful chainload of the original FNIRSI USB/updater application
- successful chainload of the original FNIRSI oscilloscope application

### Important compiler flag

The build uses:

```text
-mno-unaligned-access
```

This is required for the current bare-metal environment. During SPI NOR bring-up,
GCC `-O2` generated an unaligned ARM halfword load (`LDRH [sp,#5]`) when processing
a local byte buffer. The firmware stalled before the flag was added. With
`-mno-unaligned-access`, local `uint8_t` buffers work correctly and JEDEC/NOR reads
are stable.

Do not remove this flag without re-validating generated code and alignment handling.

## Reverse-engineered SPI NOR layout

Baseline Winbond image size: `0x200000` bytes (2 MiB).

```text
Flash offset   Container   Declared size   Payload start   Payload size
------------   ---------   -------------   -------------   ------------
0x000000       eGON.BT0    0x006800        -               -
0x018000       eGON.EXE    0x024800        0x018020        0x0247E0
0x058000       eGON.BMP    0x018A00        -               -
0x0A8000       eGON.EXE    0x095800        0x0A8020        0x0957E0
```

The first `eGON.EXE` is the FNIRSI USB/updater application. It contains USB,
SD/MMC and firmware-update functionality.

The second `eGON.EXE` is the normal oscilloscope application.

### eGON.EXE handoff

The original SPL does **not** copy the 32-byte eGON header to the execution address.
It validates the header, strips the first `0x20` bytes, loads the payload to
`0x40000000`, then transfers control with `BLX 0x40000000`.

Confirmed paths:

```text
USB/updater:
NOR 0x018020 -> DRAM 0x40000000
size 0x247E0
BLX 0x40000000

Oscilloscope application:
NOR 0x0A8020 -> DRAM 0x40000000
size 0x957E0
BLX 0x40000000
```

Both paths have been successfully executed from the custom FEL loader.

## Source tree

```text
arch/r11/       R11/V3s startup, clocks, DRAM, cache
board/dpos350p/ Board-specific memory map and pin definitions
drivers/        GPIO, display, SPI NOR; SD/MMC and GT911 work follows
storage/        Block device abstraction, FAT reader
loader/         Image validation and jump stubs
ui/             Console / future boot menu
main.c          Current FEL development entry point
link.ld         FEL loader linked at 0x41000000
```

## Development phases

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | ✅ done | FEL workflow, DRAM, display and debug console |
| 1 | ✅ done | SPI NOR, JEDEC, chunked reads, original eGON.EXE chainload |
| 2 | 🔄 next | SD/MMC raw block driver and sector reads |
| 3 | ⏳ | FAT + custom image loader |
| 4 | ⏳ | GT911 touchscreen + boot menu |
| 5 | ⏳ | Native SRAM BT0/SPL replacing `xfel ddr v3s` |

## Building and running via FEL

```bash
make clean
make

xfel ddr v3s
xfel write 0x41000000 build/dpos_display_spl_clone.bin
xfel exec  0x41000000
```

Requires an ARM bare-metal toolchain providing `arm-none-eabi-gcc` and `xfel`.

## Next milestone: SD/MMC

The next implementation target is read-only SD/MMC access:

1. initialize the V3s SD/MMC controller and card
2. reliably read raw 512-byte sectors
3. expose the card through the block-device abstraction
4. add FAT reading
5. locate and validate a custom boot image
6. load its payload into DRAM and transfer control

The original USB/updater `eGON.EXE` is useful reverse-engineering reference material
because the image contains SD/MMC and firmware-update code.

## FEL recovery

Keep the hardware FEL entry path available during development. The SPI NOR driver
in this project is currently intentionally read-only; firmware development does not
require modifying or erasing the original Winbond contents.

## License

MIT

