#!/usr/bin/env python3
import argparse,struct
from pathlib import Path
N=["DESTINATION_READY","CAPTURE_ACTIVE","CAPTURE_DONE_PRE_DISABLE"]
H=struct.Struct("<8I");R=struct.Struct("<18I")
a=argparse.ArgumentParser();a.add_argument("snapshot");x=a.parse_args();b=Path(x.snapshot).read_bytes()
if len(b)!=H.size+len(N)*R.size:raise SystemExit(f"bad size {len(b)}, expected {H.size+len(N)*R.size}")
magic,ver,done,count,mask,samples,_,_=H.unpack_from(b)
if magic!=0x57495343 or ver!=1 or count!=3:raise SystemExit("bad CSIWIN header")
print(f"version={ver} completed={done} phases={count} mask=0x{mask:08X} samples={samples}")
for i,n in enumerate(N):
 v=R.unpack_from(b,H.size+i*R.size);cap,c0,c1,first,last,chg,hi,lo,ns,ifc,mode,hs,dst,irq,st,g0,g1,g2=v
 print(f"{n}: cap={cap} CFG0={c0:08X} CFG1={c1:08X} PE1={(first>>1)&1} CHG={chg:08X} HIGH={hi:08X} LOW={lo:08X}")
 print(f"  CSI if={ifc:08X} mode={mode:08X} hsize={hs:08X} dst={dst:08X} irq={irq:08X} status={st:08X} geo={g0:08X}/{g1:08X}/{g2:08X}")
