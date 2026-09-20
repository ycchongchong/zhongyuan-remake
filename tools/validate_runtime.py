#!/usr/bin/env python3
"""Bounded Godot regression runs; a truncated or timed-out run is a failure."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import time

COMPLETION = {
    "original_unification": "ORIGINAL UNIFICATION: 0 failures",
    "original_audio_lifecycle": "ORIGINAL AUDIO LIFECYCLE: 0 failures",
    "original_quiz_input": "ORIGINAL QUIZ INPUT: 0 failures",
    "original_quiz_presentation": "ORIGINAL QUIZ PRESENTATION: 0 failures",
    "original_opening_presentation": "ORIGINAL OPENING PRESENTATION: 0 failures",
    "original_campaign_ending": "ORIGINAL CAMPAIGN ENDING: 0 failures",
    "original_audio_close": "ORIGINAL AUDIO CLOSE: 0 failures",
    "original_audio": "ORIGINAL AUDIO: 0 failures",
    "original_campaign_defeat": "ORIGINAL CAMPAIGN DEFEAT: 0 failures",
    "original_tactical_picture": "ORIGINAL TACTICAL PICTURE: 0 failures",
    "original_history": "ORIGINAL HISTORY: 0 failures",
    "original_empty_role": "ORIGINAL EMPTY ROLE: 0 failures",
    "original_clash_tail": "ORIGINAL CLASH TAIL: 0 failures",
    "original_invasion_endings": "ORIGINAL INVASION ENDINGS: 0 failures",
    "original_invasion_result": "ORIGINAL INVASION RESULT: 0 failures",
    "original_attacker_ai": "ORIGINAL ATTACKER AI: 0 failures",
    "original_player_strategy": "ORIGINAL PLAYER STRATEGY: 0 failures",
    "original_computer_role": "ORIGINAL COMPUTER ROLE: 0 failures",
    "original_computer_scratch": "ORIGINAL COMPUTER SCRATCH: 0 failures",
    "original_computer_occupied_fort": "ORIGINAL COMPUTER OCCUPIED FORT: 0 failures",
    "original_computer_nearby": "ORIGINAL COMPUTER NEARBY: 0 failures",
    "original_computer_scan": "ORIGINAL COMPUTER SCAN: 0 failures",
    "original_computer_flank": "ORIGINAL COMPUTER FLANK: 0 failures",
    "original_computer_motion": "ORIGINAL COMPUTER MOTION: 0 failures",
    "original_strategy_effect": "ORIGINAL STRATEGY EFFECT: 0 failures",
    "original_computer_strategy": "ORIGINAL COMPUTER STRATEGY: 0 failures",
    "original_computer_fort": "ORIGINAL COMPUTER FORT: 0 failures",
    "original_tactical_ai": "ORIGINAL TACTICAL AI: 0 failures",
    "original_human_failure": "ORIGINAL HUMAN FAILURE: 0 failures",
    "original_ruler_result": "ORIGINAL RULER RESULT: 0 failures",
    "original_time_limit": "ORIGINAL TIME LIMIT: 0 failures",
    "original_commander_result": "ORIGINAL COMMANDER RESULT: 0 failures",
    "original_remaining_results": "ORIGINAL REMAINING RESULTS: 0 failures",
    "original_withdrawal_result": "ORIGINAL WITHDRAWAL RESULT: 0 failures",
    "original_defender_retreat": "ORIGINAL DEFENDER RETREAT: 0 failures",
    "original_tactical_turn": "ORIGINAL TACTICAL TURN: 0 failures",
    "original_strategy_resume": "ORIGINAL STRATEGY RESUME: 0 failures",
    "original_duel": "ORIGINAL DUEL: 0 failures",
    "original_campaign": "ORIGINAL CAMPAIGN: 0 failures",
    "opening": "OPENING RESULT: 0 failures",
    "original_world": "PASS: native-decoded town sprites and background match all 40,960 original road-screen pixels",
    "original_data": "PASS: native original-data binding, 30 cities, 241 names, invalid input and state preservation",
    "smoke": "RESULT: 0 failures",
    "native_integration": "NATIVE INTEGRATION: 0 failures",
    "ui": "UI RESULT: 0 failures",
}


def run_check(project, name, log_path, timeout=60, fail_on_leaks=False):
    env = os.environ.copy()
    env.pop("LD_LIBRARY_PATH", None)
    start = time.monotonic()
    # A separate group also bounds the launcher's initial asset-import process.
    with Path(log_path).open("w") as log:
        process = subprocess.Popen(
            ["bash", str(Path(project) / "start-linux.sh"), "--headless",
             "--disable-crash-handler", "--script", f"res://tests/{name}.gd"],
            env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
        )
        timed_out = False
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
    output = Path(log_path).read_text(errors="replace")
    complete = COMPLETION[name] in output.splitlines()
    leaks = "ObjectDB instances leaked" in output or "Leaked instance:" in output
    passed = (not timed_out and process.returncode == 0 and complete and not (fail_on_leaks and leaks)
              and not any(word in output for word in ("ERROR:", "FAIL:", "runtime error:", "AddressSanitizer:")))
    return {"name": name, "passed": passed, "objectdb_leaks": leaks, "timed_out": timed_out,
            "exit_code": process.returncode, "completion_marker": complete,
            "checks": output.count("PASS:"), "seconds": round(time.monotonic() - start, 3),
            "log": str(Path(log_path).resolve()), "output": output}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--tests", nargs="+", choices=COMPLETION, default=list(COMPLETION))
    parser.add_argument("--fail-on-leaks", action="store_true", help="Fail a run that reports leaked Godot objects at exit")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=300, help="Per-test limit in seconds; pixel-by-pixel presentation checks need more than 60 seconds")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.repeat < 1 or args.timeout <= 0:
        parser.error("repeat and timeout must be positive")
    directory = args.output or args.project / ".user-data" / "validation"
    directory.mkdir(parents=True, exist_ok=True)
    report = {"passed": False, "requested_runs": args.repeat * len(args.tests), "runs": []}
    for iteration in range(args.repeat):
        for name in args.tests:
            result = run_check(args.project, name, directory / f"{iteration}-{name}.log", args.timeout, args.fail_on_leaks)
            report["runs"].append(result)
            report["passed"] = (len(report["runs"]) == report["requested_runs"]
                                and all(run["passed"] for run in report["runs"]))
            (directory / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
            print(f"{'PASS' if result['passed'] else 'FAIL'}: {iteration + 1}/{args.repeat} {name} "
                  f"({result['checks']} checks, {result['seconds']}s, exit {result['exit_code']}, "
                  f"timeout={result['timed_out']})", flush=True)
            if not result["passed"]:
                return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
