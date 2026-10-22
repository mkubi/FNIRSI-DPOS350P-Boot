# FNIRSI-DPOS350P-Boot

Custom bare-metal boot manager for the FNIRSI DPOS350P oscilloscope
(Allwinner R11 / V3s, ARM Cortex-A7).

> **Status:** The FEL-based Stage 2 boot manager is functional on real hardware.
> It can boot a custom application from SD-NAND and both original FNIRSI
> applications from SPI NOR. Autonomous power-on boot still requires the SRAM
> Stage 1 loader and native DRAM initialization.

## Goals

The project aims to provide a small boot manager that can:

- initialize and use the 1024x600 display,
- present a GT911 touchscreen boot menu,
- boot a custom ARM application from internal SD-NAND,
- boot the original FNIRSI oscilloscope application from SPI NOR,
- boot the original standalone USB Connect application from SPI NOR,
- preserve a reliable FEL recovery path,
- eventually boot autonomously from SPI NOR without `xfel ddr v3s`.

The intended final architecture is:

```text
BootROM
   |
   +-- SRAM Stage 1
   |     +-- clocks
   |     +-- DDR2 initialization
   |     +-- load Stage 2 from SPI NOR
   |
   +-- Stage 2 Boot Manager @ 0x41000000
         +-- display + touch UI
         +-- SD-NAND / FAT
         +-- original SPI NOR images
         +-- selected application @ 0x40000000
```

## Hardware

- **SoC:** Allwinner R11 / V3s compatible Cortex-A7
- **DRAM:** 64 MiB DDR2 SIP
- **SPI NOR:** Winbond 2 MiB, JEDEC `EF 40 15`
- **SD-NAND:** CSNP1GCR01-B0W
- **Display:** 1024x600 RGB565 IPS
- **Touch:** GT911 capacitive controller

Confirmed GT911 wiring:

```text
PE22  INT
PE24  RESET
PG6   SDA
PG7   SCL
```

Hardware documentation and earlier reverse-engineering work live in the
companion `FNIRSI-DPOS350P-Hack` repository.

## Current status

Confirmed working on real hardware:

- 1024x600 DE2/TCON0 display initialization
- RGB565 framebuffer and on-screen debug console
- SPI0 and read-only SPI NOR access
- Winbond JEDEC `EF 40 15`
- chunked SPI NOR reads
- eGON image parsing and payload loading
- original EXE1 / USB Connect chainload
- original EXE2 / oscilloscope chainload
- SDC0 and internal SD-NAND initialization
- repeated CMD17 512-byte sector reads through PIO FIFO
- MBR parsing
- FAT16/FAT32 read-only filesystem support
- FAT cluster-chain traversal
- `DPOSAPP.BIN` lookup and complete file loading
- cache-safe application handoff through BLX
- hardware Timer 0 based autoboot timing
- GT911 GPIO bit-banged I2C
- touch-controlled boot target selection
- autoboot fallback when touch is unavailable

Current boot targets:

```text
CUSTOM APPLICATION
ORIGINAL OSCILLOSCOPE
USB CONNECT
```

## Stage 2 boot flow

The current development build is linked at `0x41000000` and uses DRAM that has
already been initialized by XFEL.

```text
xfel ddr v3s
      |
      v
Stage 2 @ 0x41000000
      |
      +-- initialize display and timer
      +-- probe DPOSAPP.BIN
      +-- probe original EXE2
      +-- probe original EXE1
      +-- initialize GT911
      +-- show boot menu
      |
      v
load selected target @ 0x40000000
      |
      +-- clean D-cache range
      +-- invalidate I-cache
      +-- invalidate branch predictor
      +-- DSB / ISB
      +-- disable IRQ/FIQ
      |
      v
BLX 0x40000000
```

Without touch input the first available target is started automatically after
an autoboot timeout. A real touch outside a target cancels autoboot and leaves
the menu active.

## Boot targets

### Custom application

The custom application is loaded from the root of the internal FAT filesystem:

```text
Filename:       DPOSAPP.BIN
FAT 8.3 name:   DPOSAPP BIN
Load address:   0x40000000
Entry address:  0x40000000
Header skip:    0
```

The current test application is a raw ARM binary. The FAT loader follows the
cluster chain, so the file does not need to be physically contiguous.

### Original oscilloscope, EXE2

```text
Header offset:   0x000A8000
Total size:      0x00095800
Payload offset:  0x000A8020
Payload size:    0x000957E0
Load address:    0x40000000
Entry address:   0x40000000
```

### USB Connect, EXE1

```text
Header offset:   0x00018000
Total size:      0x00024800
Payload offset:  0x00018020
Payload size:    0x000247E0
Load address:    0x40000000
Entry address:   0x40000000
```

The project deliberately calls EXE1 **USB Connect**. Although the image contains
USB, SD/MMC and firmware-related code, its complete update behavior is not yet
understood well enough to label the boot target as an updater.

## eGON handoff

The original SPL does not copy the 32-byte `eGON.EXE` header to the execution
address. The boot manager reproduces this behavior:

```text
validate header
skip 0x20-byte eGON header
copy payload to 0x40000000
BLX 0x40000000
```

Observed EXE2 header:

```text
+0x00  0xEA000006
+0x04  "eGON"
+0x08  ".EXE"
+0x0C  0x00000003
+0x10  0x00095800
+0x14  "EXEC"
+0x18  0x00000000
+0x1C  0x00000000
```

