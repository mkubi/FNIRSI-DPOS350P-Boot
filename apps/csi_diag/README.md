# CSI single-bank smoke test

## Stage 27B SMEC-initialized single-bank capture

Build selector: `CSI_DIAG_STAGE=28`.

Stage 27B is the controlled counterpart to Stage 27A. The capture lifecycle,
private guarded buffer, acquisition mode, polling limits and diagnostics are
unchanged. The only intended experimental change is execution of the seven
already reconstructed `SMEC_Init()` startup jobs before CSI initialization and
the acquisition readiness/position sequence.

The screen reports SMEC completion, completed-job count, error code and the
aggregate 0x65 polling count alongside the same CSI and buffer diagnostics used
by Stage 27A.

Interpretation boundary:

- Stage 27A fail + Stage 27B pass is evidence that the startup job/service
  initialization is required for the tested FPGA sample-stream path.
- If both stages fail, the CSI receiver baseline remains confirmed and another
  stock FPGA/acquisition initialization phase is still missing.

## Stage 27A no-SMEC single-bank capture

Stage 27A takes the HW-confirmed Stage 26 CSI parity baseline and adds exactly
one bank-0 capture attempt. It deliberately does **not** run the 0x64..0x6B
SMEC/job startup sequence.

The experiment asks one narrow question: is the confirmed CSI initialization
plus the reconstructed FPGA acquisition/control sequence sufficient for one
FPGA-to-CSI-to-RAM transfer without SMEC initialization?

The sequence is bounded and uses a private guarded destination buffer:

```text
FPGA baseline ID
stock-parity CSI init
FPGA acquisition ready/position probe
FPGA stream quantity + position + bank 0
sentinel/guard preparation
PE -> CSI mode
CSI BUF0 -> private destination
ACK stale completion
Timer0 gate off
CSI capture enable
PE1 high
bounded CSI DONE poll
PE1 low
Timer0 gate on
ACK + capture disable
restore FPGA GPIO control mode
cache invalidate + buffer analysis
```

Success requires CSI DONE, intact guards, and at least one changed destination
byte. Stage 27A always reports `SMEC USED: NO`.

## Stage 26 stock CSI initialization parity

Stage 26 is a no-capture Custom App test derived from the HW-confirmed
DPOSLAB decomposition of stock `FPGA_Start()`.

It reproduces the confirmed initialization order without invoking stock EXE2:

```text
CSI clock/reset bring-up
CSI interface/geometry configuration for quantity 8000
PE CSI function-2 pinmux
bank-0 destination programming
CSI completion IRQ enable
restore FPGA GPIO control mode
```

The test reports the intermediate clock/reset state, final CSI register state,
CSI pinmux state, destination encoding and restored GPIO state independently.
It performs no SMEC jobs, no FPGA acquisition commands, no CSI capture enable
and no DMA transfer. All clock readiness waiting is timeout protected.

Pinmux parity is checked only over fields owned by the corresponding stock
helpers. `0x40065EF8` owns PE0, PE2 and PE3 in `PE_CFG0`, plus all of
`PE_CFG1`; PE1 and PE4..PE7 retain their pre-existing state. GPIO restore is
validated functionally as PE0..PE3 in GPIO mode, PE8..PE15 as inputs, PE0
high and PE1 low. Absolute full-word `PE_CFG0` equality is intentionally not
required because stock EXE2 and the standalone Custom App enter the helper
with different unrelated pinmux state.

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
