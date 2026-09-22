#!/usr/bin/env python3
"""Controlled C7EF reference calls, not a synchronized campaign playthrough."""
import argparse
import ctypes as C
import hashlib
import json
import struct
from pathlib import Path
from reference_capture import Reference

parser = argparse.ArgumentParser(description=__doc__)
for name in ("rom", "core", "entry-state", "checkpoint", "output", "evidence-dir"):
    parser.add_argument("--" + name, type=Path, required=True)
args = parser.parse_args()
if hashlib.sha256(args.rom.read_bytes()).hexdigest() != "9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959":
    raise ValueError("Reference ROM mismatch")
checkpoint = json.loads(args.checkpoint.read_text())
tactics = checkpoint["battle"]["tactics"]
args.output.mkdir(parents=True, exist_ok=True)
reference = Reference(str(args.core), str(args.rom), str(args.evidence_dir))
reference.core.retro_study_trace_start.argtypes = [C.c_uint] * 4
reference.core.retro_study_trace_stop.argtypes = [C.c_void_p]
reference.core.retro_study_trace_stop.restype = C.c_uint

def execute(ram, sram):
    inside = False
    for _ in range(80):
        shadow = bytearray(32768)
        shadow[:2048], shadow[0x6000:] = bytes(ram), bytes(sram)
        reference.core.retro_study_trace_start(90000, 3, 0, 0x7fff)
        reference.run(1)
        count = reference.core.retro_study_trace_stop(None)
        if count >= 90000:
            raise RuntimeError("Trace overflow")
        buffer = C.create_string_buffer(count * 16)
        reference.core.retro_study_trace_stop(buffer)
        for offset, pc, a, x, y, sp, p, lo, hi, value, write, unused in struct.iter_unpack("<IH10B", buffer.raw):
            if not write and offset + 16 == 0x147ff:
                before, initial, stack, inside = bytes(shadow[0x6000:]), bytes(shadow[:2048]), sp, True
            if inside and not write and lo == 0x60 and sp >= stack:
                return before, bytes(shadow[0x6000:]), initial, bytes(shadow[:2048])
            if write:
                address = lo + hi * 256
                if address < 0x2000:
                    shadow[address & 0x7ff] = value
                elif address >= 0x6000:
                    shadow[address] = value
    raise RuntimeError("C7EF did not return within the capture bound")

rows = []
try:
    for argument in range(4):
        reference.restore(args.entry_state)
        ram = (C.c_uint8 * 2048).from_address(reference.core.retro_get_memory_data(2))
        sram = (C.c_uint8 * 8192).from_address(reference.core.retro_get_memory_data(0))
        sram[:] = bytes(checkpoint["sram"])
        for address, value in {0x39:12, 0x667:5, 0x66c:3, 0x66d:16, 0x673:0,
                0x676:0, 0x6a5:255, 0x6a6:129, 0x6a7:0, 0x6a8:argument,
                0x66f:tactics["points"], 0x66b:tactics["round"], 0x66a:checkpoint["battle"]["target"],
                0x674:checkpoint["battle"]["source"], 0x2e:checkpoint["random_cursor"], 0x610:0,
                0x3d:0, 0x9b:tactics.get("computer_cursor", 0), 0xa0:0, 0x6c4:tactics["carry"]}.items():
            ram[address] = value
        ram[0x480:0x498] = tactics["status"]
        before, after, initial, final = execute(ram, sram)
        if before != bytes(checkpoint["sram"]):
            raise RuntimeError("Unexpected mutation before C7EF")
        name = f"natural-argument-lifetime-{argument}.bin"
        (args.output / name).write_bytes(after)
        rows.append(dict(argument=argument, points_before=initial[0x66f], points_after=final[0x66f],
                         position_before=before[0xdab], position_after=after[0xdab], after=name))
        reference.audio.clear()
    metadata = dict(method=__doc__, cases=rows, checkpoint_sha256=hashlib.sha256(args.checkpoint.read_bytes()).hexdigest(),
                    entry_state_sha256=hashlib.sha256(args.entry_state.read_bytes()).hexdigest())
    (args.output / "natural-argument-lifetime.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Captured {len(rows)} controlled original C7EF outcomes")
finally:
    reference.audio.clear()
    reference.close()
