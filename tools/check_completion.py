#!/usr/bin/env python3
"""Release prerequisite: all original-remake areas need completed, reviewable evidence."""
import json
from pathlib import Path

def check(project):
    project = Path(project).resolve()
    manifest = json.loads((project / "docs/completion-manifest.json").read_text())
    required = {"campaign", "tactics", "combat", "results", "presentation", "audio", "record_endings", "randomness", "stability", "platforms", "end_to_end"}
    areas = manifest.get("areas", [])
    failures = []
    if {a.get("id") for a in areas} != required or len(areas) != len(required):
        failures.append("Completion manifest must retain all required areas")
    for area in areas:
        if area.get("status") != "complete" or not area.get("evidence"):
            failures.append(area.get("id", "unknown") + ": incomplete or no evidence")
            continue
        for item in area["evidence"]:
            path = (project / item).resolve()
            if not path.is_relative_to(project) or not path.is_file():
                failures.append(area["id"] + ": missing local evidence " + item)
    return failures

if __name__ == "__main__":
    failures = check(Path(__file__).resolve().parents[1])
    print("\n".join(failures) if failures else "PASS: completion evidence present; reviewer must verify its substance")
    raise SystemExit(bool(failures))
