# FNIRSI-DPOS350P-Boot Development Plan

Development roadmap for the DPOS350P bare-metal boot manager. This file describes the current state; historical bring-up details belong in Git history or dedicated notes.

## Current status

Confirmed on hardware:

- autonomous BootROM -> custom Stage 1 -> verified Stage 2 boot from W25Q16JV SPI NOR
- native clock and 64 MiB DDR2 initialization
- SPI NOR JEDEC `EF 40 15` and read-only access
- Stage 2 slot/header/CRC validation and handoff to `0x41000000`
- 1024x600 RGB565 display, framebuffer console and hardware timer
- GT911 touch boot menu
- original EXE1 and EXE2 chainload at `0x40000000`
- internal SD-NAND through SDC0/PF0..PF5
- FAT16/FAT32-compatible read-only loader
- `DPOSAPP.BIN` Custom App loading and cache-safe execution

The normal boot path no longer requires FEL/XFEL. FEL remains the recovery and development path.

## Known-good baselines

#### Custom acquisition

- hardware-confirmed zero-stock prerequisite/acquisition chain after the
  DPOSLAB checkpoint handoff
- production acquisition API: `dpos_acq_prepare()` and `dpos_acq_capture()`
- FPGA transport split into `fpga_bus`, `fpga_protocol` and `fpga_smec`
- SoC CSI access remains isolated in `r11_csi`
- confirmed READY fingerprint: FPGA `0x0A` changes `0xC0 -> 0xC1`
- confirmed CSI capture: DONE and `0x1800` changed bytes at offsets
  `0x0000..0x17FF`
- DPOSLAB remains the hardware regression harness; detailed RE history is
  maintained separately

### SPI NOR

- W25Q16JV, 2 MiB, JEDEC `EF 40 15`
- `-mno-unaligned-access` is mandatory in known-good builds
- local `uint8_t tx[4]` / `rx[4]` buffers are safe with that compiler flag
- keep the read-only SPI path free of diagnostic callbacks

A confirmed GCC `-O2` alignment issue generated an unaligned ARM `LDRH` from a byte buffer and froze JEDEC reads. Do not remove the compiler flag without inspecting generated code and repeating hardware tests.

### SD-NAND

- controller: SDC0 at `0x01C0F000`
- pins: PF0..PF5
- tested partition: type `0x06`, starts at LBA 63, FAT16
- current stable transfer path: CMD17, single-sector, PIO FIFO

Do not regress the path while experimenting with higher clocks, 4-bit mode, CMD18 or DMA.

### Application handoff

- original EXE1 and EXE2: skip 32-byte eGON header, load/enter at `0x40000000`
- Custom App: raw `DPOSAPP.BIN`, load/enter at `0x40000000`
- clean D-cache, invalidate I-cache and branch predictor, execute DSB/ISB, disable IRQ/FIQ and transfer with BLX

## Completed phases

1. FEL development workflow, display and console
2. SPI NOR read path and original EXE chainload
3. SDC0/SD-NAND sector reads
4. FAT and Custom App loader
5. common image/handoff layer
6. hardware timer and touch boot menu
7. SRAM Stage 1 with native clocks and DDR2
8. verified STG2 container in dedicated NOR slot
9. BootROM-native eGON.BT0 and autonomous power-on boot
10. hardware-confirmed zero-stock custom acquisition baseline

## Current work

### 1. Storage and Custom App cleanup

- finish capacity/bounds reporting for SD-NAND diagnostics
- keep FAT and block-device boundaries clean
- define a versioned Custom App image while retaining raw-binary development support
- add stricter load-range and overlap validation
- define behavior for unexpected application return

### 2. Normal-update deployment

Target flow:

```text
copy custom update container to SD-NAND
  -> reboot into updater/installer
  -> validate package and target layout
  -> preserve original updater, calibration and metadata
  -> program and read-back verify Stage 2 first
  -> program and read-back verify Stage 1 last
  -> reboot into custom boot menu
```

Requirements:

- keep general SPI NOR write/erase code out of normal Stage 2
- isolate writes in a dedicated installer application
- enforce explicit address ranges and image sizes
- verify every erase/program operation by read-back
- never install Stage 1 without a valid Stage 2 already present
- retain an untouched 2 MiB backup and tested FEL recovery procedure

### 3. GT911 reliability

Touch selection works, but missed touches remain a follow-up item. Preserve the known-good bit-banged I2C path and investigate reset/address timing, pulls, polling rate and optional interrupt-driven operation without blocking the installer milestone.

## Future improvements

- higher SDC0 clock, 4-bit mode, CMD18 and DMA
- generic block-device abstraction and optional filesystem cache
- graphical boot buttons and diagnostics page
- complete D-cache clean-by-set/way helper
- controlled reset after unexpected application return
- build-time checks for unresolved freestanding runtime helpers

## Safety rules

- preserve the dedicated Stage 2 slot at `0x1DB000..0x1EAFFF`
- preserve calibration/configuration from `0x1EB000` upward unless explicitly understood
- do not classify all-`0xFF` sectors as generally free
- do not modify the stable Stage 1/SPI baseline casually
- perform persistent-write development only with FEL recovery available
