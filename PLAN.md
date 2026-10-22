# FNIRSI-DPOS350P-Boot Development Plan

Development roadmap for the FNIRSI DPOS350P custom bare-metal boot manager.

## Current milestone

The FEL-based Stage 2 boot manager is functional on real hardware.

Current known-good functionality:

```text
Display / framebuffer console          DONE
SPI NOR read path                      DONE
Original EXE1 / USB Connect loader     DONE
Original EXE2 / oscilloscope loader    DONE
SDC0 / internal SD-NAND                DONE
FAT16/FAT32 read-only support          DONE
DPOSAPP.BIN loader                     DONE
Cache-safe BLX handoff                 DONE
Hardware timer                         DONE
Boot target abstraction                DONE
Touch boot menu                        FUNCTIONAL
GT911 reliability cleanup              TODO
Native DRAM initialization             TODO
SRAM Stage 1                           TODO
Autonomous SPI NOR boot                TODO
```

The current Stage 2 is linked at `0x41000000` and is started after:

```bash
xfel ddr v3s
```

Applications are loaded and executed at `0x40000000`.

---

## Phase 0 - FEL, display and console

Status: **complete**

- [x] Establish reliable FEL entry and development workflow
- [x] Initialize DRAM externally through XFEL
- [x] Run custom code from `0x41000000`
- [x] Initialize DE2 / TCON0 display path
- [x] Configure 1024x600 RGB565 framebuffer
- [x] Confirm double buffering
- [x] Implement framebuffer cache cleaning
- [x] Implement 8x8 bitmap font
- [x] Implement framebuffer debug console
- [x] Use the console for hardware bring-up diagnostics

Known Stage 2 memory locations:

```text
Stage 2:          0x41000000
Current stack:    0x41020000
Application load: 0x40000000
```

---

## Phase 1 - SPI NOR and original application chainload

Status: **complete**

- [x] Initialize SPI0
- [x] Configure PC0..PC3
- [x] Use SPI mode 0 with manual CS
- [x] Read Winbond JEDEC ID
- [x] Confirm JEDEC `EF 40 15`
- [x] Implement read-only `0x03` NOR reads
- [x] Implement chunked reads
- [x] Reverse-engineer original eGON layout
- [x] Validate `eGON.EXE` header
- [x] Strip the 32-byte eGON header before execution
- [x] Load EXE1 payload to `0x40000000`
- [x] Successfully start EXE1 / USB Connect
- [x] Load EXE2 payload to `0x40000000`
- [x] Successfully start EXE2 / oscilloscope application

Confirmed SPI NOR layout:

```text
0x000000  eGON.BT0   size 0x006800
0x018000  eGON.EXE   size 0x024800
0x058000  eGON.BMP   size 0x018A00
0x0A8000  eGON.EXE   size 0x095800
```

Confirmed runtime payloads:

```text
EXE1:
    header:  0x018000
    payload: 0x018020
    size:    0x247E0

EXE2:
    header:  0x0A8000
    payload: 0x0A8020
    size:    0x957E0
```

Both execute from:

```text
0x40000000
```

### Open EXE1 questions

- [ ] Determine the exact firmware-update behavior of EXE1
- [ ] Determine under which conditions EXE1 writes SPI NOR
- [ ] Compare duplicated USB / SD functionality between EXE1 and EXE2
- [ ] Explain the Return button visible only when USB Connect is entered from EXE2

Until these questions are answered, present EXE1 simply as **USB Connect**.

---

## Phase 2 - SDC0 and internal SD-NAND

Status: **complete**

- [x] Identify SDC0 as the active controller
- [x] Confirm PF0..PF5 routing
- [x] Correct GPIO Port F offset to `0xB4`
- [x] Correct GPIO function fields to four-bit masks
- [x] Configure SDC0 clock/reset sequence
- [x] Implement card initialization
- [x] Implement CMD17 sector reads
- [x] Implement PIO FIFO reads
- [x] Read complete 512-byte sectors
- [x] Confirm repeated reads
- [x] Read and decode the MBR

Known-good values include:

```text
PF_CFG0:      0x07222222
PF_PUL0:      0x00000555
CMD8:         0x00000155
OCR:          0x80FF8000
RINT:         0x0000000C
FIFO words:   0x00000080
```

