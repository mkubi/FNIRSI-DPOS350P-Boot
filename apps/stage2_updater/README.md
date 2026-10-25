# Stage 2 updater U1.1, confirm-gated writer with sector verification

U1 validates `STAGE2.BIN`, backs up the installed 64 KiB Stage 2 slot in RAM,
and performs no write unless `/STG2OK.BIN` exists with the exact 16-byte content:

```text
STG2WRITE-OK-v1\n
```

The writer erases exactly sixteen 4 KiB sectors from `0x001DB000` through
`0x001EA000`, programs the complete 64 KiB slot without crossing 256-byte page
boundaries, and performs a full byte-for-byte read-back. On a powered failure it
attempts to restore the previous slot from RAM. A power loss during erase/program
still requires manual FEL recovery.

U1.1 fixes the U1 read-back overflow. Verification reads one 512-byte NOR
sector at a time into `nor_buffer` and compares it against the corresponding
offset in `new_slot` or `old_slot`. No 64 KiB read targets the 512-byte buffer.
