#!/usr/bin/env python3
import sys,ctypes as C,json,hashlib
from pathlib import Path

from reference_capture import Reference
import argparse
parser=argparse.ArgumentParser(description="Controlled original full ending screens; not a natural winning campaign.")
for name in ['rom','core','entry-state','output','evidence-dir']:parser.add_argument('--'+name,type=Path,required=True)
args=parser.parse_args()
assert hashlib.sha256(args.rom.read_bytes()).hexdigest()=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
assert hashlib.sha256(args.entry_state.read_bytes()).hexdigest()=='c84cc552b7297180168d0321acf0b70ec436a96519454bf2301220e35c6d4e83'
out=args.output;out.mkdir(parents=True,exist_ok=True)
r=Reference(str(args.core),str(args.rom),str(args.evidence_dir));rows=[]
try:
 for case in range(36):
  ruler=(case%18)//3;difficulty=case%3;year=190+case;month=case%12+1
  quality=[0,50,100][difficulty] if case<18 else [65,85,100][difficulty]
  r.restore(args.entry_state);r.run(30)
  s=(C.c_uint8*8192).from_address(r.core.retro_get_memory_data(0));ram=(C.c_uint8*2048).from_address(r.core.retro_get_memory_data(2))
  for city in range(30):s[city*36]=(s[city*36]&248)|ruler;s[city*36+14]=quality
  for officer in range(241):s[0x438+officer*8+5]=quality
  if case>=18 and difficulty==2:
   # Controlled full-recruitment endpoint exercises the three-digit score100.
   for city in range(30):
    for slot in range(12):s[city*36+16+slot]=city*4+slot if slot<4 else 255
  s[0xd8b]=s[0xd89]=ruler;s[0xd8a]=0;s[0xd88]=difficulty;s[0xd85]=year&255;s[0xd86]=year>>8;s[0xd87]=month
  r.run(8,['A']);r.run(300)
  assert ram[0x613]==0x26
  score=ram[0x37a];variant=ram[0x375]
  r.run(8,['A']);r.run(1200)
  name=f'case-{case}-ruler-{ruler}-difficulty-{difficulty}'
  (out/(name+'.rgb')).write_bytes(r.pixels.tobytes())
  rows.append(dict(name=name,year=year,month=month,difficulty=difficulty,ruler=ruler,score=score,variant=variant,rgb=name+'.rgb',sha256=hashlib.sha256(r.pixels.tobytes()).hexdigest()))
  print(rows[-1],flush=True);r.audio.clear()
 (out/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')
finally:r.audio.clear();r.close()
