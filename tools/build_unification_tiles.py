#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
import numpy as np,json
import argparse,hashlib
parser=argparse.ArgumentParser(description="Rebuild native ending tile references from verified ROM and original painting captures.")
parser.add_argument('--rom',type=Path,required=True)
parser.add_argument('--paintings',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
rom=args.rom.read_bytes()
assert hashlib.sha256(rom).hexdigest()=='9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959'
manifest=json.loads((args.paintings/'manifest.json').read_text())
def normalize(values):
 index={};sig=[]
 for v in values:
  v=tuple(v) if isinstance(v,np.ndarray) else int(v)
  if v not in index:index[v]=len(index)
  sig.append(index[v])
 return bytes(sig),list(index)
patterns={}
for tile in range(16384):
 b=rom[0x20010+tile*16:0x20020+tile*16]
 pixels=[((b[y]>>(7-x))&1)+2*((b[y+8]>>(7-x))&1) for y in range(8) for x in range(8)]
 key,colors=normalize(pixels)
 patterns.setdefault(key,(tile,colors))
rows=[];palettes=[]
for variant in range(3):
 path=args.paintings/(str(variant)+'.png')
 assert hashlib.sha256(path.read_bytes()).hexdigest()==manifest['paintings'][str(variant)]['sha256']
 im=np.array(Image.open(path).convert('RGB'));cells=[]
 for y in range(12):
  for x in range(18):
   key,colors=normalize(im[y*8:y*8+8,x*8:x*8+8].reshape(-1,3))
   if key not in patterns:raise ValueError((variant,x,y,len(colors)))
   tile,values=patterns[key];pal=[(0,0,0)]*4
   for a,c in zip(values,colors):pal[a]=tuple(map(int,c))
   if pal not in palettes:palettes.append(pal)
   cells.append([tile,palettes.index(pal)])
 rows.append(cells)
s='// Original CHR tile references and NES capture colours for the three ending paintings.\n// Reproduce with tools/build_unification_tiles.py; no image or ROM copy is embedded.\n#pragma once\n#include <cstdint>\nnamespace zhongyuan::ending_picture {\nstruct Cell { std::uint16_t tile; std::uint8_t palette; };\ninline constexpr std::uint8_t palettes[][4][3] = {\n'
for palette in palettes:s+='    {'+','.join('{'+','.join(map(str,c))+'}' for c in palette)+'},\n'
s+='};\ninline constexpr Cell cells[3][216] = {\n'
for cells in rows:
 s+='    {\n'
 for i in range(0,len(cells),18):s+='        '+','.join('{'+str(t)+','+str(c)+'}' for t,c in cells[i:i+18])+',\n'
 s+='    },\n'
s+='};\n}\n'
args.output.write_text(s)
print('Reproduced',sum(map(len,rows)),'CHR references;',len(palettes),'palettes')