Both original images have been successfully loaded and started using this
handoff.

## Internal SD-NAND and filesystem

Confirmed SDC0 Port F configuration includes:

```text
PF0..PF5 function: 2
PF_CFG0:           0x07222222
PF_PUL0:           0x00000555
```

An important GPIO correction found during bring-up was:

```c
#define GPIO_PORT_F 0xB4U
```

GPIO function fields use four-bit masks (`0xF`), not three-bit masks.

The tested internal filesystem is FAT16:

```text
Partition type:    0x06
Start LBA:         0x0000003F
Sector count:      0x0003C7C1
MBR signature:     0x55AA
```

The filesystem layer supports read-only FAT16/FAT32 autodetection, root
8.3-name lookup and cluster-chain traversal.

## GT911 touch

The GT911 driver currently uses GPIO bit-banged I2C:

```text
7-bit address: 0x5D
write address: 0xBA
read address:  0xBB

0x8140  Product ID
0x814E  coordinate status
0x8150  first touch point
```

Current menu behavior:

```text
No touch:
    autoboot continues

Touch on an available target:
    target starts immediately

Touch outside a target:
    autoboot is cancelled
    menu waits for selection

Release / empty event:
    does not cancel autoboot
    does not overwrite the last valid coordinates

Touch communication failure:
    touch is disabled
    autoboot remains available as fallback
```

The touch driver is functional but not considered final. Some physical touches
appear to be missed intermittently. Reset timing, INT handling, GPIO pulls,
bit-banged I2C timing and polling behavior remain follow-up items.

## Hardware timer

Timer 0 is used as a free-running 32-bit down-counter from OSC24M:

```text
Base:       0x01C20C00
Control:    0x01C20C10
Interval:   0x01C20C14
Current:    0x01C20C18
Clock:      24 MHz
```

It provides millisecond/microsecond delays, autoboot deadlines and touch
polling timing.

## Important compiler notes

The build currently requires:

```text
-mno-unaligned-access
```

During SPI NOR bring-up GCC `-O2` generated an unaligned ARM halfword load from
a local byte buffer, causing JEDEC reads to stall. The flag fixed the issue and
remains part of the known-good baseline.

The project is freestanding and does not link libc. GCC can still lower a
structure assignment into an external `memcpy()` call. Small structures are
therefore copied field by field where necessary.

## Source tree

```text
arch/r11/       R11/V3s startup, cache, timer; future clocks and DRAM
board/dpos350p/ Board-specific configuration and memory map
drivers/        Display, GPIO, SPI NOR, SDC0, GT911
storage/        FAT16/FAT32 read-only filesystem
loader/         Custom/original loaders, boot targets and handoff
ui/             Framebuffer console and touch boot menu
main.c          Stage 2 boot-manager entry point
link.ld         Stage 2 linked at 0x41000000
PLAN.md         Detailed development roadmap
```

## Development status

```text
FEL workflow / display / console       DONE
SPI NOR and original image loading     DONE
SDC0 / SD-NAND sector reads            DONE
FAT16/FAT32 read-only support          DONE
DPOSAPP.BIN loader                     DONE
EXE1 / EXE2 loaders                    DONE
Cache-safe BLX handoff                 DONE
Hardware timed boot menu               DONE
GT911 touch boot selection             FUNCTIONAL
GT911 reliability cleanup              TODO
SRAM Stage 1                           TODO
Native clock / DRAM initialization     TODO
Autonomous SPI NOR boot                TODO
```

## Building and running through FEL

```bash
make clean
make

xfel ddr v3s
xfel write 0x41000000 build/dpos_display_spl_clone.bin
xfel exec  0x41000000
```

Required tools include an `arm-none-eabi` bare-metal toolchain and `xfel`.

## Next major milestone

The next major low-level milestone is an SRAM-resident Stage 1 so that the
boot manager no longer depends on `xfel ddr v3s`:

```text
BootROM
  |
  +-- SRAM Stage 1
        +-- clock initialization
        +-- DDR2 initialization
        +-- DRAM test
        +-- SPI NOR Stage 2 load
        |
        +-- Stage 2 @ 0x41000000
```

The V3s/S3 DDR initialization used by XFEL is the primary reference for the
first native DRAM implementation. Mainline U-Boot and the original FNIRSI
`eGON.BT0` are additional references.

## Recovery

Keep FEL recovery available during development.

Before modifying boot-critical SPI NOR contents:

1. keep a verified full SPI NOR backup,
2. verify FEL entry and recovery wiring,
3. test Stage 1 entirely from SRAM,
4. verify native DRAM initialization,
5. verify Stage 1 -> Stage 2 loading,
6. only then install a persistent Stage 1.

The current Stage 2 SPI NOR driver is intentionally read-only.

## Known limitations

- Stage 2 still depends on `xfel ddr v3s`
- native DRAM initialization is not implemented
- SRAM Stage 1 is not implemented
- autonomous power-on boot from SPI NOR is not implemented
- GT911 occasionally appears to miss touches
- current UI is console-based
- FAT support is read-only and limited to 8.3 root-directory lookup
- custom applications do not yet use a formal image header or CRC
- application return to the boot manager is not supported
- SD/MMC performance paths such as CMD18, DMA and higher-speed modes are not
  implemented

## Related project

Hardware documentation, firmware analysis and earlier reverse-engineering work:

[FNIRSI-DPOS350P-Hack](https://github.com/mkubi/FNIRSI-DPOS350P-Hack)

## License

MIT

