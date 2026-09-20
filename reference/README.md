# 指定中文版 ROM 的还原依据

## 文件身份

- 文件：`/home/ycww/下载/三国志Ⅰ--中原之霸者(中文版).nes`
- SHA-256：`9658ef6e04fa725bd52c326745768abeae72179b4ecc882e0b7ceb4ff7484959`
- CRC32：`b0cf9573`；总长 393232 字节。
- iNES 头：`4e45531a082033100000000000000000`。
- 128 KiB PRG、256 KiB CHR，Mapper 19；无 trainer。
- 原文件只读使用，没有修改。参考模拟器只用于观察原版，不是重制游戏的运行内核。

## 当前已完成

1. 本地原版启动，采集标题、一人/二人/继续菜单、难度、性格诊断、君主匹配、大地图、城池、查询和一次开发操作。
2. 每个采样保存原始 256×240 截图、可恢复状态、RAM、SRAM，以及只读导出的映射 CPU/PPU 内存；附帧数和核心指纹。
3. C++ 读取30个城池槽、241个武将槽、16384个8×8图块。
4. 从 ROM 的字形表解码全部城名和武将名。30个城名与六名君主、刘备/赵云的 Unicode 名称已经人工校对；其余武将保留原版图形名称，未凭记忆补写文字。
5. 新野、刘备、赵云的数据与原版画面相符；三段名称字形与原版截图逐像素匹配。
6. `reference_data.tscn` 是可点击的数据核对器，使用 C++ 解码结果和原版名称图块。

主游戏现已接入30城初始表、驻城兵力及六势力。`data/scenario.json` 保留原始字段和数据来源，`tools/build_scenario.py` 可重新生成；坐标与64条道路现已从 ROM 解码并核对原版画面；粮草/发展等级映射仍为临时实现。全游戏菜单、战斗、AI、声音和事件还没有完成原版重写。

## 表布局

偏移是**包含16字节 iNES 头的绝对文件偏移**，不是 CPU 地址。

| 表 | 文件偏移 | 记录布局 | 依据 |
| --- | --- | --- | --- |
| 城名 | `0x262C` | 30 × 8字节 | 每条含 CHR bank 和六列字形编码，下一表从 `0x271C` 开始 |
| 城池初始状态 | `0x271C` | 30 × 36字节 | 下一表 `0x2B54`；运行时从 CPU `$6000` 起，以36字节排列 |
| 武将初始状态 | `0x2B54` | 241 × 8字节 | 下一表 `0x32DC`；运行时从 CPU `$6438` 起，以8字节排列 |
| 德 | `0x32DC` | 按武将 ID 索引的字节 | 刘备 ID4 → 99，赵云 ID145 → 87，与画面吻合 |
| 武将名称指针 | `0x2022` | 每 ID 一个小端16位指针 | 指针位于 `$8000–$9FFF` 数据 bank，映射到 `0x2010 + pointer - 0x8000` |
| CHR | `0x20010` | 16384 × 16字节 | 每个8×8图块两位平面 |

记录数量由相邻表起点、连续布局和名称指针表推导；初始化循环的反汇编审计仍待完成。原始字节全部保留，避免将未知字段误当作零值。

### 城池记录：新野（ID13）

ROM `0x28F0`，运行时 CPU `$61D4`。

| 相对偏移 | 当前读取方式 | 画面校对 |
| --- | --- | --- |
| +0 | 状态字节，低3位为势力 ID | CPU `$A49B–$A4A6` / `$DE28–$DE2C` 使用 AND 7；高位语义未确认 |
| +1 | 小端三字节 | 黄金200；CPU `$B433–$B449` 三字节减法确认 |
| +4 | 小端两字节 | 土地25 |
| +6 | 小端两字节 | 商业20 |
| +8 | 小端三字节 | 人口19000；CPU `$B499–$B4A3` 三字节累加确认 |
| +14 | 一字节 | 统治55；CPU `$B4AC–$B4BB` 单字节累加并限制100 |
| +16…+27 | 十二个武将 ID 槽，`FF` 表示空槽 | 4=刘备，145=赵云 |

黄金、土地、商业、人口、统治的操作宽度已通过开发代码和三组高位借位/溢出实验确认；其他未知字节仍保留。初始 ROM 表与“玩家第一次行动时”的内存并非处处相同：在玩家开始前，电脑势力已执行过动作。

### 武将记录

| 字段 | 来源 | 刘备 ID4 | 赵云 ID145 |
| --- | --- | --- | --- |
| 体 | 记录+1 | 78 | 98 |
| 知 | 记录+2 | 63 | 85 |
| 武 | 记录+3 | 53 | 96 |
| 德 | `0x32DC + ID` | 99 | 87 |
| 忠 | 记录+5，`FF` 显示 `--` | -- | 95 |
| 步兵 | 记录+7高四位 ×100 | 400 | 0 |
| 骑兵 | 记录+7低四位 ×100 | 400 | 400 |
| 弓兵 | 记录+4高四位 ×100 | 200 | 100 |

