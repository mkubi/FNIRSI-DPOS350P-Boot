# SD-NAND read-only mapper Custom App

This application is loaded as `DPOSAPP.BIN` by the existing Stage 2 Custom App
loader. It initializes the existing SD/MMC driver, reports CSD-derived capacity,
and reads selected LBAs around and beyond the approximately 121 MiB FAT area.

The application contains no SD write API and issues only initialization commands,
CMD9 (CSD), and CMD17 (single-block read).

## Apply driver patch

From the repository root:

```bash
patch -p1 < patches/sdmmc0-csd-capacity.patch
```

## Build

```bash
make -C apps/sd_nand_map clean
make -C apps/sd_nand_map
```

Output:

```text
build/apps/sd_nand_map/DPOSAPP.BIN
```

The Makefile verifies that the first word is `0xE59FD08C`, matching the current
Custom App loader signature check.

Copy `DPOSAPP.BIN` to the root of the user-visible FAT volume, safely unmount,
then select Custom App in the Stage 2 menu.

Each probe is classified as `ZERO`, `FF`, `DATA`, or `ERR`, with an FNV-1a
fingerprint. The first version performs only selected probes and does not scan or
dump the full device.
