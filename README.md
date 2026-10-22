# FNIRSI DPOS350P Boot

Custom two-stage bootloader for the FNIRSI DPOS350P oscilloscope based on the Allwinner V3s/R11.

The project has reached a major milestone: the oscilloscope now boots autonomously from its internal SPI NOR through a custom Stage 1 and Stage 2 boot chain. No FEL/XFEL connection or SPI-CS boot strap is required during normal operation.

## Current status

**Autonomous custom boot from SPI NOR is confirmed working.**

Verified boot chain:

```text
Power-on
  -> Allwinner BootROM
  -> custom eGON.BT0 / Stage 1 from SPI NOR @ 0x000000
  -> native clock initialization
  -> native DDR2 initialization
  -> SPI0 / Winbond W25Q16JV
  -> STG2 container from SPI NOR @ 0x1DB000
  -> header and CRC32 validation
  -> Stage 2 payload loaded to DRAM @ 0x41000000
  -> Stage 2 boot menu
  -> touch input
  -> selected loader
  -> original FNIRSI application or other target
```

Confirmed on hardware:

- BootROM accepts the custom `eGON.BT0` image.
- Stage 1 runs natively from SRAM at address `0x00000000`.
- Stage 1 initializes clocks and 64 MiB DDR2 without XFEL DDR setup.
- Stage 1 initializes SPI0 and reads the Winbond W25Q16JV (`JEDEC EF 40 15`).
- Stage 1 loads Stage 2 from the dedicated NOR slot at `0x1DB000`.
- Stage 2 header validation and CRC32 verification work.
- Stage 2 is loaded byte-for-byte correctly to `0x41000000`.
- Stage 1 -> Stage 2 handoff works.
- Display and boot menu work after autonomous power-on.
- GT911 touch input works.
- Stage 2 can chainload the original oscilloscope application successfully.
- Normal boot no longer requires FEL, XFEL, or the SPI-CS recovery wire.

## Hardware

Known relevant hardware:

```text
SoC:       Allwinner V3s / R11, ARM Cortex-A7
DRAM:      64 MiB DDR2, base 0x40000000
SPI NOR:   Winbond W25Q16JV, 2 MiB, JEDEC EF 40 15
Display:   800x480 RGB
Touch:     Goodix GT911
FPGA:      GOWIN
SD-NAND:   1 GiB, FAT16 on the tested unit
```

The SPI NOR remains the primary boot device. The SD-NAND is used by the original firmware/update environment and is also being investigated as a source for custom applications and future boot/update flows.

## Boot architecture

### Stage 1

Stage 1 is deliberately small and recovery-critical. It is intended to remain stable once the boot path is finalized.

Responsibilities:

```text
BootROM entry
  -> clocks
  -> DDR2
  -> SPI0
  -> read STG2 header
  -> validate size/load/entry fields
  -> load Stage 2
  -> validate CRC32
  -> synchronize execution state
  -> jump to Stage 2
```

Two Stage 1 build modes are currently useful during development:

- FEL build, linked for SRAM `0x00008000`, used for safe development/testing.
- BootROM-native BT0 build, linked at `0x00000000`, used for autonomous SPI NOR boot.

The autonomous image uses the compact 32-byte DPOS350P `eGON.BT0` layout recovered from the original SPL:

```text
0x00  ARM branch to 0x20
0x04  "eGON.BT0"
0x0C  Allwinner checksum
0x10  image length
0x14  "SPL\x02"
0x18  reserved
0x1C  reserved
0x20  Stage 1 entry
```

The final image currently preserves the original BT0 size:

```text
0x6800 = 26624 bytes
```

The Allwinner checksum is calculated over the declared image length using `0x5F0A6C39` in the checksum field during calculation.

### Stage 2

Stage 2 is the feature-rich boot manager and is designed to be easy to extend without changing Stage 1.

Current Stage 2 features include:

