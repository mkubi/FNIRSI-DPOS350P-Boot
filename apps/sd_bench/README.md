# DPOS350P SD benchmark v0.3

Read-only 24 MHz stability test using the isolated CMD17/PIO/1-bit driver.

- One unmeasured 512 KiB reference pass.
- 100 measured 512 KiB passes.
- Every pass must match the reference CRC32.
- Reports pass/fail counts and minimum, maximum and average transfer time.
- Stops on the first read error or CRC mismatch.
- Stage 2 remains untouched.

```fish
make -C apps/sd_bench clean
make -C apps/sd_bench -j4
cp build/apps/sd_bench/DPOSAPP.BIN ~/Downloads/DPOSAPP.BIN
```
