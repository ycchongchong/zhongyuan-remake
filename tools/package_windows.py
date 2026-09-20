#!/usr/bin/env python3
"""Export the explicitly requested Windows development build, not a complete remake release.
Build the DLL with native/cmake/mingw64.cmake first. Export uses a separate staging tree.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

PROJECT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('godot', 'template', 'dll', 'rom', 'work-dir', 'output-dir'):
        parser.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    for key in ('godot', 'template', 'dll', 'rom'):
        path = getattr(args, key).resolve()
        if not path.is_file():
            parser.error(f'Missing {key}: {path}')
        setattr(args, key, path)
    manifest = json.loads((PROJECT / 'reference/manifest.json').read_text())
    if hashlib.sha256(args.rom.read_bytes()).hexdigest() != manifest['sha256']:
        parser.error('ROM does not match the tested Chinese version')
    if args.dll.read_bytes()[:2] != b'MZ' or args.template.read_bytes()[:2] != b'MZ':
        parser.error('DLL and template must be Windows PE files')
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    stage = work / 'staging'
    if stage.exists():
        parser.error('Use a new work directory; staging already exists')
    stage.mkdir()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name in ('assets', 'scripts', 'data'):
        shutil.copytree(PROJECT / name, stage / name,
                        ignore=shutil.ignore_patterns('__pycache__'))
    for name in ('project.godot', 'opening.tscn', 'original_campaign.tscn',
                 'main.tscn', 'reference_data.tscn', 'zhongyuan.gdextension'):
        shutil.copy2(PROJECT / name, stage / name)
    (stage / 'bin').mkdir()
    shutil.copy2(args.dll, stage / 'bin/zhongyuan_core.dll')
    # The Linux editor needs its own extension while importing/exporting. This
    # .so is excluded from the Windows export and never becomes a runtime dependency.
    shutil.copy2(PROJECT / 'bin/zhongyuan_core.so', stage / 'bin/zhongyuan_core.so')
    (stage / 'reference').mkdir()
    manifest['local_path'] = 'res://reference/original.nes'
    (stage / 'reference/manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
    shutil.copy2(args.rom, stage / 'reference/original.nes')
    shutil.copytree(PROJECT / 'tools/windows_export', stage / 'addons/raw_assets')
    project = (stage / 'project.godot').read_text().replace(
        'config/name="中原 · 三十城争霸"',
        'config/name="中原 · 三十城争霸（Windows 开发版）"\n'
        'config/use_custom_user_dir=true\nconfig/custom_user_dir_name="ZhongyuanRemakeDev"')
    project += '\n[editor_plugins]\nenabled=PackedStringArray("res://addons/raw_assets/plugin.cfg")\n'
    (stage / 'project.godot').write_text(project)
    template = args.template.as_posix()
    (stage / 'export_presets.cfg').write_text(f'''[preset.0]
name="Windows Desktop"
platform="Windows Desktop"
runnable=true
export_filter="all_resources"
include_filter="*.json,*.nes"
exclude_filter="bin/*.so,*.md"
export_path=""
script_export_mode=2

[preset.0.options]
custom_template/debug="{template}"
custom_template/release="{template}"
binary_format/architecture="x86_64"
binary_format/embed_pck=false
codesign/enable=false
application/modify_resources=false
debug/export_console_wrapper=0
texture_format/bptc=false
texture_format/s3tc=true
texture_format/etc=false
texture_format/etc2=false
''')
    env = dict(os.environ)
    env.pop('LD_LIBRARY_PATH', None)
    for variable, name in [('XDG_DATA_HOME','editor-data'),('XDG_CONFIG_HOME','editor-config'),('XDG_CACHE_HOME','editor-cache')]:
        (work/name).mkdir(exist_ok=True)
        env[variable] = str(work/name)
    commands = [
        [str(args.godot),'--headless','--path',str(stage),'--import','--recovery-mode'],
        [str(args.godot),'--headless','--path',str(stage),'--export-release','Windows Desktop',str(output/'Zhongyuan.exe')],
    ]
    for index, command in enumerate(commands):
        log = work / ('import.log' if index == 0 else 'export.log')
        with log.open('w') as stream:
            subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=300)
        if 'ERROR:' in log.read_text():
            raise RuntimeError(f'Export/import reported errors: {log}')
    for name in ('Zhongyuan.exe','Zhongyuan.pck','zhongyuan_core.dll'):
        if not (output/name).is_file():
            raise RuntimeError('Missing export output: '+name)
    print('Exported development build:', output)
    print('Run packaged-runtime checks before creating the ZIP. Full-remake completion remains incomplete.')


if __name__ == '__main__':
    main()