Keep the current path as the known-good baseline:

```text
CMD17
PIO FIFO
single-sector reads
```

Future optimization only after autonomous boot is stable:

- [ ] Raise SDC0 clock
- [ ] Test 4-bit bus mode
- [ ] Implement CMD18 multi-block reads
- [ ] Consider DMA

---

## Phase 3 - FAT and custom application loading

Status: **complete**

- [x] Parse MBR
- [x] Confirm partition type `0x06`
- [x] Confirm partition start at LBA 63
- [x] Implement FAT16 mount
- [x] Add FAT32-compatible read-only layout handling
- [x] Automatically detect FAT16/FAT32
- [x] Parse root-directory entries
- [x] Match FAT 8.3 filenames
- [x] Skip LFN/deleted/volume entries
- [x] Follow FAT cluster chains
- [x] Read arbitrary file sectors
- [x] Locate `DPOSAPP.BIN`
- [x] Load the complete application to DRAM
- [x] Verify the first ARM word
- [x] Successfully execute the custom application

Current custom application:

```text
Filename:       DPOSAPP.BIN
Load address:   0x40000000
Entry address:  0x40000000
```

Future custom image format:

- [ ] Define image magic
- [ ] Add format/version field
- [ ] Add load address
- [ ] Add entry address
- [ ] Add payload size
- [ ] Add CRC32
- [ ] Add compatibility flags

Keep raw binary support available for development.

---

## Phase 4 - Common loader and handoff

Status: **complete**

- [x] Create common `loader_image_t`
- [x] Separate probe/load/start phases
- [x] Implement custom application loader
- [x] Implement original-image loader
- [x] Implement common application start path
- [x] Clean D-cache for the loaded image
- [x] Invalidate I-cache
- [x] Invalidate branch predictor
- [x] Execute DSB / ISB
- [x] Disable IRQ/FIQ
- [x] Transfer control using BLX
- [x] Confirm DPOSAPP handoff
- [x] Confirm EXE1 handoff
- [x] Confirm EXE2 handoff

Follow-up:

- [ ] Implement a correct full D-cache clean-by-set/way helper
- [ ] Decide whether applications may ever return
- [ ] Add a controlled reset path after unexpected return
- [ ] Add stricter image memory-range validation

---

## Phase 5 - Hardware timer

Status: **complete**

Timer 0 is configured from OSC24M as a free-running 32-bit down-counter.

```text
Base:       0x01C20C00
Control:    0x01C20C10
Interval:   0x01C20C14
Current:    0x01C20C18
Clock:      24 MHz
```

- [x] Implement `timer_init()`
- [x] Implement `timer_ticks()`
- [x] Implement `timer_get_ms()`
- [x] Implement `timer_delay_ms()`
- [x] Implement `timer_delay_us()`
- [x] Implement short wrap-safe deadlines
- [x] Replace inaccurate autoboot busy loop
- [x] Use timer for touch polling intervals

---

## Phase 6 - Boot target model and boot menu

Status: **functional**

Current targets:

```text
CUSTOM APPLICATION
ORIGINAL OSCILLOSCOPE
USB CONNECT
```

- [x] Probe all boot targets before drawing the menu
- [x] Track availability and probe errors
- [x] Select a default boot target
- [x] Hardware-timed autoboot
- [x] Loading screen
- [x] Error screen
- [x] Touch target selection
- [x] Immediate boot after valid target touch
- [x] Cancel autoboot after a real touch outside targets
- [x] Ignore release events for autoboot cancellation
- [x] Preserve autoboot after touch initialization failure
- [x] Preserve autoboot after runtime touch communication failure

Current default priority:

```text
1. Custom Application
2. Original Oscilloscope
3. USB Connect
```

Current console UI touch regions:

```text
X: 20..1003

Custom Application:      Y 48..129
Original Oscilloscope:   Y 130..219
USB Connect:             Y 220..279
```

Future UI polish:

- [ ] Replace console rows with explicit graphical buttons
- [ ] Add button outlines/backgrounds
- [ ] Add touch-down feedback
- [ ] Add progress indicator
- [ ] Add diagnostics target/page
- [ ] Add configurable default target
- [ ] Add configurable autoboot timeout

