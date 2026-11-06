# USB Linux resident payload

This standalone payload is linked at `0x42E70000`, a location where a
64-byte sentinel was confirmed to survive the stock EXE2 zero-init range
`0x400954A4..0x42E68D23`.

The first implementation intentionally reproduces the HW-confirmed
`fat_diag` S0.6 sequence:

1. initialize IRQ controller and enable IRQs;
2. initialize Timer 0;
3. initialize SDC0;
4. validate the confirmed managed-NAND geometry;
5. prepare the controller for the stock handoff;
6. publish the stock SD globals;
7. enter the stock EXE2 USB routine at `0x40069018`.

Build:

    make -C apps/usb_linux clean
    make -C apps/usb_linux

Artifacts:

    build/apps/usb_linux/USB_LINUX.elf
    build/apps/usb_linux/USB_LINUX.bin

The payload is read-only with respect to the managed NAND. It does not write
SPI NOR. Stage 2 will later embed the binary and copy it to `0x42E70000`.
