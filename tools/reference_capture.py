#!/usr/bin/env python3
"""Local reference study only: run an external libretro NES core, record evidence.

This does not implement the remake. ROM bytes are read locally and never uploaded.
Input timelines use frame counts and NES buttons; output includes exact core state.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import wave
import numpy as np
from PIL import Image

BUTTONS = {'B': 0, 'SELECT': 2, 'START': 3, 'UP': 4, 'DOWN': 5, 'LEFT': 6, 'RIGHT': 7, 'A': 8}
class GameInfo(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p), ('size', C.c_size_t), ('meta', C.c_char_p)]
class Variable(C.Structure):
    _fields_ = [('key', C.c_char_p), ('value', C.c_char_p)]
class SystemInfo(C.Structure):
    _fields_ = [('name', C.c_char_p), ('version', C.c_char_p), ('extensions', C.c_char_p), ('fullpath', C.c_bool), ('block_extract', C.c_bool)]

class Reference:
    def __init__(self, core, rom, output):
        self.output = Path(output); self.output.mkdir(parents=True, exist_ok=True)
        self.rom = Path(rom); self.frame = 0; self.buttons = set(); self.pixels = None
        self.pixel_format = 0; self.variables = {}; self.audio = bytearray()
        self.directory = str(self.output.resolve()).encode()
        self.core_path = Path(core)
        self.core = C.CDLL(str(self.core_path.resolve()))
        self.callbacks = [
            C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)(self.environment),
            C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)(self.video),
            C.CFUNCTYPE(None, C.c_int16, C.c_int16)(lambda l, r: None),
            C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)(self.audio_batch),
            C.CFUNCTYPE(None)(lambda: None),
            C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)(self.input_state),
        ]
        for name, callback in zip(['environment','video_refresh','audio_sample','audio_sample_batch','input_poll','input_state'],self.callbacks):
            setter = getattr(self.core, 'retro_set_'+name); setter.argtypes = [type(callback)]; setter(callback)
        self.core.retro_load_game.argtypes = [C.POINTER(GameInfo)]; self.core.retro_load_game.restype = C.c_bool
        self.core.retro_serialize_size.restype = C.c_size_t
        for method in ['retro_serialize', 'retro_unserialize']:
            fn = getattr(self.core, method); fn.argtypes = [C.c_void_p, C.c_size_t]; fn.restype = C.c_bool
        self.core.retro_get_memory_data.argtypes = [C.c_uint]; self.core.retro_get_memory_data.restype = C.c_void_p
        self.core.retro_get_memory_size.argtypes = [C.c_uint]; self.core.retro_get_memory_size.restype = C.c_size_t
        self.core.retro_get_system_info.argtypes = [C.POINTER(SystemInfo)]
        info = SystemInfo(); self.core.retro_get_system_info(C.byref(info))
        self.engine = {'name': info.name.decode(), 'version': info.version.decode(), 'sha256':hashlib.sha256(self.core_path.read_bytes()).hexdigest()}
        self.core.retro_init()
        self.rom_bytes = self.rom.read_bytes(); self.buffer = C.create_string_buffer(self.rom_bytes)
        self.path_bytes = str(self.rom.resolve()).encode()
        game = GameInfo(self.path_bytes, C.cast(self.buffer,C.c_void_p),len(self.rom_bytes),None)
        if not self.core.retro_load_game(C.byref(game)): raise RuntimeError('Reference core rejected ROM')
        if hasattr(self.core, 'retro_study_prg_offset'):
            self.core.retro_study_prg_offset.argtypes=[C.c_uint]; self.core.retro_study_prg_offset.restype=C.c_uint

    def environment(self, command, pointer):
        cmd = command & 0xffff
        if cmd in (9,31,30):
            if pointer: C.cast(pointer,C.POINTER(C.c_char_p))[0]=self.directory
            return True
        if cmd == 10:
            self.pixel_format=C.cast(pointer,C.POINTER(C.c_int))[0]; return self.pixel_format in (0,1,2)
        if cmd == 3:
            C.cast(pointer,C.POINTER(C.c_bool))[0]=True; return True
        if cmd in (52,39):
            C.cast(pointer,C.POINTER(C.c_uint))[0]=0; return True
        if cmd == 16:
            variables=C.cast(pointer,C.POINTER(Variable)); index=0
            while variables[index].key:
                key=variables[index].key; spec=variables[index].value
                self.variables[key] = spec.split(b';',1)[1].strip().split(b'|')[0]
                index+=1
            # Capture uncropped native output without enhancement or cheats.
            self.variables.update({b'fceumm_overscan_v_top':b'0',b'fceumm_overscan_v_bottom':b'0',b'fceumm_hdpacks':b'disabled',b'fceumm_region':b'NTSC'})
            return True
        if cmd == 15:
            var=C.cast(pointer,C.POINTER(Variable)).contents
            if var.key in self.variables: var.value=self.variables[var.key]; return True
            return False
        if cmd == 17:
            C.cast(pointer,C.POINTER(C.c_bool))[0]=False; return True
        return False

    def video(self, data, width, height, pitch):
        if not data: return
        dtype=np.uint32 if self.pixel_format==1 else np.uint16
        bpp=4 if self.pixel_format==1 else 2
        values=np.frombuffer(C.string_at(data,pitch*height),dtype=dtype).reshape(height,pitch//bpp)[:,:width]
        if self.pixel_format==1:
            channels=[(values>>16)&255,(values>>8)&255,values&255]
        elif self.pixel_format==2:
            channels=[((values>>11)&31)*255//31,((values>>5)&63)*255//63,(values&31)*255//31]
        else:
            channels=[((values>>10)&31)*255//31,((values>>5)&31)*255//31,(values&31)*255//31]
        self.pixels=np.stack(channels,axis=2).astype(np.uint8)

    def audio_batch(self, data, frames):
        self.audio.extend(C.string_at(data,frames*4)); return frames
    def input_state(self, port, device, index, button):
        return int(port==0 and device==1 and index==0 and button in self.buttons)
    def run(self, frames, buttons=()):
        self.buttons={BUTTONS[b.upper()] for b in buttons}
        for _ in range(frames): self.core.retro_run(); self.frame+=1
        self.buttons=set()
    def restore(self, path):
        data=Path(path).read_bytes(); buffer=C.create_string_buffer(data)
        if not self.core.retro_unserialize(buffer,len(data)): raise RuntimeError('State restore failed')
        meta=Path(path).with_suffix('.json')
        if meta.exists(): self.frame=json.loads(meta.read_text()).get('frame',0)
    def capture(self, name):
        if Path(name).name != name: raise ValueError('Capture name must be a filename stem')
        base=self.output/name
        if self.pixels is not None: Image.fromarray(self.pixels).save(base.with_suffix('.png'))
        length=self.core.retro_serialize_size(); state=C.create_string_buffer(length)
        if not self.core.retro_serialize(state,length): raise RuntimeError('State serialization failed')
        base.with_suffix('.state').write_bytes(state.raw)
        for region, suffix in [(0,'.sram.bin'),(2,'.ram.bin')]:
            count=self.core.retro_get_memory_size(region); ptr=self.core.retro_get_memory_data(region)
            if ptr and count: base.with_suffix(suffix).write_bytes(C.string_at(ptr,count))
        metadata={'frame':self.frame,'rom_sha256':hashlib.sha256(self.rom_bytes).hexdigest(),'engine':self.engine,'size':[self.pixels.shape[1],self.pixels.shape[0]] if self.pixels is not None else None,'variables':{k.decode():v.decode() for k,v in self.variables.items()}}
        if hasattr(self.core,'retro_study_cpu'):
            for kind, length in [('cpu',65536),('ppu',16384)]:
                buffer=C.create_string_buffer(length); getattr(self.core,'retro_study_'+kind)(buffer)
                base.with_suffix('.'+kind+'.bin').write_bytes(buffer.raw)
            metadata['prg_windows']={hex(a):self.core.retro_study_prg_offset(a) for a in range(0x8000,0x10000,0x2000)}
            metadata['ppu_ctrl']=self.core.retro_study_ppuctrl()
        base.with_suffix('.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2))
        print(json.dumps({'capture':str(base),'frame':self.frame,'size':metadata['size']},ensure_ascii=False))
    def close(self):
        if self.audio:
            with wave.open(str(self.output/'session.wav'),'wb') as wav:
                wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(48000); wav.writeframes(self.audio)
        self.core.retro_unload_game(); self.core.retro_deinit()

def main():
    parser=argparse.ArgumentParser()
    for option in ['core','rom','output','timeline']: parser.add_argument('--'+option,required=True)
    parser.add_argument('--resume')
    args=parser.parse_args()
    reference=Reference(args.core,args.rom,args.output)
    try:
        if args.resume: reference.restore(args.resume)
        timeline=json.loads(Path(args.timeline).read_text())
        for step in timeline:
            reference.run(step.get('frames',1),step.get('buttons',[]))
            if 'capture' in step: reference.capture(step['capture'])
        (Path(args.output)/'timeline.json').write_text(json.dumps(timeline,ensure_ascii=False,indent=2))
    finally: reference.close()

if __name__=='__main__': main()