- display initialization,
- framebuffer console/UI,
- boot menu,
- GT911 touch,
- loading/chaining original FNIRSI firmware components,
- infrastructure for custom applications and storage loaders.

Current Stage 2 load address:

```text
0x41000000
```

Current Stage 2 payload size at the successful autonomous-boot milestone:

```text
15212 bytes (0x3B6C)
```

## STG2 image format

Stage 2 is stored in a dedicated 64 KiB SPI NOR slot:

```text
0x1DB000 - 0x1EAFFF
```

The slot begins with a custom 32-byte `STG2` header:

```text
u32 magic          = "STG2"
u32 version        = 1
u32 header_size    = 32
u32 payload_size
u32 load_address   = 0x41000000
u32 entry_address
u32 payload_crc32
u32 flags          = 0
```

The payload follows immediately after the header. Unused slot space is padded with `0xFF`.

Build the slot image with:

```bash
python tools/mkstg2.py \
    build/dpos_display_spl_clone.bin \
    build/dpos_stage2_slot.bin
```

At the verified milestone the Stage 2 CRC32 was:

```text
0xBAF1E1C1
```

This value changes whenever the Stage 2 binary changes and is generated automatically by `mkstg2.py`.

## SPI NOR map

Current working map of the 2 MiB Winbond:

```text
0x000000 - 0x0067FF  original/custom eGON.BT0 Stage 1 region
0x006800 - 0x017FFF  reserved / currently preserved
0x018000 - 0x03C7FF  original EXE1 / updater
0x03C800 - 0x056FFF  reserved / currently preserved
0x057000 - 0x13E855  original ISO update region
0x13E856 - 0x13FFFF  padding / end of update sector
0x140000 - 0x1DAFFF  unknown existing data, preserve
0x1DB000 - 0x1EAFFF  custom Stage 2 slot, 64 KiB
0x1EB000 - 0x1EEFFF  calibration/configuration data, preserve
0x1EF000 - 0x1EFFFF  empty in current dump
0x1F0000 - 0x1FDFFF  metadata/version/configuration data, preserve
0x1FE000 - 0x1FFFFF  empty in current dump
```

Do not treat unused-looking upper NOR regions as generally free space. Only the dedicated Stage 2 slot is used by this bootloader.

## Original updater findings

Reverse engineering of the original EXE1 updater established several useful properties:

- it contains SD/MMC and FAT filesystem support,
- it searches for a firmware file and exposes messages such as `Found new firmware !`, `Prepare for upgrade`, `Upgrading ...`, and `write end`,
- its NOR writer operates on 4 KiB erase-sector boundaries,
- page programming is split on 256-byte boundaries,
- the update ISO is not a raw 2 MiB NOR image,
- the tested original ISO maps into the NOR firmware area with an offset of `+0x57000` for the confirmed ranges,
- the original update region ends around `0x13E855`, far below the custom Stage 2 slot at `0x1DB000`.

The tested original ISO size was 948310 bytes. Block comparison against the physical NOR dump identified linear mappings including:

```text
ISO 0x001000-0x018FFF -> NOR 0x058000-0x06FFFF
ISO 0x050000-0x0E6FFF -> NOR 0x0A7000-0x13DFFF
```

Both use the same `+0x57000` mapping offset.

## Build Stage 1 for FEL testing

The development/FEL Stage 1 build can be built with the normal Stage 1 target:

```bash
make stage1-clean
make stage1
```

The verified Stage 1B.4 development flow was:

```text
XFEL -> SRAM 0x8000 -> Stage 1 -> STG2 -> Stage 2 -> Original App
```

This remains useful as a safe regression and recovery test path.

## Build autonomous eGON.BT0 Stage 1

Stage 1C uses a separate BootROM-native build:

```bash
make -C stage1 -f Makefile.bt0 clean
make -C stage1 -f Makefile.bt0
```

Generated files include:

