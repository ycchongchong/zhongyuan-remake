#!/usr/bin/env python3
"""Run a bounded GDB regression against a frozen local runtime snapshot.

Keep the exact engine, extension, scripts, test inputs, registers and mappings
with any crash. A successful run is not proof of the historical crash's cause.
No network access, source changes or release-package changes are performed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

from validate_runtime import COMPLETION


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--test', choices=COMPLETION, default='original_campaign')
    p.add_argument('--repeat', type=int, default=3)
    p.add_argument('--timeout', type=int, default=180)
    p.add_argument('--extension', type=Path, help='Optional diagnostic extension; never changes the playable build')
    p.add_argument('--source-project', type=Path, help='Source tree used to build --extension (defaults to project)')
    p.add_argument('--build-directory', type=Path, help='Matching build directory; retain compiler/link metadata')
    p.add_argument('--core-dump', action='store_true')
    p.add_argument('--debugger', choices=['gdb', 'engine'], default='gdb',
                   help='engine retains Godot crash reporting without requiring ptrace')
    a = p.parse_args()
    if a.repeat < 1 or a.timeout < 1:
        p.error('repeat and timeout must be positive')
    if a.debugger == 'gdb' and not shutil.which('gdb'):
        p.error('gdb is required')
    project, out = a.project.resolve(), a.output.resolve()
    if out.exists():
        p.error('use a new output directory to preserve previous failures')
    out.mkdir(parents=True)
    frozen = out / 'project'
    frozen.mkdir()
    # Copy rather than symlink or hard-link: rebuilds and edits must not change
    # the mapped executable or erase the source/test inputs behind a crash.
    for name in ['assets', 'scripts', 'tests', '.godot', 'bin', 'reference']:
        shutil.copytree(project / name, frozen / name)
    manifest_path = frozen / 'reference/manifest.json'
    manifest = json.loads(manifest_path.read_text())
    rom = Path(manifest['local_path'])
    if not rom.is_file() or sha(rom) != manifest['sha256']:
        p.error('reference ROM is missing or differs from the project manifest')
    shutil.copy2(rom, frozen / 'reference/game.nes')
    manifest['local_path'] = str(frozen / 'reference/game.nes')
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    for source in project.iterdir():
        if source.suffix in ['.godot', '.gdextension', '.tscn']:
            shutil.copy2(source, frozen / source.name)
    (frozen / 'runtime').mkdir()
    shutil.copy2(project / 'runtime/godot', frozen / 'runtime/godot')
    if a.extension:
        shutil.copy2(a.extension, frozen / 'bin/zhongyuan_core.so')
    source = (a.source_project or project).resolve()
    shutil.copytree(source / 'native', frozen / 'native')
    shutil.copy2(source / 'CMakeLists.txt', frozen / 'CMakeLists.txt')
    metadata = frozen / 'build-metadata'
    metadata.mkdir()
    if a.build_directory:
        build = a.build_directory.resolve()
        for name in ['CMakeCache.txt', 'compile_commands.json', 'build.ninja', 'CMakeFiles/rules.ninja']:
            path = build / name
            if path.is_file():
                target = metadata / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    files = {str(f.relative_to(frozen)): sha(f) for f in frozen.rglob('*') if f.is_file()}
    report = {'schema': 2, 'purpose': 'frozen ordinary GDExtension loader', 'debugger': a.debugger,
              'test': a.test, 'diagnostic_extension': str(a.extension) if a.extension else None,
              'source_project': str(source), 'build_directory': str(a.build_directory) if a.build_directory else None,
              'files': files, 'runs': [], 'historical_root_cause_confirmed': False}
    env = os.environ.copy()
    env.pop('LD_LIBRARY_PATH', None)
    for key, name in [('XDG_DATA_HOME', '.user-data'), ('XDG_CONFIG_HOME', '.user-config'), ('XDG_CACHE_HOME', '.user-cache')]:
        path = frozen / name
        path.mkdir()
        (path / '.gdignore').touch()
        env[key] = str(path)
    for index in range(a.repeat):
        run = out / ('run-%02d' % index)
        run.mkdir()
        result_path = run / 'gdb-result.json'
        # repr/json encode paths as Python literals, never as shell programs.
        script = '''set pagination off
set confirm off
set print thread-events off
set disable-randomization off
handle SIGILL stop print nopass
handle SIGSEGV stop print nopass
handle SIGBUS stop print nopass
handle SIGABRT stop print nopass
python
import gdb, json, hashlib
from pathlib import Path
folder = Path(FOLDER)
gdb.execute('set substitute-path ' + json.dumps(SOURCE) + ' ' + json.dumps(FROZEN))
result = {"signal": None, "exit_code": None}
def stopped(event):
    if isinstance(event, gdb.SignalEvent):
        result["signal"] = event.stop_signal
        for command in ["p $_siginfo", "info all-registers", "thread apply all bt full", "info proc mappings", "info sharedlibrary", "info files", "maintenance info sections", "x/32bx $pc-16", "x/12i $pc"]:
            try: print(gdb.execute(command, to_string=True))
            except gdb.error as error: print(str(error))
        # Preserve loader evidence even if PC points into .dynstr or an invalid
        # address. The caller's PLT bytes and /proc maps identify that boundary.
        try:
            frame = gdb.newest_frame()
            for index in range(4):
                if frame is None: break
                pc = frame.pc()
                try:
                    data = bytes(gdb.selected_inferior().read_memory(pc-32, 96))
                    (folder / ('frame-%d-%x.bin' % (index, pc))).write_bytes(data)
                except gdb.error as error: print(str(error))
                frame = frame.older()
            pid = gdb.selected_inferior().pid
            maps = Path('/proc/%d/maps' % pid).read_text()
            (folder / 'maps.txt').write_text(maps)
            images = {}
            for line in maps.splitlines():
                fields = line.split(maxsplit=5)
                if len(fields) != 6 or not fields[5].startswith('/'): continue
                path = fields[5]
                if path in images: continue
                try:
                    with Path(path).open('rb') as f:
                        images[path] = hashlib.file_digest(f, 'sha256').hexdigest()
                except OSError as error: images[path] = {'unreadable': str(error)}
            (folder / 'mapped-file-hashes.json').write_text(json.dumps(images, indent=2))
        except (gdb.error, OSError) as error: print(str(error))
        try:
            pc = int(gdb.parse_and_eval("$pc"))
            result["pc"] = hex(pc)
            (folder / "pc-memory.bin").write_bytes(bytes(gdb.selected_inferior().read_memory(pc-64, 256)))
        except gdb.error as error: result["memory_error"] = str(error)
        if CORE:
            try: gdb.execute('generate-core-file ' + str(folder / 'core'))
            except gdb.error as error: result["core_error"] = str(error)
def exited(event):
    result["exit_code"] = getattr(event, "exit_code", None)
gdb.events.stop.connect(stopped)
gdb.events.exited.connect(exited)
end
run
python
(folder / "gdb-result.json").write_text(json.dumps(result, indent=2) + "\\n")
end
quit
'''.replace('FOLDER', repr(str(run))).replace('CORE', repr(a.core_dump)).replace('SOURCE', repr(str(source))).replace('FROZEN', repr(str(frozen)))
        commands = run / 'commands.gdb'
        commands.write_text(script)
        argv = ['gdb', '-q', '-batch', '-x', str(commands), '--args',
                str(frozen / 'runtime/godot'), '--headless', '--audio-driver', 'Dummy',
                '--disable-crash-handler', '--path', str(frozen), '--script', 'res://tests/%s.gd' % a.test]
        if a.debugger == 'engine':
            argv = [str(frozen / 'runtime/godot'), '--headless', '--audio-driver', 'Dummy',
                    '--path', str(frozen), '--script', 'res://tests/%s.gd' % a.test]
        started = time.monotonic()
        with (run / 'gdb.log').open('w') as log:
            process = subprocess.Popen(argv, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            timed_out = False
            try:
                process.wait(a.timeout)
            except subprocess.TimeoutExpired:
                timed_out = True
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            except BaseException:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise
        output = (run / 'gdb.log').read_text(errors='replace')
        details = json.loads(result_path.read_text()) if result_path.exists() else {}
        if a.debugger == 'engine':
            details = {'exit_code': process.returncode, 'signal': -process.returncode if process.returncode < 0 else None}
        if 'ptrace: Operation not permitted' in output:
            details['blocked'] = 'sandbox forbids ptrace; inferior did not start'
        complete = COMPLETION[a.test] in output.splitlines()
        passed = (not timed_out and process.returncode == 0 and details.get('exit_code') == 0
                  and not details.get('signal') and complete
                  and not any(marker in output for marker in ['ERROR:', 'FAIL:', 'runtime error:', 'AddressSanitizer:',
                                                                'ObjectDB instances leaked', 'Leaked instance:']))
        row = dict(index=index, passed=passed, timed_out=timed_out,
                   seconds=round(time.monotonic()-started, 3), gdb_exit=process.returncode,
                   checks=output.count('PASS: '), complete=complete, **details)
        report['runs'].append(row)
        report['passed'] = all(r['passed'] for r in report['runs']) and len(report['runs']) == a.repeat
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(row), flush=True)
        if not passed:
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