其余标志、低四位以及记录+6尚未命名为确定规则。已纠正早期低分辨率观察中把赵云误认成关羽的识别错误，以放大截图、原始字形和数值共同为准。

### 原版名称字形

每个8×16像素列的下半块索引为 `CHR_bank×64 + (code & 63)`，上半块比它小16。武将每个编码展开为左右两个列，城名直接给六个列，值 `01` 表示空白。实际 16×16 汉字由四个8×8图块组成。

`fixtures/name-masks.json` 记录三个独立原版截图区域：新野 `(56,40,48,16)`，刘备和赵云 `(8,200,48,16)`。测试把 C++ 解码后的像素与这些截图的黑色字形掩码逐点比较。

## 一次原版操作样本

简单难度、刘备局、新野，由刘备执行“开发土地”：

- 菜单路径：城堡 → 开发 → 开发土地 → 刘备 → 确认命令书。
- 本次黄金：200 → 186。
- 本次土地：25 → 40。
- 本次统治：55 → 58。
- 本次命令书：3 → 2。

这是早期的一次自然观察，其收益不能作为固定结果。后续已经通过代码分析和六组独立 SRAM 样本移植计算公式，见下文。原始记录保留在 `land-development-observation.json`。

## 运行与验证

项目根目录：

```bash
cmake -S . -B build -DZHONGYUAN_REFERENCE_ROM='/home/ycww/下载/三国志Ⅰ--中原之霸者(中文版).nes'
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash start-linux.sh --headless --script res://tests/original_data.gd
bash start-linux.sh res://reference_data.tscn
```

命令行导出：

```bash
build/rom_inspect '/home/ycww/下载/三国志Ⅰ--中原之霸者(中文版).nes' reference/local/imported/rom-data.json reference/local/imported/chr-tiles.pgm
```

原版自动采样工具：`tools/reference_capture.py`，需要 Python 3、NumPy、Pillow 和外部 FCEUmm libretro 动态库。`tools/reference_hooks.c` 可附加到 FCEUmm 的 `src/drivers/libretro/libretro.c` 再编译，提供无总线副作用的映射内存快照；不附加也可采集画面、存档和 RAM。此工具不参与重制游戏的运行。

`timelines/liubei-easy.json` 从冷启动回放至刘备局查询操作。`timelines/develop-land.json` 以该回放的 `castle-menu.state` 为恢复点。单步输入可能早于文字播放结束，因此回放保留了原始等待和重复确认，而没有优化输入时序。

```bash
python3 tools/reference_capture.py --core /你的/fceumm_libretro.so --rom /你的/原版.nes --output reference/local/replay --timeline reference/timelines/liubei-easy.json
```

参考核心源代码：https://github.com/libretro/libretro-fceumm 。头格式与 Mapper 19 定义：https://www.nesdev.org/wiki/INES_Mapper_019 。核心、输入时序和设置需保持一致才能比较参考回放。

## 原版地图与道路

- 城池 X / Y 表：文件 `0x1DE75` / `0x1DE93`，各30字节。原版 OAM 的实际绘制纵坐标为表值 +1。
- 道路指针表：文件 `0x19A50`，30个小端指针；每个列表由 `(目标城, 原始辅助字节)` 对组成，以 `FF` 结束。CPU `$BE29–$BE4F` 使用此表判断邻接。
- 原版“线路”截图的棕色连线经过像素连通分量分析，独立得到64条边，与 ROM 解码结果完全一致。
- 两组辅助字节不对称（25→26 为53，反向58；27→22 为44，反向54），原样保留为 `aux_index_raw`，尚不确定全部含义。
- 城池图块表：文件 `0x1DE69`，色板属性表 `0x1DE6F`。C++ 解码的6种图标叠加到隐藏精灵后采样的道路背景，完全匹配原版上方256×160区域。
- `fixtures/world-roads.json` 记录 OAM 坐标、64条边及像素连通分量；`world-roads-screen.png` 是独立原版截图。地图背景来自原版采样，并非已重写完整 PPU / 动态背景程序。

## 性格诊断

原版逐状态采样得到13题、26条是/否分支及6个结果。题号由 RAM `$0600` 对话编号定位，君主 ID 由结果状态 RAM `$06A3` 确认。分支图见 `fixtures/personality-quiz.json`，原生实现见 `native/src/opening.cpp`。静态画面已接入默认开场；难度效果、双人流程及动画未完成。

## 武将移动

自然样本：刘备（4）由新野（13）移至荆州（21）。原版 SRAM 改动为 `$61E4:04→FF`、`$6306:FF→04`、`$6D8C:01→02`、`$6D9D:0D→15`、`$6D9F:03→02`，其余字节不变。完整前后样本见 `fixtures/move-before.bin` / `move-after.bin`。

- `$A870–$A8F0` 校验归属、相邻与运行时道路限制表；`$A810–$A84F` 限制选择数量，出发城必须留将。
- `$A921–$A98D` 按来源槽位倒序移动，填入目标第一个空槽，原槽保留 `FF` 空洞。移动君主（ID<6）时更新该君主所在地。
- `$D5F7–$D60C` 增加行动计数并扣一命令书，存储的命令书数限制到15。
- 原生模块已移植这些步骤。自然移动样本只有一组，因此未声称所有原版失败/特殊道路分支均已采样验证。

