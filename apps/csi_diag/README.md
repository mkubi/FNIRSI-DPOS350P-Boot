# CSI single-bank smoke test

This Custom App first reproduces the seven HW-confirmed SMEC_Init startup jobs, then performs one timeout-protected bank-0 FPGA-to-CSI-to-RAM capture.

Default build stage: `CSI_DIAG_STAGE=25`. Stage 1 and Stage 2 are unchanged.

The test uses the locally HW-confirmed stock CSI register baseline:

```text
CSI IF       0x01060400
CSI HSIZE    0x00000A00
CSI GEO0     0x18000000
CSI GEO1     0x00010000
CSI GEO2     0x2374287C
CSI IRQ      0x00000002
CSI BUF0     destination >> 2
```

The 8500-byte destination is surrounded by 4 KiB guards, filled with `0xAA`, cleaned before DMA, invalidated after capture, and scanned for modifications. All completion waits are bounded.

Success requires FPGA ID, acquisition readiness/position, CSI config, CSI completion, intact guards, and a nonzero modified-byte count. The stock reference changed the first `0x1800` (6144) bytes of a `0x2134` slot.

## Stage 22 Job 2 investigation

Stage 22 performs only FPGA ID plus one Job 1 and one Job 2 transaction. It uses timing 2000, no retries, bounded 0x64/0x65 polls, a fixed bounded inter-job gap, and no CSI access. The screen reports first/last/OR/change counts for both polling phases.

## Stage 23 Job 2 re-arm A/B debug

Stage 23 verifies the statically confirmed stock transaction order, captures idle 0x64/0x65 values after Job 1 and after the normal Job 2 failure, then retries the same Job 2 once after the diagnostic re-arm write `0x67 <- 0x08`. No CSI registers are touched.




## Stage 24 stock PE idle cleanup

Stage 24 locally reproduces the hardware effect of stock `0x40049110` and
`0x400490DC`: PE0..PE3 function 1, PE0..PE3 pulls disabled, PE8..PE15 input,
PE0 high and PE1 low. It then performs one Job 1 and one Job 2 attempt with
timing 2000. No stock EXE2 address is called; no retry, re-arm, CSI or DMA is
used. All polling remains bounded.


## Stage 25 exact stock ID RAW4 prerequisite

Stage 25 executes the exact stock ID transaction reconstructed at
`0x400492E4`: selector `0x01` followed by exactly four byte reads. It adds no
settle delay and performs no fifth diagnostic read. The corrected Stage 24 PE
idle cleanup then runs unchanged, followed by one Job 1 and one Job 2 attempt
with timing 2000. No retry, re-arm, CSI, DMA or stock EXE2 call is used.
