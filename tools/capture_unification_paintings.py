#!/usr/bin/env python3
"""Reproduce three controlled NES ending paintings, not a winning playthrough."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path

from PIL import Image
from reference_capture import Reference


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("rom", "core", "entry-state", "output", "evidence-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    expected = {
        "rom": "9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959",
        "entry_state": "c84cc552b7297180168d0321acf0b70ec436a96519454bf2301220e35c6d4e83",
    }
    for key, value in expected.items():
        if digest(getattr(args, key)) != value:
            parser.error("Unexpected " + key + " fingerprint")
    args.output.mkdir(parents=True, exist_ok=True)
    reference = Reference(str(args.core), str(args.rom), str(args.evidence_dir))
    paintings = {}
    try:
        for variant, name, stat in ((0, "low", 0), (1, "high", 100), (2, "normal", None)):
            reference.restore(args.entry_state)
            reference.run(30)
            sram = (C.c_uint8 * 8192).from_address(reference.core.retro_get_memory_data(0))
            ram = (C.c_uint8 * 2048).from_address(reference.core.retro_get_memory_data(2))
            # Controlled endpoint setup. The ROM performs the actual scoring,
            # portrait selection and drawing after the confirmation inputs.
            for city in range(30):
                sram[city * 36] = (sram[city * 36] & 248) | 4
                if stat is not None:
                    sram[city * 36 + 14] = stat
            if stat is not None:
                for officer in range(241):
                    sram[0x438 + officer * 8 + 5] = stat
            sram[0xd8b] = sram[0xd89] = 4
            sram[0xd8a] = 0
            reference.run(8, ["A"])
            reference.run(300)
            if ram[0x613] != 0x26 or ram[0x375] != variant:
                raise RuntimeError("Original score endpoint did not select " + name)
            score = ram[0x37a]
            reference.run(8, ["A"])
            reference.run(1200)
            reference.capture(name + "-score")
            painting = Image.fromarray(reference.pixels).crop((56, 40, 200, 136)).convert("RGB")
            path = args.output / (str(variant) + ".png")
            painting.save(path)
            source = args.evidence_dir / (name + "-score.png")
            paintings[str(variant)] = {
                "file": path.name, "sha256": digest(path),
                "rgb_sha256": hashlib.sha256(painting.tobytes()).hexdigest(),
                "source": str(source), "source_sha256": digest(source),
                "source_rect": [56, 40, 144, 96], "observed_score": score,
            }
    finally:
        reference.audio.clear()
        reference.close()
    manifest = {
        "schema": 1,
        "source": "Controlled original unification score screen, before the late presentation tail; not a natural winning playthrough.",
        "rom_sha256": digest(args.rom), "core_sha256": digest(args.core),
        "entry_state_sha256": digest(args.entry_state),
        "size": [144, 96], "paintings": paintings,
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