## 三类开发的原生计算

CPU `$B521–$B5A1` 按智力分三档：小于40、40至69、70及以上。各档方案数5/6/7，方案起点0/5/11。方案台词与费用从文件 `0x1999C` / `0x199D2` 开始读取，每种开发各18项。

CPU `$E9E6` 从文件 `0x1EA02` 的256字节序列按8位游标取值并递增。生成方案时取低3位，超出该档方案数则重取，并非简单取模。

CPU `$B430–$B4D0` 执行：

```
收益单位 = floor(方案费用 / 2)
         + 智力奖励表[floor(智力 / 8)]
         + (帧计数器低字节 & 7)
```

智力奖励表位于文件 `0x1B4E1`：`0,1,2,3,4,5,5,6,7,8,9,9,10,10,10,10`。

- 黄金扣除方案费用，使用24位减法。
- 土地、商业增加收益单位，使用16位累加。
- 人口增加收益单位×100，使用24位累加。
- 统治增加 floor(收益单位/4)，原版先做8位累加，再限制到100。
- 消耗一命令书，行动计数递增；武将属性未改变。

`fixtures/development.json` 包含三次自然操作和三组明确修改初始 SRAM 的边界实验。随机值来自原版指令追踪时 `$B45A` 的 A 寄存器，没有从期望结果反推。每组都对照全部8192字节 SRAM。受控实验覆盖黄金跨高字节借位、土地/商业16位溢出、人口24位溢出和统治上限。

`OriginalState::develop` 显式接收方案和帧计数器低字节。原版 NMI `$FB06` 会递增帧计数器，但游戏还有其他写入点；整个帧调度、方案滚动 UI 和其他随机调用尚未移植。该方法的精确结果只在相同输入状态、方案和计数器值下成立。

## 可复现的分析工具

`tools/reference_trace.patch` 对已附加 `reference_hooks.c` 的 FCEUmm 源码增加可选指令/写入追踪。追踪默认关闭、最多32 MiB，新增的隐藏精灵接口只影响参考画面输出。补丁不属于 C++ 重制运行时。

追踪记录每条16字节，小端格式 `<IH10B`：PRG偏移、CPU地址、A/X/Y/S/P、三字节指令（写入记录则为地址低/高/值）、写入标志和保留字节。文件地址需要加16字节 iNES 头。调用 `retro_study_trace_start(limit, mode, lo, hi)` 开始，`retro_study_trace_stop(NULL)` 获取数量，再传缓冲区复制。

CPU 映射快照的 `$6000–$7FFF` 可能不能代表实际 SRAM。状态对照必须读取 libretro 的 `RETRO_MEMORY_SAVE_RAM`，即采样生成的 `.sram.bin` 文件。

## 新增：初始化、回合数据和默认原生战局

`initial-difficulty-0/1/2.bin` 是原版选择难度后、性格诊断前的完整 SRAM。菜单实际使用 SELECT 循环难度、START 确认。CPU `$ABBC–$ABE8` 设置200年1月，`$B12D–$B1EF` 复制30×36字节城池、2048字节武将及尾随数据、24字节君主状态，并初始化待到达和道路表。C++ 三个难度均逐字节一致。此时尚未执行开场 AI。

命令书由 CPU `$A0D1–$A11E` 统计领城数，再从文件 `0x19D5D / 0x19D7B / 0x19D99` 的难度表读取。刘备初始3城：简单/正常为3枚，困难为2枚。

`calendar.json` 的三例覆盖普通月、跨年及第二玩家参与。原版 CPU `$D373–$D437` 对非玩家城池调整统治，跨年时对玩家城池调整统治，并递增年份。测试比较完整 SRAM 和随机游标。

`monthly.json` 的三例覆盖待到达武将、道路时效、四月商业收入与十月土地收入，包含跨字节溢出输入。原版程序：

- `$AE68–$AEAF`：处理31个三字节待到达记录。失效分支对武将记录+2执行 `(value & 0xBF) | 0x80`，当前只按原始位操作移植，不猜测其最终显示语义。
- `$DC4F–$DC6C`：遍历248字节道路/尾随数据，减 `0x20`，高三位耗尽则变 `FF`。原版实际包含30×8表后的8字节，测试保留其初始0→E0的行为。
- `$DA4F–$DACB`：四月取商业、十月取土地；先计算16位 `field + floor(population/1024)`，乘3后累加到24位黄金。全部30城结算，保持原版整数溢出。

季节收入演出也调用随机序列；因此组合月结算样本只验证完整 SRAM，不宣称其演出结束时的随机游标已复现。详见 `fixtures/turn-state-evidence.json` 的输入条件、程序边界、核心与样本指纹。

