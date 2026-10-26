#!/usr/bin/env python3
import argparse, struct
from pathlib import Path
MAGIC=0x4B484343; VERSION=1
NAMES=["FPGA_Start","SMEC_Init","PowerInit","HorizontalInit","VerticalInit","HardCostTimeInfoInit","Gen_Init","Init_4004F134","POST_Init_40051CF0","SysConfigExec","Init_4003D6AC","Runtime_40049A9C","Init_40052098","Loop_4003CF58"]
H=struct.Struct("<4I"); R=struct.Struct("<9I")
ap=argparse.ArgumentParser(); ap.add_argument("snapshot"); a=ap.parse_args()
data=Path(a.snapshot).read_bytes(); expected=H.size+len(NAMES)*R.size
if len(data)!=expected: raise SystemExit(f"bad size {len(data)}, expected {expected}")
magic,version,count,mask=H.unpack_from(data)
if (magic,version,count)!=(MAGIC,VERSION,len(NAMES)): raise SystemExit("bad header")
print(f"version={version} checkpoints={count} sample_mask=0x{mask:08X}")
print("checkpoint      cap cfg0       cfg1       first      last       changed    high       low        samples activity")
for i,name in enumerate(NAMES):
 v=R.unpack_from(data,H.size+i*R.size); cap,c0,c1,first,last,changed,high,low,samples=v
 print(f"{name:15s} {cap:3d} 0x{c0:08X} 0x{c1:08X} 0x{first:08X} 0x{last:08X} 0x{changed:08X} 0x{high:08X} 0x{low:08X} {samples:8d} {'YES' if changed & mask else 'NO'}")
