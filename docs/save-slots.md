# 三槽存档及读档边界

以下计数记录存档改进最初验收；随后加入的 v3 战斗暂存与最新验证见 [本地进度](completion-pass.md)。新游戏保存 v3；加载 v1/v2 后保持旧记录语义，不自动猜测跨战斗的命令暂存。

地图存档、战场存档和标题继续游戏现可选择三个独立文件。槽 1 沿用旧路径，选择窗口显示年月、玩家和阶段；选择期间暂停时钟、电脑回合及自动交锋。取消战场存档会返回战场，自动推进保持停止。加载成功会关闭旧战局的弹窗，再恢复存档中的待处理操作。

读档恢复界面此前调用了实际访问城池的方法，可能领取已返回的搜索队并消耗随机数。现在恢复选中显示时不触发访问；玩家随后主动进入城池仍会显示搜索报告。测试同时覆盖游戏内读档和标题续档。

原有临时文件写入流程保留，但 Windows 最终替换不再使用 Godot 4.5.2 的 `DirAccessWindows::rename`：该实现先删除目标文件，再调用 `MoveFileW`，后一步失败会丢失旧档。现调用 `MoveFileExW` 的替换模式，不执行预先删除。Linux 使用文件系统 rename。依据：[Godot 实现](https://github.com/godotengine/godot/blob/4.5.2-stable/drivers/windows/dir_access_windows.cpp)、[Windows API](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)。此修复不承诺断电恢复或多进程同时写同一槽。

本轮 Linux 界面相关 9 组共 587 项检查通过，其中新存档流程 45 项；最终 C++ 扩展重新构建后复验存档及旧原型接口 56 项。Linux 文件替换 8 项通过，Windows/Wine 文件替换 12 项通过，包含源文件和目标文件不允许删除共享时的失败保护。Windows 导出运行时的相同 45 项存档检查通过。图形模式也完成 45 项，并检查了三槽窗口截图。Windows 验证使用 Wine 9，未替代实体 Windows 验收。

这些是电脑存档机制改进，不代表原版记录界面、整局通关或完整重制验收通过。源码、测试和构建结果保留本地；按用户要求暂不同步 GitHub。既有发布 ZIP 未改动。详细运行日志位于工作区 `work/save-slots/`。