默认开场已改接 `original_campaign.tscn` / `OriginalSession`，开发与移动实际改变该状态。战略 AI、双人开局、回合调度与征兵编成已接入；玩家战场、剩余命令、声音与结局尚未完成。此前关于“命令尚未接入”的历史阶段说明不再适用于新场景；旧 `main.tscn` 原型仍单独保留。

### 搜索证据

参见 `fixtures/search.json`（35组）与 `fixtures/search-evidence.json`。只把原版指令边界之间的 SRAM 与随机游标作为对应规则证据，派遣与候选重试的帧间演出随机调用未声称复现。默认原生战局已接入搜索派遣、返回报告与招募，完整回合仍未实现。

新增42组战略与军备 SRAM 对照见 `fixtures/README.md`、`fixtures/strategy-evidence.json`，总计93组。子程序样本通过不代表完整原版逐帧一致。

新增物资、人事、商店与侦查依据见 `fixtures/commands.json`、`fixtures/scout.json`、`fixtures/battlefields.json`、`fixtures/commands-evidence.json`。34组指令和30组侦查对照完整SRAM；30张战场地形的顶部区域逐像素对照。参考程序仅用于采样；游戏使用独立C++规则与图像解码。

### 单挑规则与返回证据（未发布开发）

新增 `fixtures/duel-strike.json`（768组）、`duel-ai.json`（968组）、`duel-finish.json`（32组）和 `duel-retreat.json`（2组），共1770条受控原版样本。分别核对8F86伤害边界、9325电脑选招、91BC/92F5结果确认，以及9233后的交锋场景恢复。结果样本比较完整SRAM、兵队、俘虏和随机游标；后退样本比较完整SRAM、兵队、军令和保留的交锋执行字段。

原版研究现场先由合法C++动作到达的单挑入口数据置入模拟器，随后执行原版指令；伤害和电脑样本另外控制体力、武力、随机游标或帧字节。这些不是无注入的自然通关。攻击极值包含正常游戏范围之外的算术边界。动画等待、骑马画面、音效、帧间随机推进及完整通关一致性尚未验证；不能用子程序样本数量替代这些要求。

`tests/clash-duel-0.json`、`clash-duel-3.json`、`clash-duel-4.json` 是可由原生存档严格重放的PC集成场景，与原版采样文件分开。当前验证及源码/样本指纹见 `../docs/duel-validation.json`；游戏运行核心没有嵌入研究用模拟器。

电脑左军暂存数据的确定性恢复复用 `fixtures/clash-strategy.json` 的1758条原版受控样本，没有新增自然整局证据。新C++分析器覆盖所有可能的三字节尾部，只在军令结果及随机游标全部一致时继续；独立的3072次C++域枚举验证该缩减，包括同军令而不同随机游标的歧义。测试样本中505条旧暂停可恢复，109条保留暂停。`tests/clash-strategy-boundary.json` 是通过合法动作严格重放的PC会话存档（不是模拟器状态），可继续4次军令决策、277次操作抵达撤离。完整暂存生命周期及完整战场重制仍未完成，详见 `docs/strategy-resume-validation.json`。


战术轮换与守方指令新增 `fixtures/tactical-turns.json` 405条及 `fixtures/defender-commands.json` 837条受控原版样本。前者核对主动结束的保留机动力、换边、轮数、状态递减与结果优先级；后者有对应完整SRAM前后文件，核对12格守军的移动、阵容、侦察和反击。全部均为设置原版临时状态后的控制实验；PC会话存档 `tests/tactical-turn-start.json`、`tactical-turn-single.json` 和 `tactical-defender-clash.json` 通过严格动作重放验证，不能当作原版自然整局证据。源码及验证见 `docs/tactical-turn-validation.json`。


守军战术撤退新增 `fixtures/defender-retreat.json` 306组受控原版D4D3–D5C2函数样本及完整SRAM前后文件。入口物理偏移154E3，临时状态设置为战术阶段13、子阶段8、守方行动；随机游标从真实函数入口捕获，不能用设置输入帧前的游标代替。对照覆盖30城选择、12格守军、满员/无城、君主、电脑离散忠诚和D558标志字节查找细节。界面与保存只接通玩家守方撤退；电脑战术AI、最终战果和逐帧演出仍未完成。证据与限制见 `docs/defender-retreat-validation.json`。


守方撤离战果新增 `fixtures/withdrawal-result.json` 432组阶段13、子阶段9..12（DC78/DBD8/DC05/DB20）受控原版函数样本，含完整SRAM、24格俘虏和入口/出口随机游标。`fixtures/withdrawal-flow.json`另记录受控入口后连续65帧至战略阶段3的8个原版实际步骤，核对跨步骤SRAM及最后D8F4清理；步骤间的随机游标推进来自原版演出，未伪称PC逐帧一致。两侧俘虏的集成测试由已有合法交锋投降形成，非直接伪造俘虏动作。该结果仅对应守方撤离原因02；其他胜败、君主/限时、AI和整局验收仍待继续。详情见 `docs/withdrawal-result-validation.json`。


