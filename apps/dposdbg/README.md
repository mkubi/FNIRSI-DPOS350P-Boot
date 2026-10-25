# DPOSDBG resident transaction tracer

DPOSDBG runs beside the stock DPOS350P EXE2 application and exports reconstructed
SMEC transaction records to `/DPOSDBG.TRC` when the stock Screenshot action
completes.

## Current format: version 5 (v4.1 coverage fix)

DPOSDBG v4 is based on the hardware-tested v3 transaction tracer. It preserves
the v3 patch sites, wrappers, screenshot export path and resident placement, and
extends each reconstructed SMEC job with caller context.

Resident range:

```text
0x42E70000 .. 0x42E7FFFF
```

The Stage 2 loader address in `loader/dposdbg.h` and the linker origin in
`apps/dposdbg/link.ld` must remain identical.

Trace header:

```text
magic        TRC0
version      5
capacity     768 records
record_size  64 bytes
```

Each v4 record retains all v3 fields and adds:

```text
begin_call_site  EXE2 BL site that selected command 0x69 and began the job
last_call_site   most recent patched FPGA primitive call site in the job
selector_count   number of selector primitives recorded for the job
primitive_count  selector + write + read primitives recorded for the job
```

The capacity is reduced from 1024 to 768 so the 64-byte records and plugin code
remain inside the reserved 64 KiB resident window.

## Build and deploy

```sh
make -C apps/dposdbg clean
make -C apps/dposdbg
```

Copy `build/apps/dposdbg/DPOSDBG.BIN` to the SD-NAND FAT root as
`DPOSDBG.BIN`.

Stage 2 loads and validates the plugin, installs the known v3 screenshot and
SMEC primitive veneers, and then starts stock EXE2. Trigger a normal Screenshot
to export `/DPOSDBG.TRC`.

## Compatibility

The v4 trace layout is intentionally not binary-compatible with v3 records.
Consumers must verify both `version == 4` and `record_size == 64` before parsing.
The loader ABI remains version 1 because the plugin header and wrapper entry
points are unchanged.


## v4.1 coverage correction

Static audit confirmed that all 25 direct `BL` calls to the FPGA primitives were
already in the v3 patch map. Five tail branches (`B`) were not covered:
`0x40049F78`, `0x40049FB4`, `0x40049FD8`, `0x4004A01C`, and `0x4004A07C`.
They carry the missing 0x6A byte, final 0x6B byte, second 0x69 byte, stop write,
and one tail read.

v4.1 redirects those `B` instructions to the existing write/read veneers while
keeping them as `B`, so the wrappers inherit the original caller LR and retain
tail-call return semantics. No additional stock code-cave space is consumed.
A job is now COMPLETE only when byte counts are exactly 2/1/4/4 for commands
0x69/0x6A/0x6B/0x66. Otherwise `INCOMPLETE_PAYLOAD` is set.
