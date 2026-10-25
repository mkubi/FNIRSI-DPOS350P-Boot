#!/usr/bin/env python3
import argparse, csv, struct
from pathlib import Path
MAGIC=0x30435254
HEADER=struct.Struct('<8I')
RECORD=struct.Struct('<16I')
FLAGS={0:'69',1:'6A',2:'6B',3:'START',4:'64',5:'65_DONE',6:'67',7:'66',8:'COMPLETE',9:'TRUNCATED',10:'OVERFLOW',11:'UNEXPECTED',12:'INCOMPLETE_PAYLOAD',13:'CSI_DESTINATION'}
def ft(v): return '|'.join(n for b,n in FLAGS.items() if v&(1<<b)) or '-'
def main():
 ap=argparse.ArgumentParser(description='DPOSDBG v6-minimal decoder'); ap.add_argument('trace'); ap.add_argument('--csv'); a=ap.parse_args()
 d=Path(a.trace).read_bytes(); magic,ver,cap,written,wrapped,exports,dropped,rs=HEADER.unpack_from(d)
 if magic!=MAGIC or ver!=5 or rs!=64: raise SystemExit(f'bad header magic=0x{magic:08X} version={ver} record_size={rs}')
 print(f'version={ver} capacity={cap} written={written} wrapped={wrapped} exports={exports} dropped={dropped} record_size={rs}')
 print('seq time mode run flags value6b begin last')
 rows=[]
 for absolute in range(max(0,written-cap),written):
  v=RECORD.unpack_from(d,32+(absolute%cap)*rs)
  if v[0]!=absolute: continue
  seq,tick,run,state,flags,v69,v6a,v6b,polls,r66,status,counts,begin,last,sc,pc=v
  row=dict(sequence=seq,tick_ms=tick,run_count=run,run_stop=state&255,flags=flags,flags_text=ft(flags),value_6b=v6b,begin_call_site=begin,last_call_site=last,primitive_count=pc)
  rows.append(row)
  mode='RUN' if row['run_stop']==0 else ('STOP' if row['run_stop']==1 else f"0x{row['run_stop']:02X}")
  print(f"{seq:4d} {tick:6d} {mode:<4} {run:5d} {row['flags_text']:<40} 0x{v6b:08X} 0x{begin:08X} 0x{last:08X}")
 if a.csv:
  with open(a.csv,'w',newline='') as f:
   w=csv.DictWriter(f,fieldnames=list(rows[0]) if rows else ['sequence']); w.writeheader(); w.writerows(rows)
if __name__=='__main__': main()
