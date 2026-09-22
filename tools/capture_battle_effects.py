#!/usr/bin/env python3
"""Trace original melee/bow driver calls from controlled unit layouts; offline only.

The seeded cases are diagnostics, not a normal-start campaign or mixed PCM proof.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from reference_capture import Reference
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--core',type=Path,required=True)
p.add_argument('--start-state',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(a.rom)=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
assert sha(a.start_state)=='67558a05802ddfc28def6324c5bc18e34fb78b2dbd0bd867498c8d087db25a4e'
a.output.mkdir(parents=True,exist_ok=True)
r=Reference(str(a.core),str(a.rom),str(a.output))
r.core.retro_study_trace_start.argtypes=[C.c_uint]*4
r.core.retro_study_trace_stop.argtypes=[C.c_void_p]
r.core.retro_study_trace_stop.restype=C.c_uint
rng=random.Random(4187);rows=[]
try:
 for index in range(96):
  r.restore(a.start_state)
  ram=(C.c_uint8*2048).from_address(r.core.retro_get_memory_data(2));s=(C.c_uint8*8192).from_address(r.core.retro_get_memory_data(0))
  side=index%2;kind=(index//2)%4;mode=(index//8)%3;slot=0 if kind==3 else 1+(index//24)%10;token=side*128+slot
  ram[0x39]=10;ram[0x686]=4;ram[0x68b]=1;ram[0x68a]=token;ram[0x66d]=0x11;ram[0x6a5]=255;ram[0x64a]=255;ram[0x63e]=0;ram[0x3d]=0;ram[0x690]=4;ram[0x2e]=index&255
  for i in range(8):ram[0x6ad+i]=mode
  cells=rng.sample(range(16,160),22)
  for i in range(160):s[0xe1a+i]=s[0xf1a+i]=rng.choice([3,3,3,5,6,8,14,23,24,25])
  for army in range(2):
   for unit in range(11):
    b=0x4b0+army*33+unit*3;pos=cells[army*11+unit]
    ram[b]=army*128+(3 if unit==0 else unit%3);ram[b+1]=99;ram[b+2]=pos
    s[0xe1a+pos]=0x40|(army*128+unit)
  ram[0x4b0+side*33+slot*3]=side*128+kind
  events=[];shadow=bytearray(ram);previous=0
  for frame in range(80):
   r.core.retro_study_trace_start(90000,3,0,0x7fff);r.run(1)
   n=r.core.retro_study_trace_stop(None);assert n<90000;buf=C.create_string_buffer(n*16);r.core.retro_study_trace_stop(buf)
   for off,pc,acc,x,y,sp,flags,lo,hi,v,w,_ in struct.iter_unpack('<IH10B',buf.raw):
    if w:
     addr=lo+256*hi
     if addr<0x2000:shadow[addr&0x7ff]=v
    else:
     if pc==0xe6a6:events.append(dict(frame=frame,descriptor=acc,caller=hex(previous),global_phase=shadow[0x39],phase=shadow[0x686],counter=shadow[0x68b],active=shadow[0x68a],unit_kind=shadow[0x4b0+(shadow[0x68a]&15)*3+(33 if shadow[0x68a]&128 else 0)]&3))
     previous=off+16
   if shadow[0x686]!=4 or shadow[0x68b] in [2,3,8]:break
  if events:rows.append(dict(case=index,events=events))
  r.audio.clear()
 events=[e for row in rows for e in row['events']]
 assert any(e['descriptor']==25 and e['caller']=='0x1257e' and e['counter']==1 for e in events)
 assert any(e['descriptor']==25 and e['caller']=='0x12974' and e['counter']==3 for e in events)
 for descriptor in [23,24]:
  assert any(e['descriptor']==descriptor and e['unit_kind']==2 and e['counter']==1 for e in events)
 report=dict(passed=True,rom_sha256=sha(a.rom),core_sha256=sha(a.core),start_state_sha256=sha(a.start_state),cases=96,events=rows)
 (a.output/'battle-effect-trace.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Captured',sum(len(row['events']) for row in rows),'driver calls from',len(rows),'of 96 controlled layouts')
finally:r.audio.clear();r.close()
