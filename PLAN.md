# DPOS350P custom boot manager: awboot reference plan

## Goal

Build a small R11/V3s boot manager that can:

1. initialize clocks and the 64 MiB SIP DDR2;
2. initialize the DE2/TCON0 display and GT911 touch controller;
3. show a boot menu;
4. load and execute the original FNIRSI application from SPI NOR;
5. load and execute a custom application from the CSNP1GCR01-B0W SD-NAND;
6. preserve a hardware FEL recovery path by holding SPI0 CS low during reset.

## Confirmed DPOS350P facts

- SoC: Allwinner R11/V3s family, ARM Cortex-A7.
- Boot ROM accepts the existing eGON BT0/SPL image from SPI NOR offset 0.
- Original application is stored at SPI NOR offset 0x000A8000.
- Original application is loaded to RAM address 0x40000000.
- xfel can initialize DDR using `xfel ddr v3s`.
- Display: 1024x600 RGB565 through DE2 mixer 0, channel 2.
- Framebuffers proven working at 0x43000000 and 0x43200000.
- GT911 touch is proven working through bit-banged I2C:
  - SDA = PG6
  - SCL = PG7
  - INT = PE22
  - RESET = PE24
  - address = 0x5d
  - status = 0x814e
  - first point = 0x8150
- SD-NAND: CSNP1GCR01-B0W, connected to SDC2.

## What to reuse from awboot

Use awboot as a design and code reference, not as a drop-in R11 port.

Candidate reusable concepts:

- separation of architecture, board and generic drivers;
- `board_init()` and board device descriptions;
- SDRAM address-range validation before loading an image;
- storage-independent image metadata;
- SD/MMC driver structure;
- FAT/filesystem loader structure;
- FEL build/workflow concepts;
- eGON image generation.

Do not initially reuse:

- T113/V851 register addresses;
- T113/V851 clock setup;
- T113/V851 DRAM training values;
- T113/V851 GPIO mux values;
- Linux/PSCI boot path.

Those are SoC-specific and must be replaced with R11/V3s values.

## Proposed source tree

```text
dpos-boot/
  arch/r11/
    start.S
    clock.c
    clock.h
    dram.c
    dram.h
    cache.c
    cache.h
    regs.h

  board/dpos350p/
    board.c
    board.h
    memory_map.h

  drivers/
    gpio.c
    gpio.h
    display.c
    display.h
    gt911.c
    gt911.h
    spinor.c
    spinor.h
    sdc2.c
    sdc2.h

  storage/
    blockdev.c
    blockdev.h
    fat.c
    fat.h

  loader/
    image.c
    image.h
    original.c
    custom.c
    jump.S

  ui/
    boot_menu.c
    boot_menu.h

  main.c
  link.ld
  Makefile
```

## Memory map for the first boot manager

```text
0x40000000  boot manager load/entry address during FEL development
0x40020000  temporary stack top for small test payloads
0x41000000  candidate load address for a custom application
0x42000000  candidate staging/read buffer
0x43000000  display framebuffer 0
0x43200000  display framebuffer 1
```

Before this becomes a flashed SPL, the complete linker layout must be adjusted to execute initially from SRAM and only move to DRAM after DDR initialization. The current 0x40000000 payload assumes DDR has already been initialized by xfel or the original SPL.

## Development phases

### Phase 0: keep current FEL workflow

- Enter FEL by holding SPI0 CS/PC2 low during power-on.
- Run `xfel ddr v3s`.
- Upload the boot-manager test payload to 0x40000000.
- Execute it with xfel.

This phase is for display, touch, SPI NOR and SDC2 development. It does not yet replace BT0.

### Phase 1: SPI NOR original-app loader

Implement a read-only SPI NOR driver and prove this sequence:

```text
SPI NOR 0x000A8000 -> RAM staging/load area -> validate eGON.EXE -> jump
```

Important design point: the boot manager itself currently occupies 0x40000000, which is also the original application's load address. The loader must therefore either:

- execute from SRAM while copying the application to 0x40000000; or
- execute from another DRAM address and use a relocation-safe handoff stub.

The first FEL version should link the boot manager above the original app destination, for example at 0x41000000, after validating that it does not overlap framebuffers or the custom image.

### Phase 2: SDC2 raw block driver

Implement only enough SDC2/MMC protocol to:

- reset the controller;
- configure PC0-PC3 pin mux as required by SDC2;
- identify the CSNP1GCR01-B0W;
- read sector 0;
- read an arbitrary 512-byte sector;
- read a contiguous range.

Do not introduce FAT until raw sector reads are reliable.

### Phase 3: FAT/custom image loader

After block reads work:

- mount the existing FAT volume read-only;
- load `/boot/dpos.bin` or another fixed path;
- validate a small custom image header;
- clean caches;
- disable display/touch if the custom image expects full hardware ownership;
- jump to the custom entry point.

### Phase 4: touchscreen boot menu

Use the proven display and GT911 modules. Provide:

- Original firmware
- Custom firmware
- Diagnostics

Keep hardware FEL entry as the recovery mechanism until a software transition back to BROM FEL is proven on R11.

### Phase 5: native R11 BT0/SPL

Port the R11-specific startup path:

1. SRAM startup and stack;
2. clocks;
3. original DDR2 timing and training sequence;
4. optional MMU/cache setup;
5. load the larger boot manager into DRAM;
6. jump to it.

The primary source of truth for DDR must be the original DPOS350P BT0/SPL and the working V3s implementation used by xfel/U-Boot. awboot's DRAM code is useful structurally, but its T113/V851 register values are not assumed compatible.

## Immediate reversing targets

### Original SPL

Extract and document:

- CPU PLL and bus clock initialization;
- DRAM controller/PHY register sequence;
- memory size detection;
- whether the SPL enables MMU/cache before loading the app;
- SPI NOR read implementation;
- exact handoff behavior into the main application.

### Original main app

Extract and document:

- SDC2 base and reset/clock gates;
- PC pin mux for SD-NAND;
- card initialization sequence;
- sector-read function;
- FAT mounting/file read calls;
- original app entry assumptions.

## First implementation milestone

The first new executable should still rely on `xfel ddr v3s` and should be linked away from 0x40000000. It should:

1. initialize display/touch;
2. read the first 64 bytes of the original image from SPI NOR offset 0xA8000;
3. show pass/fail on the display;
4. read sector 0 from SD-NAND via SDC2;
5. show pass/fail on the display;
6. return to FEL where possible, or remain in a diagnostic loop.

This isolates storage work before attempting any destructive flash replacement.