UI work is not required before Stage 1 development.

---

## GT911 touch follow-up

Status: **functional, reliability investigation pending**

Confirmed wiring:

```text
PE22  INT
PE24  RESET
PG6   SDA
PG7   SCL
```

Current interface:

```text
GPIO bit-banged I2C
7-bit address: 0x5D
write:         0xBA
read:          0xBB
```

Registers used:

```text
0x8140  Product ID
0x814E  coordinate status
0x8150  first touch point
```

Confirmed behavior:

- [x] Product ID reads successfully
- [x] DATA_READY events work
- [x] X/Y coordinates work
- [x] Menu selection works
- [x] Release events are handled
- [x] Release events do not overwrite last coordinates with `0/0`
- [x] Release events do not cancel autoboot
- [x] Polling is limited to approximately 10 ms

Observed issue:

```text
Some physical touches are occasionally missed.
```

Investigation tasks:

- [ ] Determine the most reliable GT911 reset/address-selection timing
- [ ] Preserve the known-good rough legacy timing as a reference
- [ ] Compare the working timing with GT911 documentation
- [ ] Verify PE22 is a truly floating input during normal operation
- [ ] Verify PE22 pull-up/pull-down configuration
- [ ] Verify PG6/PG7 pull configuration
- [ ] Measure bit-banged I2C frequency
- [ ] Test 5 ms / 10 ms / 20 ms polling intervals
- [ ] Consider INT-driven polling instead of continuous status polling
- [ ] Add press/hold/release state tracking
- [ ] Add missed-event and I2C-error counters
- [ ] Verify all four display corners
- [ ] Confirm whether coordinate swap/inversion is required

Do not consider the touch driver final until target selection is reliable over
many consecutive boots.

---

## Compiler / freestanding constraints

Known-good build requires:

```text
-mno-unaligned-access
```

A confirmed GCC `-O2` issue generated an unaligned ARM `LDRH` from a local byte
buffer and caused SPI NOR JEDEC reads to freeze.

The project is freestanding and does not link libc.

Watch for compiler-generated runtime helpers, especially:

```text
memcpy
memset
division helpers
```

Small structures should currently be copied field by field rather than with a
structure assignment that may be lowered to `memcpy()`.

Useful build check:

```bash
arm-none-eabi-nm build/dpos_display_spl_clone.elf | grep ' U '
```

Future tooling:

- [ ] Fail builds with unexpected unresolved runtime helpers
- [ ] Add a small freestanding `memcpy()` if genuinely useful
- [ ] Add a small freestanding `memset()` if genuinely useful
- [ ] Add automated image-size checks

---

## Next major phase - SRAM Stage 1

Status: **not started**

This is the next major low-level milestone.

Goal:

```text
BootROM
   |
   +-- SRAM Stage 1
         |
         +-- basic CPU setup
         +-- clock initialization
         +-- DDR2 initialization
         +-- DRAM test
         +-- SPI NOR initialization
         +-- load Stage 2 to 0x41000000
         +-- verify Stage 2
         +-- jump to Stage 2
```

Stage 1 requirements:

- [ ] Run entirely from internal SRAM
- [ ] Use an SRAM-resident stack
- [ ] Create a dedicated SRAM linker script
- [ ] Clear SRAM BSS
- [ ] Avoid framebuffer usage before DRAM init
- [ ] Avoid SD-NAND/FAT in Stage 1
- [ ] Keep buffers small
- [ ] Keep code within BootROM/SRAM limits

### Native clock / DRAM initialization

Primary reference:

```text
XFEL V3s/S3 DDR payload
```

The XFEL implementation is especially valuable because:

```bash
xfel ddr v3s
```

already initializes DRAM successfully on this exact DPOS350P hardware.

Secondary references:

```text
mainline U-Boot V3s DDR2 initialization
original FNIRSI eGON.BT0 / SPL
```

Recommended bring-up order:

1. Port the XFEL clock/DRAM code into an SRAM-only test payload.
2. Execute the payload through FEL without running `xfel ddr v3s` first.
3. Write/read test patterns in DRAM.
4. Confirm Stage 2 load region at `0x41000000`.
5. Add minimal SPI NOR reading.
6. Load the existing Stage 2 from SPI NOR.
7. Jump to Stage 2.
8. Only then consider persistent installation.