进攻军撤退与守军全灭新增 `fixtures/remaining-results.json` 768组受控原版样本，阶段13子阶段14..16、阶段14子阶段24..28分别执行原版DCDB/DDBE/DDF3和DC47/DC78/DC05/DBD2/DB20。`attacker-result-flow.json`（49帧、7步骤）和`defender-defeat-flow.json`（65帧、9步骤）从受控入口跟随原版调度到战略阶段3，核对连续完整SRAM/俘虏。`tests/tactical-last-defender-clash.json`为11条合法动作生成的单守军交锋测试，初始驻城名册是控制实验，不能称为原版自然整局。限制与验证见 `docs/remaining-results-validation.json`。


进攻主将离场新增 `fixtures/commander-result.json` 516组受控原版阶段14子阶段0..3样本，核对DAFE/DBA2/DC40/DCDB的完整SRAM、双方俘虏及随机游标。`commander-result-flow.json`从受控入口连续65帧到战略阶段3，记录8个原版调度步骤，含最终D8F4清理。两组俘虏都安置到守城，满员直接离散，之后余军返回各自来源城；不改变城池归属或资源。PC集成夹具 `tactical-commander-clash.json` / `tactical-last-commander-clash.json` 从受控布阵执行真实动作生成，不能当作原版自然通关。验证及限制见 `docs/commander-result-validation.json`。


限时报告新增 `fixtures/time-limit.json` 288组受控原版阶段15子阶段0..2样本，核对DD02/DD8A/DD98的报告人、势力、阶段/原因、完整SRAM及无随机调用。DD50读取武将偏移3（武力）选择最高者，同值后格优先，包含守方第12格。`time-limit-flow.json`从受控限时入口连续113帧至战略阶段3，记录10个有效步骤，含阶段13/14..16的强制撤军和最终D8F4清理。等待被折叠、随机采用实测入口，不能称为PC逐帧同步或自然通关。PC以实际轮换及投降动作验证无俘虏与双方俘虏的完整限时路径，详见 `docs/time-limit-validation.json`。


电脑君主败北领地接收新增 `fixtures/ruler-annexation.json` 430组受控原版DA30样本，核对完整SRAM、逐城游标及随机调用。`ruler-defeat-flow.json`（原因83、39步骤）与 `ruler-attacker-flow.json`（原因94、38步骤）均从受控入口连续129帧运行至战略阶段3，包含军团/俘虏处理、30城接收及D8F4清理。PC集成夹具 `tests/clash-npc-ruler-defeat.json` 是323条合法动作重放的电脑守城君主败北入口，初始军团和体力属于控制条件，不是原版自然通关。电脑先攻与玩家君主失败仍未接通，逐帧演出随机亦未同步。验证见 `docs/ruler-result-validation.json`。


玩家失败报告新增 `fixtures/human-failure.json` 的1008组AFA8原版函数样本（每组完整SRAM前后与入口/出口随机游标），覆盖六位君主、单/双人、另一位玩家是否败北、两端控制器A/B。`human-failure-bridge.json` 从受控原版全局16失败画面连续57帧进入战术结果14/0，核对地图恢复的完整SRAM；JSON的`phase`是全局12，`tactical_phase`才是14，原因字段读取原版RAM $06B7（148），不能误读$06C3。单人/最后玩家死亡停留，不接收领地；还有玩家存活才确认继续。PC纯C++实现，不在运行时嵌入模拟器。操作夹具与限制见 `docs/human-failure-validation.json`。


`fixtures/tactical-ai-assessment.json` 新增302组A1D9前段原版样本：捕获出口A21A/A283（撤军）、A2D6（角色调整返回）或A2D7（开始部队规划），不是整段AI函数的结果。逐组比较SRAM全量、撤军槽位、实际评估时的战力分值及随机游标。`computer-retreat-flow.json` 连续运行36帧，从评估入口到D4D3返回，比较组合规则的全量SRAM；使用实测评估与撤退入口随机游标。受控低体力/兵力场景，不是自然整局录像。见 `docs/tactical-ai-validation.json`。


`fixtures/computer-castle.json`：678组原版A2D7普通部队扫描至A3C4补位返回/A3C5角色入口的受控样本，完整SRAM及入口/出口随机游标对照；含30城堡位置、稀疏名册与首尾游标、状态/地形/占用和兵力/机动力门槛。`computer-fort-flow.json`：原版连续36帧由电脑规划运行至实际移动函数返回，比较守军槽3从44到54、23减至19的机动力和全部SRAM。原版帧间随机仍不同步；这不是全部AI或自然整局对照。见 `docs/computer-fort-validation.json`。

`fixtures/computer-strategy.json`：784组A3E8守军角色入口至B630/B7F9计策筛选出口的原版受控样本，逐组核对完整SRAM、选择命令、参数和随机游标。覆盖八种计策结果、角色/智力/机动力门槛、目标地形/兵力/体力/忠诚/状态、多个敌军和ROM全部48搜索偏移（含合法边界）。捕获脚本在工作区 `work/original-battle/capture_computer_strategy.py`。这些只证明选择规则，不证明计策效果、整段AI或帧驱动随机同步；范围见 `docs/computer-strategy-validation.json`。

