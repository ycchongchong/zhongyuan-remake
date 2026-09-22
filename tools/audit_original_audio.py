#!/usr/bin/env python3
"""Verify the lossless original audio bank and its recorded loop bounds."""
import argparse
import hashlib
import json
from pathlib import Path
import wave


def audit(directory):
    manifest = json.loads((directory / 'manifest.json').read_text())
    assert manifest['sample_rate'] == 48000
    assert set(manifest['tracks']) == {'title','diagnosis','campaign','tactical','clash_orders','clash','duel','battle_result','defeat','confirm','cursor','text','clash_hit','clash_bow','unification_0','unification_1','unification_2'}
    results = {}
    for cue, track in manifest['tracks'].items():
        path = directory / track['file']
        assert path.parent == directory and path.suffix == '.wav'
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        assert digest == track['sha256'] == track['source_pcm_sha256'], cue
        with wave.open(str(path), 'rb') as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 48000), cue
            samples = wav.getnframes()
            assert samples == track['samples'], cue
        assert 0 < track['peak'] < 32767, cue
        if track['end'] == 'tracker_repeat':
            assert 0 <= track['loop_start_sample'] < track['loop_end_sample'] == samples, cue
            assert track['loop_frames'] > 120 and bytes.fromhex(track['loop_tracker_state_hex']), cue
        else:
            assert track['end'] == 'score_end' and track['loop_start_sample'] is None and track['loop_end_sample'] is None, cue
        results[cue] = {'sha256': digest, 'samples': samples, 'bytes': path.stat().st_size}
    return {'passed': True, 'tracks': results, 'total_samples': sum(r['samples'] for r in results.values()),
            'total_bytes': sum(r['bytes'] for r in results.values())}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path(__file__).resolve().parents[1] / 'assets/original/audio')
    args = parser.parse_args()
    print(json.dumps(audit(args.directory), indent=2))
