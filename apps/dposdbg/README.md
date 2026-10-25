# DPOSDBG resident plugin, Phase 0

Build with:

```fish
make -C apps/dposdbg
```

Copy `build/apps/dposdbg/DPOSDBG.BIN` to the SD-NAND FAT root as
`DPOSDBG.BIN`.

When Stage 2 boots the original oscilloscope, it optionally loads the plugin at
`0x41700000`, validates the `DBG0` header, and patches EXE2 RAM word
`0x40052F44` from the stock tail branch to the plugin screenshot bridge.

The bridge calls the complete stock screenshot action first, records one
`TRC0` event in resident RAM, and returns to the original UI dispatcher.

Phase 0 does not write `TRACE.BIN` yet. Filesystem export will be implemented by
replacing only `DPOSDBG.BIN`; the EXE2 bridge and Stage 2 loader stay stable.