`fixtures/tactical-strategy-effect.json`：1,312组原版CE1E至CE7F成功返回/CDF1失败入口样本，完整SRAM、状态和随机游标对照，覆盖八种计策及双方、伤害与属性门槛、距离和随机回绕、转属空位与君主限制。`strategy-execution-flow.json` 连续100帧经过计策选择、提示确认、扣费和失败判定；before样本位于效果入口，额外charged.bin位于扣费入口，二者相差原版重绘施计部队闪烁格的一字节占用。A键用于确认提示，效果阶段使用实测随机游标/帧字节；不是逐帧PC同步或自然整局验证。见 `docs/strategy-effect-validation.json`。

## 电脑移动和主动攻击依据（本轮）

`fixtures/computer-motion.json` 的604组受控原版运行截取角色计策返回后的 A411/A7B1 至命令发出或后续扫描/绕行入口，保存完整前后 SRAM、状态字节及随机游标。覆盖30城堡位置、边界格、阻挡、状态效果、地形、耗费、兵力和武力分值。102组移动、302组攻击、112组扫描边界、88组绕行边界；未把停在尚未移植分支的样本计作完整AI动作。

`fixtures/computer-motion-flow.json` 记录连续36帧移动和81帧交战准备。移动逐字节匹配；交战准备有明确的一字节发起方闪烁占位差异，PC保留规范战术占位，其他字节及全部交战字段、3点机动力消耗匹配。原版D6D5先检查发起方是否电脑，电脑主动攻击改变发起方阵形。连续证据为受控战场运行，不是自然开局通关；演出/逐帧随机时序尚未还原。采集脚本和文件指纹见 `../docs/computer-motion-validation.json`。

## 守军绕行与续行依据（本轮）

- `fixtures/computer-flank.json`：744组 A8D6 / AAA4 入口至移动、清除/扫描或 A949 补充计策入口，136移动、384扫描、224补充计策；比较全部SRAM、缓存和负A0标记。
- `fixtures/computer-reuse.json`：180组 A0=FE 时 A2D7 到 A3C4/A3C5，包括24次补位，验证同队选择、角色处理、无随机消耗和全部SRAM。
- `fixtures/computer-retry.json`：392组 A949 直接调用 B61D，184次选中计策，208次未选中；核对命令、目标、全部SRAM与精确随机游标。
- `fixtures/computer-flank-flow.json`：96帧连续运行，城堡上方部队先左移再下移，两次同队续行，抵达后清除目标；机动力24→18→16→16，三个阶段完整SRAM连续一致。源军团来自受控PC场景，原版施计智力设为0以观察不中断的绕行；不声称全游戏逐帧一致。

采集脚本、原始状态、PC场景、验证日志和文件指纹见 `../docs/computer-flank-validation.json`。模拟器仅用于读取原版行为；重制运行核心仍是C++。


## 无命令轮询依据（最新未发布进度）

- `fixtures/computer-scan.json`：368轮受控原版执行，截取1,658个 A2DF–A330 片段。覆盖守方12槽倒序、攻方11槽正序、全空/全满/单双稀疏槽及同队续行。逐条核对起点、选中槽、命令4、完整SRAM及不变随机游标。
- `fixtures/computer-scan-flow.json`：既有受控合法部署场景连续43帧运行，部队3→2→1→0→空起点11，发出命令4。实际写入记录显示 DEB2 清零机动力，未经过玩家 C6EC 携带点数计算；交接后进攻方30点、回合1、携带点数0。完整SRAM和交接字段对上；命令发出时随机游标为3，演出后为179，后者未声称复现。

采集脚本、原始状态、文件指纹与验证结果见 `../docs/computer-scan-validation.json`。两类参考实验均不代表自然开局通关；攻方轮询只是内核覆盖，尚未接入电脑先攻玩法。


## 角色1/3附近搜索及耗尽攻击依据（最新未发布）

- `fixtures/computer-nearby.json`：1,515条受控原版 A57C/ACE7 后续决策，包含角色1/3及别名、近邻与外围目标、同分、阻挡、状态、地形费用和失败回退。147条专门验证 A700 超出24项搜索表后的4字节评分读取。全部SRAM、命令/方向、不变决策随机游标对照。
- `fixtures/computer-nearby-flow.json`：四段36/81/36/81帧的原版连续执行，覆盖两类角色的移动和主动交战。攻击比较仅明确归一化发起方占位闪烁字节，其他完整状态直接对照。
- `fixtures/computer-rejected-attack.json`：五段各46帧运行。角色1/3剩余1或2点时发出攻击后，经DF27直接结束行动；第五段从PC受控军团的384条合法行动后的停点开始。该样本交接前显示预算50，PC按既有C010规则限制可用预算40。无交战、无SRAM变更；未匹配演出后的随机调用。

采集脚本、存档与文件指纹见 `../docs/computer-nearby-validation.json`。这些是局部行为与会话续接验证，不代表完整游戏已完成。


### 敌军占堡应对（AAD7–ACDE）

