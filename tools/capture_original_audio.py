"""Controlled, offline ROM audio capture. Never part of the game runtime.

Start from the silent original main menu; seed the same channel descriptors
written by E6A6. The original driver, APU and N163 then render every frame.
Loop detection compares all live tracker records and the vibrato phase.
It does not assert equality of the emulator's oscillator/filter state.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import wave

from reference_capture import Reference

TRACKS = {
    'title': list(range(70, 76)), 'diagnosis': list(range(67, 70)),
    'campaign': [0, 1, 2], 'tactical': [3, 4, 5],
    'clash_orders': [26, 27, 28], 'clash': [6, 7, 8],
    'duel': [9, 10, 11], 'battle_result': [12, 13, 14],
    'defeat': [63, 64, 65, 66], 'confirm': [16], 'cursor': [17], 'text': [19],
    'unification_0': [104, 105, 106, 107, 108],
    'unification_1': [109, 110, 111, 112, 113],
    'unification_2': [100, 101, 102, 103, 114],
}

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tracks', nargs='+', choices=list(TRACKS), help='Capture only these cues into a separate output directory')
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--start-state', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    args = parser.parse_args()
    ROM, CORE, STATE, ASSETS, OUT = args.rom, args.core, args.start_state, args.output, args.evidence_dir
    if sha(ROM) != '9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959':
        raise ValueError('This sound table is only valid for the verified Chinese ROM')
    if sha(STATE) != '95019cde9fb6edbcbabe1ed2f880ef623f3d80962460b0ae6a6cb168574ba379':
        raise ValueError('Expected the verified silent main-menu reference state')
    OUT.mkdir(parents=True, exist_ok=True)
    ASSETS.mkdir(parents=True, exist_ok=True)
    rom = ROM.read_bytes()
    def read(address):
        assert 0xa000 <= address < 0xe000
        return rom[address + (0x4010 if address < 0xc000 else 0xa010)]
    table = int.from_bytes(rom[0xe050:0xe052], 'little')
    catalog = [list(read(table + i * 4 + j) for j in range(4)) for i in range(115)]
    manifest = {'schema': 1, 'rom_sha256': sha(ROM), 'core_sha256': sha(CORE),
                'start_state_sha256': sha(STATE), 'sample_rate': 48000,
                'capture': 'controlled original E6A6 descriptors; no game input',
                'loop_basis': 'all active 22-byte tracker records plus 07B6 & 15; oscillator phase not compared',
                'encoding': 'lossless PCM16 48000 Hz mono (original left/right samples equal)',
                'descriptor_table_cpu': table, 'descriptors': catalog, 'tracks': {}}
    r = Reference(str(CORE), str(ROM), str(OUT))
    try:
        for name in args.tracks or TRACKS:
            ids = TRACKS[name]
            r.restore(STATE)
            ram = (C.c_uint8 * 2048).from_address(r.core.retro_get_memory_data(2))
            # Fixed controlled initial condition, including envelope/vibrato fields.
            for i in range(0x700, 0x7b7): ram[i] = 0
            for i in range(8): ram[0x700 + i * 22] = 255
            for sound_id in ids:
                slot, channel, lo, hi = catalog[sound_id]
                assert slot % 22 == 0 and slot < 176
                ram[0x700 + slot:0x704 + slot] = [0, channel, lo, hi]
            r.audio.clear()
            seen = {}
            loop_start = loop_end = None
            end_reason = ''
            for frame in range(24000):
                r.run(1)
                records = [bytes(ram[0x700 + i * 22:0x716 + i * 22]) for i in range(8)]
                active = [v for v in records if v[0] != 255]
                key = b''.join(v if v[0] != 255 else b'\xff' for v in records) + bytes([ram[0x7b6] & 15])
                samples = len(r.audio) // 4
                if not active:
                    # Retain hardware envelope/filter decay after the score stops.
                    r.run(30)
                    end_reason = 'score_end'
                    break
                if key in seen and frame - seen[key][0] > 120:
                    loop_frame, loop_start = seen[key]
                    loop_end = samples
                    end_reason = 'tracker_repeat'
                    break
                seen.setdefault(key, (frame, samples))
            assert end_reason, name + ': no end or verified tracker loop'
            import numpy as np
            stereo = np.frombuffer(bytes(r.audio), dtype='<i2').reshape(-1, 2)
            assert np.array_equal(stereo[:, 0], stereo[:, 1]), 'not mono'
            mono = stereo[:, 0].copy()
            wav_path = OUT / (name + '.wav')
            with wave.open(str(wav_path), 'wb') as wav:
                wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(48000)
                wav.writeframes(mono.tobytes())
            target = ASSETS / (name + '.wav')
            shutil.copyfile(wav_path, target)
            row = {'ids': ids, 'file': name + '.wav', 'end': end_reason,
                   'frames': frame + 1, 'samples': len(mono), 'peak': int(abs(mono.astype('int32')).max()),
                   'loop_start_sample': loop_start, 'loop_end_sample': loop_end,
                   'source_pcm_sha256': sha(wav_path), 'sha256': sha(target)}
            if loop_start is not None:
                row.update(loop_frames=frame-loop_frame,
                           loop_tracker_state_hex=key.hex(),
                           seam_delta=int(mono[loop_start])-int(mono[loop_end-1]),
                           max_adjacent_delta=int(abs(np.diff(mono.astype('int32'))).max()))
            manifest['tracks'][name] = row
            print(name, json.dumps(row), flush=True)
            (ASSETS / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    finally:
        r.audio.clear()
        r.close()

if __name__ == '__main__': main()
