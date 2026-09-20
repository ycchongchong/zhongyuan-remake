#!/usr/bin/env python3
"""Offline capture of the two original diagnosis introductions; not a runtime emulator.

Requires the two verified original input-boundary states. Captures 480 frames per
intro, records original sound-driver calls, verifies a16-frame visual wait loop,
and exports lossless RGB keyframes. Old-screen transition frames are omitted.
"""
import argparse
import ctypes as C
import hashlib
import json
import shutil
import struct
from pathlib import Path
import numpy as np
from PIL import Image
from reference_capture import Reference
parser=argparse.ArgumentParser(description=__doc__)
for name in ['rom','core','menu-state','second-result-state','output','evidence-dir']:
 parser.add_argument('--'+name,type=Path,required=True)
args=parser.parse_args()
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
assert sha(args.rom)=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
assert sha(args.menu_state)=='95019cde9fb6edbcbabe1ed2f880ef623f3d80962460b0ae6a6cb168574ba379'
assert sha(args.second_result_state)=='ca83a0ab62f88ccdff5e55299fbe1087e20b55503d7b22c268cc9ca53bd6cec3'
out=args.evidence_dir;out.mkdir(parents=True,exist_ok=True)
r=Reference(str(args.core),str(args.rom),str(out))
r.core.retro_study_trace_start.argtypes=[C.c_uint]*4
r.core.retro_study_trace_stop.argtypes=[C.c_void_p];r.core.retro_study_trace_stop.restype=C.c_uint
manifest={'schema':1,'fps':60,'clips':{},'rom_sha256':hashlib.sha256(r.rom_bytes).hexdigest(),'core_sha256':hashlib.sha256(r.core_path.read_bytes()).hexdigest()}
try:
 for name,source,key in [('quiz-intro',args.menu_state,'START'),('quiz-intro-second',args.second_result_state,'A')]:
  r.restore(source)
  if name=='quiz-intro':r.run(30);r.run(8,['START']);r.run(120)
  r.output=out;r.capture(name+'-entry')
  rows=[];sounds=[];previous=None;last_change=0;r.audio.clear()
  for frame in range(480):
   r.core.retro_study_trace_start(100000,1,0,0xffff)
   r.run(1,[key] if frame<8 else [])
   n=r.core.retro_study_trace_stop(None);assert n<100000
   buf=C.create_string_buffer(n*16);r.core.retro_study_trace_stop(buf)
   for off,pc,a,x,y,sp,p,lo,hi,v,w,_ in struct.iter_unpack('<IH10B',buf.raw):
    if pc in (0xe6a6,0xe632):sounds.append({'frame':frame,'pc':hex(pc),'id':a})
   raw=r.pixels.tobytes()
   if raw!=previous:
    file=f'{name}-{frame:03}.png';Image.fromarray(r.pixels).save(out/file)
    rows.append({'frame':frame,'file':file,'rgb_sha256':hashlib.sha256(raw).hexdigest(),'sha256':hashlib.sha256((out/file).read_bytes()).hexdigest()});previous=raw;last_change=frame
  target=np.array(Image.open(Path(__file__).resolve().parents[1]/'assets/original'/(name+'.png')).convert('RGB'))
  equality=bool(np.array_equal(target,r.pixels))
  print(name,'frames',len(rows),'lastchange',last_change,'sounds',len(sounds),'matches_static',equality,flush=True)
  manifest['clips'][name]={'duration_frames':480,'frames':rows,'sounds':sounds,'source_state_sha256':hashlib.sha256(Path(source).read_bytes()).hexdigest(),'entry_state_sha256':hashlib.sha256((out/(name+'-entry.state')).read_bytes()).hexdigest(),'matches_static':equality}
  r.capture(name+'-final')
 (out/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
finally:r.close()

src=args.evidence_dir;out=args.output;out.mkdir(parents=True,exist_ok=True)
data=json.loads((src/'source-manifest.json').read_text());result={k:v for k,v in data.items() if k!='clips'};result['clips']={}
for name,start,loop in [('quiz-intro',33,153),('quiz-intro-second',8,59)]:
 c=data['clips'][name];assert c['matches_static']
 def row_at(frame):return next(row for row in reversed(c['frames']) if row['frame']<=frame)
 for frame in range(loop+16,c['duration_frames']):assert row_at(frame)['rgb_sha256']==row_at(loop+(frame-loop)%16)['rgb_sha256']
 rows=[]
 for frame in sorted(set([start,loop]+[r['frame'] for r in c['frames'] if start<=r['frame']<loop+16])):
  row=row_at(frame);target=row['rgb_sha256']+'.png'
  shutil.copyfile(src/row['file'],out/target)
  rows.append({'frame':frame-start,'file':target,'rgb_sha256':row['rgb_sha256'],'sha256':row['sha256']})
 result['clips'][name]={'frames':rows,'text_frames':[s['frame']-start for s in c['sounds'] if s['pc']=='0xe6a6' and s['id']==19 and s['frame']>=start], 'loop_start':loop-start,'loop_frames':16,'source_start_frame':start,'source_verified_frames':c['duration_frames'],'entry_state_sha256':c['entry_state_sha256']}
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
print('Exported',sum(len(c['frames']) for c in result['clips'].values()),'keyframes',len(list(out.glob('*.png'))),'unique lossless frames')
