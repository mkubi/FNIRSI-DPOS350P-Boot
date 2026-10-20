# FNIRSI-DPOS350P-Boot

Custom boot manager for the FNIRSI DPOS350P oscilloscope (Allwinner R11/V3s).

> **Work in progress — Phase 0 skeleton**

## Goal

Replace the original boot flow with a small boot manager that:

1. Initializes clocks and 64 MiB DDR2
2. Initializes the 1024×600 IPS display and GT911 touch controller
3. Shows a boot menu (touchscreen)
4. **Original firmware** — loads FNIRSI app from SPI NOR and jumps to it
5. **Custom firmware** — loads a custom binary from SD-NAND and jumps to it
6. Preserves hardware FEL recovery (hold SPI0 CS/PC2 low during reset)

## Hardware

| Component | Detail |
|-----------|--------|
| SoC | Allwinner R11 / V3s (ARM Cortex-A7) |
| DRAM | 64 MiB DDR2 SIP |
| SPI NOR | Winbond W25Q16JV (2 MiB) — original firmware |
| SD-NAND | CSNP1GCR01-B0W (1 GiB) — custom firmware |
| Display | 1024×600 IPS RGB565, 50-pin FFC |
| Touch | GT911 capacitive (SDA=PG6, SCL=PG7, INT=PE22, RST=PE24) |

See [FNIRSI-DPOS350P-Hack](https://github.com/mkubi/FNIRSI-DPOS350P-Hack) for
full hardware documentation, reverse engineering notes and the ISO format spec.

## Source tree

```
arch/r11/       R11/V3s startup, clocks, DRAM, cache
board/dpos350p/ Board-specific memory map and pin definitions
drivers/        GPIO, display (DE2/TCON0), GT911, SPI NOR, SDC2
storage/        Block device abstraction, FAT reader
loader/         Image validation and jump stubs
ui/             Boot menu
main.c          Entry point
```

## Development phases

| Phase | Status | Description |
|-------|--------|-------------|
| 0 | 🔄 active | FEL workflow — display + touch proven |
| 1 | ⏳ next | SPI NOR driver — read original app |
| 2 | ⏳ | SDC2 raw block driver |
| 3 | ⏳ | FAT + custom image loader |
| 4 | ⏳ | Touchscreen boot menu |
| 5 | ⏳ | Native BT0/SPL (replaces xfel ddr) |

## Building (Phase 0, via FEL)

```bash
make
xfel ddr v3s
xfel write 0x41000000 dpos-boot.bin
xfel exec  0x41000000
```

Requires `arm-none-eabi-gcc`.

## FEL recovery

Short SPI0 CS (PC2 on the 6-pin debug header) to GND at power-on to enter FEL mode.
This hardware path is preserved at all times as a recovery mechanism.

## License

MIT
