# Windows 64 位开发包

用户在完整重制仍未完成时，明确要求打包当前 Windows 版。本包是开发版，不把 `completion-manifest.json` 改为完成，也不覆盖既有 Linux/源码 ZIP。

成品：`../zhongyuan-windows-dev.zip`，解压后双击 `Zhongyuan.exe`。EXE、PCK 与 DLL 保持在同一目录。存档位置为 `%APPDATA%\ZhongyuanRemakeDev`。包内附带本次指定的中文 ROM，参考路径在打包暂存目录中替换成 `res://reference/original.nes`，不修改开发工程的 ROM 设置。

## 构建

使用 MinGW-w64 GCC 13.2 POSIX 和项目内 godot-cpp 编译 x86_64 DLL，静态链接 C++ 运行库。DLL 的导入表仅有 KERNEL32.dll 和 msvcrt.dll。

```bash
ZHONGYUAN_MINGW_ROOT=/path/to/mingw/usr cmake -S /path/to/project -B /path/to/windows-build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/project/native/cmake/mingw64.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DZHONGYUAN_EXTENSION_OUTPUT_DIR=/path/to/windows-bin
cmake --build /path/to/windows-build --target zhongyuan_gdextension --parallel 8

python3 /path/to/project/tools/package_windows.py \
  --godot /path/to/project/runtime/godot \
  --template /path/to/windows_release_x86_64.exe \
  --dll /path/to/windows-bin/zhongyuan_core.dll \
  --rom /path/to/reference.nes \
  --work-dir /path/to/new-staging-work \
  --output-dir /path/to/windows-output
```

模板取自 [Godot 4.5.2 官方归档](https://godotengine.org/download/archive/4.5.2-stable/)，整包 SHA512 与官方 SHA512-SUMS.txt 一致。导出脚本保留独立暂存工程，工作目录必须是新的；Linux 编辑器需要工程内现有 `.so` 进行导出，但它不会被放进 Windows 包。

Godot 默认导出只保留导入资源，运行时 `Image.load_from_file` 和 `AudioStreamWAV.load_from_file` 仍需要原始数据。因此暂存工程启用 `tools/windows_export`，按 [EditorExportPlugin.add_file](https://docs.godotengine.org/en/4.5/classes/class_editorexportplugin.html#class-editorexportplugin-method-add-file) 加入原始 PNG/WAV。这里不改变画面或声音数据。独立重导出的 EXE、PCK、DLL 与首个成功导出逐字节一致。

## 实际验证及限制

Windows EXE/DLL 在 Wine 9 中运行；无界面检查覆盖原生扩展、ROM 指纹、30城/241武将、中文文件名存读档、15支音频、3种结局画与音乐、长战斗记录恢复。单人和双人开场路径另作检查。ZIP 解压到含中文和空格的目录后再次运行，核对全部2665份原始 PNG/WAV 的 SHA256。

实际窗口使用 Xvfb + Mesa llvmpipe 绘制，1200×760 截图已查看。Wine 报告拖放注册失败，窗口内按钮和绘制正常；文件拖放未验收。音频测试使用 Dummy 驱动，因此证明资源可读及播放状态，不证明实体扬声器输出。

Wine 默认 dinput8 在官方原始模板执行 `--version` 时也出现启动故障。禁用该 Wine 模块后引擎和成包测试通过；这个测试环境设置不修改 EXE，也不随包禁用 Windows 手柄。Windows 实机、实体声音和手柄仍未验收。

完整重制的音画、剩余战斗边界、正常统一胜局及整局 ROM 同步验收仍未完成。详见 `current-progress.md`。本次完整日志和构建摘要见 `windows-package-validation.json`。
