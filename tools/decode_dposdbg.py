#!/usr/bin/env python3
import argparse, struct
from pathlib import Path

HDR = struct.Struct('<8I')
REC = struct.Struct('<16I')
MAGIC = 0x30435254
F_CSI=1<<13; F_SEL=1<<14; F_WR=1<<15; F_RD=1<<16

def mode(v): return 'STOP' if (v & 0xff) else 'RUN'
def main():
 p=argparse.ArgumentParser(description='Decode DPOSDBG TRC0 v6')
 p.add_argument('trace', type=Path); p.add_argument('-o','--output',type=Path)
 a=p.parse_args(); data=a.trace.read_bytes()
 if len(data)<HDR.size: raise SystemExit('short file')
 magic,ver,cap,widx,wrapped,exports,dropped,rsize=HDR.unpack_from(data)
 if magic!=MAGIC: raise SystemExit(f'bad magic 0x{magic:08X}')
 if rsize!=REC.size: raise SystemExit(f'unsupported record size {rsize}')
 avail=min(cap,(len(data)-HDR.size)//rsize); count=min(widx,avail)
 first=max(0,widx-count); lines=[f'version={ver} capacity={cap} written={widx} wrapped={wrapped} exports={exports} dropped={dropped} records={count}',
 'sequence tick_ms run_count mode call_site  op     value/details']
 for seq in range(first,widx):
  slot=seq%cap; r=REC.unpack_from(data,HDR.size+slot*rsize)
  (stored,tick,run,state,flags,v69,v6a,v6b,p65,res66,status,counts,begin,last,sc,pc)=r
  site=begin
  if flags & F_CSI:
   text=f'CSI_DST bank={counts} destination=0x{v6b:08X}'
  elif flags & F_SEL: text=f'SEL    0x{v6b&255:02X}'
  elif flags & F_WR: text=f'WR     0x{v6b&255:02X}'
  elif flags & F_RD: text=f'RD     0x{v6b&255:02X}'
  else: text=f'JOB flags=0x{flags:08X} 69=0x{v69:X} 6A=0x{v6a:X} 6B=0x{v6b:X} result=0x{res66:08X}'
  lines.append(f'{stored:8d} {tick:7d} {run:9d} {mode(state):4s} 0x{site:08X} {text}')
 out='\n'.join(lines)+'\n'
 if a.output: a.output.write_text(out)
 else: print(out,end='')
if __name__=='__main__': main()
