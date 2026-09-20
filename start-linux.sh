#!/usr/bin/env bash
set -euo pipefail
game_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
game_engine="$game_dir/runtime/godot"
if [[ ! -f "$game_engine" ]]; then
  game_engine="$(command -v godot || command -v godot4 || true)"
fi
if [[ -z "$game_engine" ]]; then
  printf '%s\n' '未找到 Godot 4.5。请使用包含 runtime 的 Linux 包，或先安装 Godot 4.5 标准版。' >&2
  exit 1
fi
if [[ ! -x "$game_engine" ]]; then
  chmod +x "$game_engine"
fi
# Keep runtime files and saves inside this portable game's folder.
export XDG_DATA_HOME="$game_dir/.user-data"
export XDG_CONFIG_HOME="$game_dir/.user-config"
export XDG_CACHE_HOME="$game_dir/.user-cache"
for game_storage in "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"; do
  mkdir -p "$game_storage"
  touch "$game_storage/.gdignore"
done
# Avoid loading Conda's incompatible desktop libraries.
unset LD_LIBRARY_PATH
export XKB_CONFIG_ROOT=/usr/share/X11/xkb
export XLOCALEDIR=/usr/share/X11/locale
if [[ ! -f "$game_dir/bin/zhongyuan_core.so" ]]; then
  printf '%s\n' '正在编译 C++ 游戏核心（需要 CMake、C++17 编译器和 Python 3）…'
  cmake -S "$game_dir" -B "$game_dir/build" -DCMAKE_BUILD_TYPE=Debug
  cmake --build "$game_dir/build" --parallel 4
fi
game_extension_list="$(cat "$game_dir/.godot/extension_list.cfg" 2>/dev/null || true)"
if [[ ! -d "$game_dir/.godot/imported" || "$game_extension_list" != *res://zhongyuan.gdextension* ]]; then
  # Import only assets here. Godot 4.5 can race while generating native editor docs
  # on first headless import (godotengine/godot#111645). Runtime loads the core below.
  "$game_engine" --headless --path "$game_dir" --import --recovery-mode
fi
exec "$game_engine" --path "$game_dir" "$@"