### DRAM test patterns

At minimum:

```text
0x00000000
0xFFFFFFFF
0xAAAAAAAA
0x55555555
address pattern
inverted address pattern
```

Test multiple regions while avoiding the active SRAM Stage 1 code/stack.

---

## Autonomous SPI NOR boot

Status: **not started**

After Stage 1 works reliably through FEL:

- [ ] Determine BootROM SPI NOR loading behavior
- [ ] Determine the required boot header
- [ ] Determine checksum requirements
- [ ] Determine maximum initial SRAM image size
- [ ] Define persistent Stage 1 location
- [ ] Define persistent Stage 2 location
- [ ] Preserve EXE1 and EXE2 regions
- [ ] Add Stage 2 image magic/size/CRC
- [ ] Add safe fallback behavior
- [ ] Verify complete recovery procedure
- [ ] Install Stage 1 only after all previous checks pass

Do not add general SPI NOR erase/write support to normal Stage 2 operation until
the persistent layout and recovery process are documented and tested.

---

## Recovery strategy

Already confirmed:

- [x] FEL access
- [x] SPI NOR read access
- [x] Original EXE1 remains bootable
- [x] Original EXE2 remains bootable

Still required before persistent installation:

- [ ] Document exact FEL recovery wiring/procedure
- [ ] Keep a verified full SPI NOR backup
- [ ] Add host-side image verification
- [ ] Verify Stage 1 entirely from SRAM first
- [ ] Verify native DRAM initialization
- [ ] Verify Stage 1 -> Stage 2 loading
- [ ] Document rollback procedure

---

## Future storage improvements

Only after autonomous boot is stable:

- [ ] Generic block-device abstraction
- [ ] Separate FAT from direct SDC0 dependency
- [ ] Higher SDC0 clock
- [ ] 4-bit bus mode
- [ ] CMD18 multi-block reads
- [ ] DMA
- [ ] Optional filesystem cache
- [ ] Long filename support if ever required
- [ ] Subdirectory support if ever required

The current CMD17 PIO implementation remains the known-good baseline.

---

## Future loader improvements

- [ ] Define final custom image header
- [ ] Magic and image version
- [ ] Explicit load address
- [ ] Explicit entry address
- [ ] Payload size validation
- [ ] CRC32
- [ ] Boot-manager compatibility version
- [ ] Validate all image ranges against the memory map
- [ ] Prevent overlap with Stage 2 and framebuffer memory
- [ ] Define a boot-information structure for custom applications
- [ ] Optionally pass framebuffer/display information to custom applications
- [ ] Decide how unexpected application return is handled

---

## Milestones

```text
A  Display + console                     COMPLETE
B  SPI NOR read path                     COMPLETE
C  Original EXE chainload                COMPLETE
D  SD-NAND sector reads                  COMPLETE
E  FAT16/FAT32                           COMPLETE
F  DPOSAPP.BIN loader                    COMPLETE
G  Common cache-safe handoff             COMPLETE
H  Hardware timer                        COMPLETE
I  Touch boot menu                       FUNCTIONAL
J  GT911 reliability cleanup             PENDING
K  Native DRAM initialization            NOT STARTED
L  SRAM Stage 1                          NOT STARTED
M  Stage 1 -> Stage 2 SPI NOR boot       NOT STARTED
N  Autonomous power-on boot              NOT STARTED
O  Recovery-safe persistent install      NOT STARTED
```

---

## Known-good baseline

Preserve the following behavior while continuing development:

```text
-mno-unaligned-access
Winbond JEDEC EF 40 15
read-only SPI NOR
GPIO Port F offset 0xB4
4-bit GPIO function fields
SDC0 CMD17 PIO reads
FAT16/FAT32 read-only support
DPOSAPP.BIN loading
EXE1 payload skip +0x20
EXE2 payload skip +0x20
load/entry address 0x40000000
D-cache clean + I-cache invalidate
BLX handoff
hardware Timer 0
GT911 touch menu
safe autoboot fallback
```

Current known-good Stage 2 tag:

```text
touch-boot-menu-ok
```

Touch reliability remains an explicit follow-up item and should not be silently
considered solved by this tag.

