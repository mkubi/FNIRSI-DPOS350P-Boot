# FATDIAG C1: stock branch A + custom timer

C1 differs from the failing C0 by exactly one added component: stock `FUN_40042C94()`.

## Runtime

1. stock `sys_clock_init(1152000000)`
2. stock `sys_mmu_init()`
3. branch at stock main `0x40034710` instead of stock `System_Init()`
4. stock first System_Init branch `0x40042C94`
5. custom `timer_init()` from `arch/r11/timer.c`
6. `MemoryType = MemoryTypeSD` (`0x40095490 = 1`)
7. stock `FatFsInit()` at `0x40049B80`
8. HW-working USB entry at `0x40069018`

The complete stock `System_Init()` at `0x40042D28` is not called. C1 calls only its first branch, then uses the existing custom timer implementation instead of stock branch `0x40042CD8`.

Stock branch `0x40042C94` initializes the stock interrupt-controller runtime, configures timer channel 0 with reload 1000, installs IRQ `0x32` handler `0x40042C64`, enables timer channel 0 and enables CPU IRQs.

## Interpretation

- C1 GOOD: the missing first stock branch explains C0; custom `timer_init()` is sufficient as replacement for the second branch.
- C1 FAIL while M1 remains GOOD: custom `timer_init()` is not behaviorally equivalent to stock branch `0x40042CD8`, or another interaction remains.

## Read-only oracle

```fish
sudo dmesg -W
lsusb | grep 0483:5720
sudo dd if=/dev/sdc bs=512 skip=63 count=1 iflag=direct status=none | hexdump -C | head -20
sudo mount -o ro /dev/sdc1 /mnt/dpos350p
```

Only after correct LBA63 and stable RO mount test RW.