- `fixtures/computer-occupied-fort.json`：1,200条受控原版执行片段；30城、角色2及低位别名、已有/未初始化方向缓存、棋盘边界、机动力、体力、强度、状态、阻挡和地形。比较8192字节 SRAM、随机游标、命令参数及继承临时字节 $21。140移动、280攻击、231计策、549轮询。
- `fixtures/computer-occupied-fort-flow.json`：来自合法 PC 操作历史的战场边界，原版连续36帧移动和81帧攻击进入交战。决策随机游标精确比较，演出后的随机游标只记录，不宣称已经重现。攻击只将发起方占位闪烁恢复为其兵队标记；其余数据原样比较。
- `../tests/computer-occupied-fort-{move,attack,strategy}.json`：受控城池/武将与合法布阵初态；之后通过真实会话操作完成玩家交战、占堡、回合交接、电脑评估，不直接伪造最终战术状态。适用于纯 C++ 和 Godot 入口、结算、存档回放测试，不代表自然通关样本。

采集脚本、前置模拟器状态和文件指纹见 `../docs/computer-occupied-fort-validation.json`。临时状态仍有未覆盖来源，完整游戏与逐帧演出仍未完成。


### 跨决策临时字节（$21）

- `fixtures/computer-scratch-motion.json`、`computer-scratch-nearby.json`、`computer-scratch-strategy.json`：分别604、1,515、784条原版片段，在原有采集流程中补录 $21 入口/出口值。引用原有 SRAM 文件；采集器重新执行并断言全部字节不变，未覆写历史样本。指针输出也须保持所有旧 API 返回值不变。
- `fixtures/computer-scratch-flow.json`：两段各36帧的连续原版执行，包含战力统计、无动作、跨空槽选队和实际移动，逐阶段记录 SRAM、临时字节及决策随机游标；不对 SRAM 作归一化。演出结束后的随机变化只记录，不宣称已重现。
- `../tests/computer-scratch-resume-{move,attack}.json`：从既有受控合法布阵继续进行251、187条真实会话操作，重现只剩2点机动力时的旧停点。标签表示起始历史来源；两份恢复后的实际动作都是移动。测试包含旧停点重载、继承字节恢复、实际移动、玩家回合交接、篡改拒绝及新载入状态不能沿用旧值。

文件、脚本和检查指纹见 `../docs/computer-scratch-validation.json`。样本不等于完整自然通关，未知的命令/演出临时状态仍有保留边界。

### 角色调整后的残留指令（未发布）

`fixtures/computer-role.json` 包含 384 组受控原版执行记录，每组记录评估、角色写入、AI 就绪、移动入口和返回时的 SRAM 与 RAM 摘要，覆盖 256 个参数字节及低机动力、状态、空槽组合。`computer-role-flow.json`、`computer-role-flow-blocked.json` 各记录一条 129 帧连续原版撤军及角色返回流程，并验证残留参数没有被改写。研究只修改测试状态与输入，不修改 ROM/CPU 指令。

`tests/computer-role-start.json`、`computer-role-blocked.json`、`computer-role-unknown.json` 为受控初始战局经正式 C++ 操作 API 生成的历史；最终停点没有手工改写。状态效果等待和空槽仍受保护，演出随机调度及完整自然对局不在本轮完成声明内。


### 玩家战场施计依据（最新未发布）

- `fixtures/player-strategy-menu.json`：452条原版菜单确认记录，覆盖智力0–255和可进入的第二页；在CBA4入口到返回捕获分组编号，完整SRAM不变。菜单分组、费用、地形直接取指定ROM。
- `fixtures/player-strategy-gate.json`：双方、八计、七种地形共112条CD11验证记录。通过电脑分派进入玩家也使用的同一地形函数；不是112次自然玩家菜单操作。前后SRAM和随机游标不变。
- `fixtures/player-strategy-execution.json`：双方八计共16条从受控已选方案开始的原版确认流程，记录CD76扣费入口、CE1E效果入口和成功/失败出口。扣费入口到效果入口的SRAM完全相同；效果比较全部8192字节、状态及随机游标，不做闪烁归一化。

执行样本使用实测效果入口帧字节和随机游标，不代表整个演出时序已移植。PC界面使用现代简体计策说明，原版菜单布局、字形和动画仍待还原；未验证自然完整对局。采集脚本、状态、全部样本指纹和测试结果见 `../docs/player-strategy-validation.json`。


### 电脑进攻方依据（最新未发布）

`fixtures/attacker-ai.json` 包含858条受控原版决策链，分段保存选队入口、策略入口/返回和后续移动判断/返回。覆盖回合0、6–10、智力/费用边界、全部11个进攻槽、首次同队与正序轮询、30城地形、状态、边界、守军君主及忠诚/战力替代目标。结果为102次施计、120次移动、436次攻击、200次无动作轮询。每段比较完整SRAM和随机游标；指令/方向/施计目标直接匹配，不替换原版CPU代码。

