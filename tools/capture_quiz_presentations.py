#!/usr/bin/env python3
"""Offline original diagnosis transition capture. Requires verified entry states.

Use a separate output directory. This tool exports the33 answer/rejection clips;
merge them with the separately verified two introduction clips for game assets.
The game itself never loads the emulator or these entry states.
"""
import argparse
import ctypes as C
import hashlib
import json
import shutil
import struct
from pathlib import Path
from PIL import Image
from reference_capture import Reference

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['rom','core','entries','output','evidence-dir']:
  parser.add_argument('--'+name,type=Path,required=True)
 args=parser.parse_args()
 project=Path(__file__).resolve().parents[1]
 specs=json.loads((project/'reference/fixtures/quiz-presentation-sources.json').read_text())
 assert sha(args.rom)=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
 for spec in specs.values():assert sha(args.entries/spec['entry_file'])==spec['entry_sha256']
 out=args.output;out.mkdir(parents=True,exist_ok=True)
 evidence=args.evidence_dir;evidence.mkdir(parents=True,exist_ok=True)
 manifest={'schema':1,'fps':60,'rom_sha256':sha(args.rom),'core_sha256':sha(args.core),'clips':{},'choices':{}}
 raw={'clips':{}}
 Reference.input_state=lambda self,port,device,index,button:int(port==self.input_port and device==1 and index==0 and button in self.buttons)
 r=Reference(str(args.core),str(args.rom),str(evidence));r.input_port=0
 r.core.retro_study_trace_start.argtypes=[C.c_uint]*4
 r.core.retro_study_trace_stop.argtypes=[C.c_void_p];r.core.retro_study_trace_stop.restype=C.c_uint
 try:
  for name,spec in specs.items():
   r.input_port=spec['port'];r.restore(args.entries/spec['entry_file'])
   rows=[];sounds=[];sequence=[];previous=None;r.audio.clear()
   for frame in range(480):
    r.core.retro_study_trace_start(100000,1,0,0xffff);r.run(1,['A'] if frame<8 else [])
    n=r.core.retro_study_trace_stop(None);assert n<100000
    buf=C.create_string_buffer(n*16);r.core.retro_study_trace_stop(buf)
    for off,pc,a,x,y,sp,p,lo,hi,v,w,_ in struct.iter_unpack('<IH10B',buf.raw):
     if pc in (0xe6a6,0xe632):sounds.append({'frame':frame,'pc':hex(pc),'id':a})
    rgb=r.pixels.tobytes();digest=hashlib.sha256(rgb).hexdigest();sequence.append(digest)
    if rgb!=previous:
     file=f'{name}-{frame:03}.png';Image.fromarray(r.pixels).save(evidence/file)
     rows.append({'frame':frame,'file':file,'rgb_sha256':digest,'sha256':sha(evidence/file)});previous=rgb
   raw['clips'][name]={'frames':rows,'sounds':sounds,'entry_state_sha256':spec['entry_sha256'],'port':spec['port']}
   r.capture(name+'-final')
   start,loop=spec['start'],spec['loop']
   for frame in range(loop+16,480):assert sequence[frame]==sequence[loop+(frame-loop)%16],name
   exported=[]
   for frame in sorted(set([start,loop]+[row['frame'] for row in rows if start<=row['frame']<loop+16])):
    row=next(row for row in reversed(rows) if row['frame']<=frame);file=row['rgb_sha256']+'.png'
    shutil.copyfile(evidence/row['file'],out/file)
    exported.append({'frame':frame-start,'file':file,'rgb_sha256':row['rgb_sha256'],'sha256':row['sha256']})
   manifest['clips'][name]={'target':spec['target'],'frames':exported,'text_frames':[s['frame']-start for s in sounds if s['pc']=='0xe6a6' and s['id']==19 and s['frame']>=start],'loop_start':loop-start,'loop_frames':16,'source_start_frame':start,'source_verified_frames':480,'entry_state_sha256':spec['entry_sha256']}
   target=spec['target']
   if target.startswith('quiz-') and target not in manifest['choices']:
    r.restore(evidence/(name+'-final.state'));r.run(8);r.run(8,['RIGHT']);r.run(24)
    file='choice-'+target+'.png';Image.fromarray(r.pixels).save(out/file)
    manifest['choices'][target]={'file':file,'sha256':sha(out/file)}
   (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
   (evidence/'source-manifest.json').write_text(json.dumps(raw,indent=2)+'\n')
   print(name,'verified',480-start,'frames;',len(exported),'keyframes',flush=True)
 finally:r.audio.clear();r.close()
if __name__=='__main__':main()
