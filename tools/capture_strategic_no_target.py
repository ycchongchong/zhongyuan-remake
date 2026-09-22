#!/usr/bin/env python3
"""Capture the original BCEF return for a saved strategic no-target boundary.

Offline controlled comparison. The verified base state consumes one RNG entry
before BCEF; seed its predecessor and assert the captured entry matches the save.
This does not alter the remake save or run an emulator in the game.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
from reference_capture import Reference
p=argparse.ArgumentParser(description=__doc__)
for name in ['rom','core','start-state','save','output']:p.add_argument('--'+name,type=Path,required=True)
args=p.parse_args()
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(args.rom)=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
assert sha(args.start_state)=='222279d706ab7f858de3ffb04e776132749fdebeb0cd23a4138b0239aff6a081'
args.output.mkdir(parents=True,exist_ok=True)
r=Reference(str(args.core),str(args.rom),str(args.output))
r.core.retro_study_trace_start.argtypes=[C.c_uint]*4
r.core.retro_study_trace_stop.argtypes=[C.c_void_p];r.core.retro_study_trace_stop.restype=C.c_uint
saved=json.loads(args.save.read_text());runtime=saved['ai'];out=args.output
try:
 r.restore(args.start_state)
 ram=(C.c_uint8*2048).from_address(r.core.retro_get_memory_data(2));s=(C.c_uint8*8192).from_address(r.core.retro_get_memory_data(0));s[:]=saved['sram']
 ram[0x6a5]=128;ram[0x6a6]=64;ram[0x6a7]=0;ram[0x6a8]=saved['command_argument'];ram[0x30]=saved['frame_counter']
 for key,addr in [('budget',0x688),('phase',0x686),('plan',0x689),('round',0x68a),('a1',0xa1)]:ram[addr]=runtime[key]
 ram[0x2e]=(saved['random_cursor']-1)&255;ram[1:4]=runtime['work_gold'].to_bytes(3,'little')
 inside=False;found=False;cuts=[]
 for f in range(1000):
  shadow=bytearray(32768);shadow[:2048]=bytes(ram);shadow[0x6000:]=bytes(s)
  r.core.retro_study_trace_start(90000,3,0,0x7fff);r.run(1)
  n=r.core.retro_study_trace_stop(None);assert n<90000;buf=C.create_string_buffer(n*16);r.core.retro_study_trace_stop(buf)
  for off,pc,a,x,y,sp,p,lo,hi,v,w,_ in struct.iter_unpack('<IH10B',buf.raw):
   if not w and off+16==0xbcff and not inside:before=bytes(shadow[0x6000:]);rb=bytes(shadow[:2048]);inside=True;entrysp=sp
   if inside and not w and pc in [0xbfa9,0xc050,0xc092,0xc0ec,0xc0f2,0xbf53]:cuts.append(dict(pc=hex(pc),frame=f,random=shadow[0x2e],budget=shadow[0x688],plan=shadow[0x689],source=shadow[0x9e],target=shadow[0x9f]))
   if inside and not w and lo==0x60 and sp>=entrysp:after=bytes(shadow[0x6000:]);ra=bytes(shadow[:2048]);found=True;break
   if w:
    addr=lo+hi*256
    if addr<0x2000:shadow[addr&0x7ff]=v
    elif addr>=0x6000:shadow[addr]=v
  if found:break
 assert found,'No original AI return'
 assert before==bytes(saved['sram'])
 assert rb[0x2e]==saved['random_cursor'], 'Reference entry must match saved logic cursor'
 for suffix,data in [('before.bin',before),('after.bin',after),('before.ram',rb),('after.ram',ra)]: (out/('no-target-'+suffix)).write_bytes(data)
 fields=[('budget',0x688),('phase',0x686),('plan',0x689),('round',0x68a),('random_cursor',0x2e),('a1',0xa1)]
 row=dict(runtime={k:rb[at]for k,at in fields},expected={k:ra[at]for k,at in fields},cuts=cuts)
 row['runtime']['work_gold']=int.from_bytes(rb[1:4],'little');row['expected']['done']=ra[0x6a5]==255;row['expected']['battle_result']=ra[0x6a7];row['expected']['argument']=ra[0x6a8]
 row['changes']=[(i,a,b)for i,(a,b)in enumerate(zip(before,after))if a!=b]
 row.update(rom_sha256=sha(args.rom),start_state_sha256=sha(args.start_state),save_sha256=sha(args.save),core_sha256=sha(args.core))
 (out/'case.json').write_text(json.dumps(row,indent=2)+'\n');print(row)
finally:r.audio.clear();r.close()