```text
build/stage1c/dpos_stage1c_bt0_raw.elf
build/stage1c/dpos_stage1c_bt0_raw.bin
build/stage1c/dpos_stage1c_bt0_raw.lst
build/stage1c/dpos_stage1c_egon_bt0.bin
```

Verify the final image offline:

```bash
python tools/mkbt0.py \
    --verify \
    build/stage1c/dpos_stage1c_egon_bt0.bin
```

Expected structural properties:

```text
magic          eGON.BT0
branch target  0x20
length         0x6800
marker         SPL\x02
checksum       valid
```

At the first successful autonomous-boot milestone the generated checksum was:

```text
0x1B661BEE
```

As with the Stage 2 CRC, this will change when Stage 1 changes.

## Recovery

FEL remains the recovery mechanism.

If the internal BT0 is invalid or a future Stage 1 build fails, the V3s can still be forced into FEL using the known SPI-CS boot strap/debug method. From FEL, the original BT0 backup can be written back to SPI NOR.

Always keep an untouched full 2 MiB Winbond backup and the original `0x6800` BT0 image outside the repository.

Development dumps and reverse-engineering artifacts belong under:

```text
work/
```

The `work/` directory is intentionally ignored by Git.

## Repository layout

```text
arch/r11/       SoC-specific definitions and low-level support
board/dpos350p/ board configuration
drivers/        display, GPIO, touch, SPI NOR and other drivers
storage/        block-device/FAT work
loader/         original/custom image loaders
ui/             boot menu and UI
stage1/         SRAM Stage 1, FEL build and BootROM-native BT0 build
tools/          host-side image/build helpers
work/           local dumps and RE artifacts, ignored by Git
main.c          Stage 2 entry
link.ld         Stage 2 linker script
Makefile        primary build
PLAN.md         project plan and milestone notes
README.md       current project status
```

## Verified milestones

```text
Stage 1A     SRAM execution and safe FEL return
Stage 1A.2   native clock initialization
Stage 1A.3   native DDR2 initialization
Stage 1B.1   native read-only SPI NOR access, JEDEC EF 40 15
Stage 1B.2   complete EXE2 NOR -> DRAM transfer, byte-identical
Stage 1B.3   STG2 slot/header/CRC validation and load to 0x41000000
Stage 1B.4   Stage 1 -> Stage 2 handoff; menu/touch/original app verified
Stage 1C     BootROM-native eGON.BT0; autonomous power-on boot verified
```

## Next milestone

The bootloader itself is now functional. The next major task is deployment through the normal FNIRSI firmware-update workflow.

Target user experience:

```text
copy custom DPOS350P firmware ISO to SD-NAND
  -> reboot
  -> original/customized updater starts
  -> validate package
  -> install/verify Stage 2 first
  -> install/verify Stage 1 eGON.BT0 last
  -> reboot
  -> custom boot menu
```

The final installation path should not require XFEL or manual SPI NOR commands. FEL remains recovery-only.

A safe installer must preserve the original updater, calibration data, device-specific metadata, and all currently unknown upper-NOR regions outside the explicitly allocated Stage 1 and Stage 2 areas.

## Safety notes

- Keep a full, verified 2 MiB SPI NOR backup before experimenting with writes.
- Never erase unknown upper-NOR regions merely because some sectors contain `0xFF`.
- Preserve calibration/configuration areas from `0x1EB000` upward unless their format is explicitly understood.
- Program Stage 2 before Stage 1 during installation so Stage 1 is never installed without a valid second stage.
- Verify every persistent write by reading it back and comparing byte-for-byte.
- Keep FEL recovery available while developing new Stage 1 versions.

## Project goal

The goal is a small and stable first-stage bootloader plus an extensible second-stage boot manager for the DPOS350P:

```text
Stage 1 = minimal, stable, recovery-critical hardware bring-up + verified loader
Stage 2 = extensible UI, storage, boot selection, diagnostics and custom applications
```

This architecture allows Stage 2 to evolve without repeatedly modifying the BootROM-facing Stage 1.