`fixtures/attacker-opening.json` 记录从已有 `work/original-battle/defense/complete.state` 恢复后、不再写入状态、不发送按键的192帧连续运行。前四队先后移动，费用24→21→18→12→6；完整SRAM不归一化。规则测试使用每次策略入口实测随机游标；画面等待造成的游标推进未声称已经原生重现。此开场状态源于既有受控AI入侵实验，不是自然新开局至通关录像。

`tests/attacker-invasion-start.json`、`attacker-invasion-far.json`、`attacker-invasion-strategy.json` 为入侵边界经正式部署操作生成的布阵完成存档；后两份控制初始智力及布阵/随机条件。它们用于验证原生与界面推进、交接及存档，不替代原版证据。捕获脚本、来源、验证与指纹见 `../docs/attacker-ai-validation.json`。


### 来袭撤军与战略返回采样

`fixtures/invading-retreat.json`：32条受控电脑进攻撤退（槽0/1/5/10 × 普通/主将 × 原城空/满 × 1/20点机动力）。每条有评估、决定、扣费入口、D354前后及结果报告的6份8192字节SRAM。原版电脑单位标记为0x30|槽号，不带主将位；所有样本走单人撤回分支。9B选队游标保持7。报告确认和后续结算不在该段采样范围内；随机比较采用实测逻辑入口游标，记录但不复现演出阶段随机消耗。

`fixtures/invasion-return.json`：原因2、20、36各一条62帧原版战后清场至下一君主记录。受控起点使用已有结算内核生成的战后SRAM，原版CPU随后执行D8F4和A086；对比完整SRAM，并记录命令书及君主字段的原版写入点。不是自然完整战役。

三个 `tests/invasion-retreat-*.json` 由正式PC操作累计92条事件生成第4回合停点。起点属性和满员名册受控；最终战术结果经严格回放重建。生成程序、参考状态及文件SHA-256见 `docs/invasion-result-validation.json`。研究模拟器不进入产品运行内核。


### 来袭限时与败北连续结果

`invasion-defeat-flow.json` 含普通守将、单人君主、双人中的主玩家君主、来袭电脑君主各两段原版败北确认，共8段。起点来自合法PC命中事件，导入一次SRAM和交锋上下文后运行原版函数；比较宣布与确认的完整SRAM和原版去向。

`invasion-limit-flow.json` 为129帧/11步，`invasion-settlement-flows.json` 包含守军全灭65帧/10步、双人续局145帧/40步、来袭电脑君主败北145帧/35步。原版分别执行公告、撤军或入城、俘虏处理、资源结算、30城领地接收和战场清理。结算片段从PC已达成的受控边界开始，至原版战略阶段3结束；没有宣称从原版自然开局或完整原版交锋采得。随机对照使用各逻辑函数入口游标。

五份新PC存档分别有607、172、172、172、244条正式操作；测试加载时逐条重放。原版输入状态、采样脚本、PC生成器及文件指纹见 `docs/invasion-endings-validation.json`。研究模拟器不进入游戏内核，现有发布包未更新。


### 左军尾部RAM来源与恢复（本次）

`fixtures/clash-tail-lifetime.json` 保存原版E036的三次启动清零记录，以及534个留存场景的SHA256、原值和标记值追踪结果。每个场景将04F2–04F4设为213/106/177，运行32帧无输入、8帧A、24帧无输入；追踪所有RAM写入及其镜像，无CPU代码修改，534段均无写入。该证据支持已覆盖路径的正常启动模型，不能当作所有ROM控制流均已证明。

`fixtures/clash-reset-strategy.json` 是155次独立原版B920调用，均保留参考状态原有的三字节零值。前154条复用旧样本的受控输入；最后一条从真实PC暂停存档导入SRAM和兵队，记录原版逻辑入口/返回军令及随机游标。`tests/clash-tail-boundary.json` 经32条实际操作生成，在旧全值一致性分析下有歧义；新的独立恢复事件可继续112步并完成现有败北、战后结算与战略返回。没有新增自然整局、完整逐帧或窗口视觉验收。文件指纹、脚本及验证结果见 `docs/clash-tail-validation.json`。


### 首队离场后的空槽位命令

`fixtures/computer-empty-role.json` 新增316条原版受控记录，逐条保留评估、角色调整、就绪、移动入口与返回/等待的8192字节SRAM。覆盖四方向、1/8/24/40机动力、状态限制、6C36值、地图字节与非标准空槽位置。原版移动37条仅改变6DAB，135条受阻，144条等待；没有画出或复活不存在的军队。这些是函数/调度器边界实验，不是316场自然战斗。

`fixtures/empty-role-flow-0.json` 至 `-3.json` 分别从PC实际最后一次评估前导入状态，控制四种残留方向，各运行36帧至原版返回评估。每段5个记录点，未改写方向；逐段比较完整SRAM及移动入口/出口的RNG。PC存档 `tests/computer-empty-role.json` 由受控初始名单、忠诚/体力/兵力参数经过两次实际撤军和角色调整等6条正式操作生成；方向未知时四种原版结果均为受阻，因此可确定性续行。脚本与指纹见 `docs/empty-role-validation.json`。
