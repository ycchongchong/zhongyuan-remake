#!/usr/bin/env python3
"""Controlled original-ROM unification score fixtures; not a natural playthrough."""
import argparse, ctypes as C, hashlib, json
from pathlib import Path
from reference_capture import Reference
parser=argparse.ArgumentParser(description=__doc__)
for name in ['rom','core','entry-state','output']:parser.add_argument('--'+name,type=Path,required=True)
args=parser.parse_args()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(args.rom)=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
assert sha(args.entry_state)=='c84cc552b7297180168d0321acf0b70ec436a96519454bf2301220e35c6d4e83'
r=Reference(str(args.core),str(args.rom),str(args.output))

profiles=[{'name':f'ruler-{k}-difficulty-{d}','ruler':k,'difficulty':d} for k in range(6) for d in range(3)]
profiles += [{'name':f'score-{label}','count':120,'loyalty':loyalty,'control':control} for label,loyalty,control in [(59,77,0),(60,80,0),(79,137,0),(80,140,0),(99,197,0),(100,200,0),('clamped',255,100)]]
profiles += [{'name':'one-officer','count':1,'loyalty':0,'control':0},{'name':'maximum-officers','count':241,'loyalty':100,'control':100}]
rows=[];out=args.output
try:
 for case in profiles:
  r.restore(args.entry_state);r.run(30)
  s=(C.c_uint8*8192).from_address(r.core.retro_get_memory_data(0));ram=(C.c_uint8*2048).from_address(r.core.retro_get_memory_data(2))
  ruler=case.get('ruler',4)
  for city in range(30):
   s[city*36]=(s[city*36]&248)|ruler
   if 'control' in case:s[city*36+14]=case['control']
  if 'count' in case:
   ids=[6] if case['count']==1 else list(range(case['count']))
   for city in range(30):
    for slot in range(12):
     i=city*12+slot;s[city*36+16+slot]=ids[i] if i<len(ids) else 255
  if 'loyalty' in case:
   for officer in range(241):s[0x438+officer*8+5]=case['loyalty']
  s[0xd8b]=ruler;s[0xd89]=ruler;s[0xd8a]=0;s[0xd88]=case.get('difficulty',0)
  before=bytes(s);(out/(case['name']+'.bin')).write_bytes(before)
  r.run(8,['A']);r.run(300)
  assert ram[0x613]==0x26,(case,ram[0x613])
  row={**case,'sram':case['name']+'.bin','sram_sha256':hashlib.sha256(before).hexdigest(),'score':ram[0x37a],'variant':ram[0x375],'adviser':ram[0x6a3]}
  rows.append(row);print(row,flush=True)
 (out/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')
finally:r.audio.clear();r.close()

(out/'provenance.json').write_text(json.dumps({'rom_sha256':sha(args.rom),'core_sha256':sha(args.core),'entry_state_sha256':sha(args.entry_state),'method':'Controlled SRAM ownership/rosters before original A11F and A17B-A29F execution; no natural campaign claim'},indent=2)+'\n')
