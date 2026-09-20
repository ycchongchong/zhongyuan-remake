# C++ 核心开发

默认开场现在进入 `original_campaign.tscn`，使用 `OriginalSession` → `OriginalState`。`main.tscn` / `Campaign` 是保留的旧玩法原型。二者存档分开；原版战局已接入战略 AI、双人轮换和月结算，玩家战场仍未完成。

## 从哪里修改

| 文件 | 职责 |
| --- | --- |
| `include/zhongyuan/campaign.hpp` | City 数据结构、Campaign API、只读状态访问 |
| `src/campaign.cpp` | 内政、命令限制、战斗、AI、月结算、胜负、JSON 存档校验 |
| `godot/bridge.hpp`、`godot/bridge.cpp` | 把 C++ 类注册为 Godot 的 ZhongyuanGame；字典转换和文件 I/O |
| `godot/register_types.cpp` | GDExtension 动态库入口 |
| `tests/campaign_test.cpp` | 独立 C++ 回归测试，不启动 Godot |
| `../scripts/main.gd` | 界面和地图显示、接收玩家操作 |

旧原型调用流程：`Godot UI → ZhongyuanGame 原生类 → Campaign C++ 核心 → 状态快照 → UI 刷新`。

Campaign 不依赖 Godot，可以单独运行和测试。所有可变数据由它持有，Godot 接收到的是数据副本。玩家操作通过 `act()`、`end_turn()` 发出；禁止用修改 `game.cities` 的方式修改真实状态。测试、存档恢复或开发工具需要修改状态时，编辑 `snapshot()` 的副本，再调用 `restore_snapshot()`，完整通过校验后才提交。

`scripts/game.gd` 只继承 `ZhongyuanGame`，没有规则实现，也没有 GDScript 回退核心。动态库加载失败时应修复编译/路径问题。

## Linux 编译与运行

依赖：CMake ≥ 3.22、C++17 编译器、Python 3、Make 或 Ninja。第三方源码已随项目提供，正常构建不联网。当前使用 GCC 13、Godot 4.5.2 验证。

在**项目根目录**执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash start-linux.sh
```

首次运行仅在独立的 `--import --recovery-mode` 进程中导入资源，随后正常启动并加载 C++ 核心。这避开了 Godot 4.5 首次无界面导入时的[原生扩展文档生成崩溃](https://github.com/godotengine/godot/issues/111645)，已用干净解压包验证。

输出：`bin/zhongyuan_core.so`。修改 C++ 后重新编译并重启游戏；当前禁用动态库热重载，避免运行中的对象引用旧代码。

VS Code：`Ctrl+Shift+B` 编译；“终端 → 运行任务”选择 `C++: test` 或 `Game: run Linux`。已提供 `compile_commands.json` 配置供 C/C++ 插件读取。安装 C/C++ 调试扩展并具备 GDB 后，可在“运行和调试”选择核心测试或游戏调试。没有自动安装 VS Code 扩展。

## Windows 构建入口（未实机验证）

安装 Visual Studio 2022 的 C++ 桌面开发工具、CMake 和 Python 3。在开发者终端中执行：

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
cmake --build build-windows --config Debug --parallel 4
ctest --test-dir build-windows -C Debug --output-on-failure
```

然后用 Godot 4.5.2 标准版导入 `project.godot`。预期输出 `bin/zhongyuan_core.dll`；当前交付只验证 Linux x86_64，不含 Windows/macOS 二进制。

## 只编译核心

```bash
cmake -S . -B build-core -DZHONGYUAN_BUILD_EXTENSION=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

该配置不编译 godot-cpp，适合快速测试玩法。C++ 测试的判断不依赖 `assert`，Release 构建同样执行。

## 迁移验证与存档

- `tests/legacy_traces.json` 是迁移前 GDScript 核心实际生成的 12 个场景、720 步状态和文本记录，覆盖内政、调兵、胜败、AI、缺粮及无效命令。
- 独立 C++ 测试使用 `tests/legacy_scenario.json` 重放旧记录并对比；30城 Godot 测试另行验证新剧本和扩展绑定。文件是固定回归数据，不能为使测试通过而用新实现重新生成。
- `tests/smoke.gd` 和 `tests/ui.gd` 保留原有检查，并增加原生核心加载断言。
- 旧原型使用 `zhongyuan-original-map-v2` 剧本标识和版本2存档 `campaign-original-map.json`，校验剧本标识、30城数量、拓扑和不可变的原始数据字段；旧三城版本1仅用于旧剧本回归，不与主游戏互读。
- 原生存档先写同目录临时文件，再重命名替换。加载损坏文件、错误版本或不合法字段不会改变当前战局。
- 原型战斗公式保持原样；迁移 C++ 不代表已经还原 NES 原版算法。

30城扩展验证：独立 C++ 旧版720步回归、30城核心测试、全部30城与指定 ROM 的数据对照、Godot 规则/绑定/界面检查通过；已用 Mesa 启动并检查30城界面。Windows/macOS 和 VS Code 插件调试会话尚未验证。

## 依赖版本与来源

- `vendor/godot-cpp`：官方 `godot-4.5-stable` 标签，与 Godot 4.5.x 配套；MIT，见目录内 LICENSE.md。
  https://github.com/godotengine/godot-cpp/tree/godot-4.5-stable
- `vendor/nlohmann/json.hpp`：nlohmann/json v3.12.0 单头文件；MIT，见 LICENSE.MIT。
  https://github.com/nlohmann/json/releases/tag/v3.12.0
- Godot 原生扩展文档：
  https://docs.godotengine.org/en/4.5/tutorials/scripting/cpp/gdextension_cpp_example.html

godot-cpp 的生成代码由 CMake 根据 `build_profile.json` 在 build 目录创建，无需手工修改。

## 原版模块与主战局的边界

- `src/opening.cpp`：原版开场状态机；13题诊断的26条分支均来自原版采样。已通过 `ZhongyuanOpening` 接入默认开场场景。
- `src/original_rom.cpp`：只读导入30城/241武将、图块、坐标、64条道路、开发方案与随机表。严格检查指定 ROM 的头、大小和 CRC32。
- `src/original_state.cpp`：独立8192字节原版状态，已实现 `move_officers`、`development_options`、`next_development_offer`、`develop`。不执行6502代码，不依赖 Godot。
- `OriginalState::develop` 显式接收方案索引和原版帧计数器的低字节；方案滚动和完整帧调度仍未还原。移动与开发已接入新默认场景的 `OriginalSession`，按钮通过本模块修改真实记录。旧 `Campaign` 原型继续单独保留。
- `reference/fixtures/development.json`：三类自然开发操作、三组受控 SRAM 边界实验。测试对照完整 SRAM，不只比较显示数值。
- `reference/fixtures/officer-move.json`：刘备由新野移往荆州的完整 SRAM 差异依据。
- `tests/original_world.gd`：通过原版截屏独立核对40,960像素；`tests/opening.gd` 检查原生诊断、六势力开局和状态保存字段。

默认场景已切换为原版状态，支持命令书结束后的势力轮换和跨月；玩家战斗仍会暂停推进。旧原型可单独启动用于回归检查。详细未完成项以 `docs/original-remake.md` 为准。

## 原版状态与会话

- `OriginalRom::initial_sram` 按原版初始化程序生成8192字节状态，三个难度均有独立原版样本。`command_books` 按难度和领城数读取原版表。
- `OriginalState::city` / `officer` / `snapshot` 从当前 SRAM 解码字段；原版名字、地图、德等不可变数据来自已校验 ROM。
- `OriginalState::advance_calendar` 已验证日期、跨年统治变化与随机游标；`settle_month` 进一步处理待到达武将、道路有效期及季节收入。现在由原生回合控制器调用；收入演出的额外随机调用未复现。
- `OriginalSession` 管理真实状态、待确认开发、取消、移动、16位帧计数器和8位随机游标。新存档格式为 `native-original-v2`（兼容读取v1），文件为 `campaign-native-original.json`；不与旧原型互读。
- `ZhongyuanOriginalData` 除资产读取外，提供原版会话的开始、快照、命令与保存恢复接口。加载坏文件时先验证候选状态，成功后才替换。
- 新场景显示真实黄金/土地/商业/人口/统治与241名武将，已接入开发、移动、名册及键盘/手柄按键映射。

`tests/original_campaign.gd` 检查整条 Godot → C++ → SRAM → UI 流程，含难度命令书、开发确认、真实将领移动、君主所在地、全名册、保存恢复及坏存档保护。`native/tests/original_rom_test.cpp` 对照初始化、命令与月结算的完整原版样本。

### 搜索模块

`OriginalState::dispatch_search / collect_search / search_kind / search_find / recruit_search` 原生实现搜索状态规则。`OriginalSession::search / visit_city / finish_search` 提供派遣、报告、招募和防止重复结算，保存包含待处理报告。对应 Godot 按钮与报告已接入，35组独立指令边界样本验证字节状态。正常回合已支持搜索自然返回；候选重试的演出随机调用未还原。

## 战略回合与征兵

- `OriginalState::ai_phase`：原生战略处理；`begin_ai` 提供运行阶段，`finish_ai` 完成非玩家战后占城与命令书清零。24组完整字节/游标/预算样本验证。
- `next_ruler`：按原版顺序跳过死者，回到首位时结算月份。6组独立原版样本验证。
- `OriginalSession::advance/end_turn`：推进电脑阶段和结束玩家回合，保留玩家受袭的战斗 ledger；`start` 可指定第二位玩家。
- `recruit_reserves/assign_troops`：征募与编成，由 Session 的 `pending_army` 约束流程。12组原版字节样本验证。
- `native-original-v2`：兼容读取 v1，增加 phase/ai/battle/ending/events/pending_army；任意恢复验证失败均保留当前完整状态。
- `original_turn_test`：18种开场/跨月、自然搜索、双人顺序、战斗存档与拒绝非法阶段。

战略规则与原版子程序样本一致不等于原版全局逐帧一致。原版 UI/NMI 会修改随机游标和 scratch RAM；目前仅显式保存已移植阶段所需上下文。玩家战场、出征、战后处理及完整演出尚未实现。

## 物资指令与侦查

- `OriginalState::execute_command` / `OriginalSession::execute_command`：运输、赠送、赏赐武将黄金/武器、赏赐百姓、购买、出售、学习、治疗、侦查。规则与数值修改均在C++，Godot只提交已选参数和显示结果。
- `OriginalRom::command_tables` 保留静态赏赐索引、礼物结果与每城商店表；`battlefield_pixels` 解码顶部地形CHR bank和城堡图块。
- `original_commands_test`：34组物资指令、30组侦查的完整8192字节、对应随机游标，以及30张地形截图的1,228,800像素；另测非法参数原子拒绝、会话阶段限制和存档。
- `tests/original_campaign.gd` 增加购买、运输、物资存档、本城/远程侦查与C++图像接口验证。

## 玩家出征

`OriginalState::expedition_quote/dispatch_expedition` 与对应 Session/Godot 接口负责不可变报价和原版出征提交。`original_expedition_test` 比较5组完整SRAM，检查失败原子性、外交限制及出征存档。新增 `expedition` 阶段仍使用v2保存格式，并与AI受袭的 `battle` 阶段分开验证。后续已接入布阵、首轮战术操作及部分交战流程，当前边界见下文。


## 交战与俘虏进度（开发中）

`OriginalSession::begin_clash/cycle_clash_order` 管理初始兵队、地图和玩家军令。`request_clash_surrender/answer_clash_surrender/advance_clash_surrender/finish_clash_result` 接入投降确认、对话、结算和返回战术行动。底层规则复用 `OriginalState::clash_surrender/restore_tactical_board`。Godot只提交命令和显示结果。

投降相关UI阶段依次为 `clash_orders → surrender_confirm → surrender_notice → surrender_accepted → clash_result`，取消只允许发生在确认阶段。`battle.tactics.captives` 保存24个原版俘虏槽位；事件记录用于v2存档验证和重放。提交前预留后续确认与返回所需的记录空间。主将离场、全军结果和最终俘虏处置尚未接入，相关边界保留可恢复状态，不能通过结束战斗丢弃俘虏。

这不是完整自动交战调度器。最新源码验证和未完成项见 `../docs/duel-validation.json` 及 `../docs/original-remake.md`。


## 交锋步骤执行（开发中）

`OriginalState::clash_step(runtime, context, cursor)` 原子推进一个逻辑边界；`OriginalSession::advance_clash` 负责从配置进入执行、保存和投降/待实现入口。`clash_running` 内保持原版阶段与投射物字段，`advance_clash` 事件逐条参与v2重放。界面计时器只调用C++，不计算交战规则；这不是原版帧调度器。

`clash_strategy` 的 `tail` 可缺省，但实际分支需要04F2–04F4时会返回 `ai_scratch`，不会填零推测。旧会话操作对缺失暂存数据保留 `clash_boundary`；新增 `recover_clash_strategy` 用独立事件恢复已采样的正常启动内存，详见文末；单挑、撤离和直接武将败北由各自专用接口继续。动作间隙（内部4/1）可改玩家军令；电脑投降进入现有公告/接纳/结算；人类中途取消投降恢复原决策状态。

新增896组原版执行实验、133个自然运行边界和32张原版静止朝向图。`tests/clash-session-start.json` 是经合法C++动作重放验证的集成场景，和 `reference/fixtures/clash-step*.json` 的原版CPU捕获分开标记。最新验证及边界说明见 `../docs/duel-validation.json`。


## 交锋撤离会话（开发中）

`OriginalSession::advance_clash_retreat` 从旧的 `clash_boundary/retreat` 进入 `clash_retreat`，复用追击、散兵和确认内核，按原版内部阶段5的子计数器处理到 `clash_result`。v2重放新增同名事件和随机游标；不修改旧 `advance_clash` 事件的语义。Godot只负责确认与呈现，初始淡出/文字等待被折叠，尚无原版帧调度。

结果增加 `kind: retreat`、`map_restored`、`defeated`、`losses` 与 `can_continue`；原版阶段11重建战术地图，阶段15保留交锋图。普通结果可调用已有 `finish_clash_result` 返回战术行动；主将/君主及最终战场结果保留待续。这里的内部5/计数器7不能与尚未接入的内部7直接败北混淆。旧投降结果缺省 `map_restored` 仍按true处理，保持兼容。

新增3个自然原版边界及最终返回SRAM；7种C++受控集成场景和4份Godot场景分开记录。参考 `../docs/duel-validation.json`。完整系统和发布条件仍未满足。


## 直接武将败北会话（开发中）

`OriginalSession::advance_clash_defeat` 延续 `clash_boundary/general_defeat`：第一次从内部7/0折叠淡出和文字准备，调用 `clash_general_defeat` 并停在7/2的 `clash_defeat`；第二次调用 `finish_clash_defeat`，进入 `clash_result`。活动兵队属于胜方；两步均不抽取随机数，事件无需额外随机字段，完整v2重放仍校验结果。

结果使用 `kind: defeat`、`officer`、`defeated`、`map_restored`、`can_continue`，保留内核的原版阶段/计数器及君主winner字段。普通败北恢复地图；出征主将/君主及整场战果继续保留待续。阶段15复用已存在的保留交锋图逻辑。这里不创建原版未登记的俘虏或假定败北者最终命运。

4种合法动作重放的受控集成场景、8个受控原版分派边界和3个完整返回地图SRAM验证会话衔接；128+96组此前原版内核案例继续回归。详见 `../docs/duel-validation.json`。淡出、文字、音频与逐帧调度和整场最终结算仍未完成。


## 单挑会话与原版内核（开发中）

`OriginalState::duel_strike` 实现伤害边界，`duel_ai` 读取ROM打包决策表，`duel_finish` 原子处理败北或投降的统计、兵力和俘虏。三个方法都不代替原版动画帧循环。

`OriginalSession::begin_duel/choose_duel_command/advance_duel/answer_duel_surrender` 管理独立duel状态和v2事件重放。`duel_orders`、`duel_exchange`、`duel_defeat`、`duel_retreat` 及三段投降状态可保存；结束进入共用 `clash_result`，kind为duel并保留outcome。单挑后退使用 `duel_return` 标记返回既有军令，第一次普通推进保持原版连续行动状态，之后清除标记。NPC帧与随机游标记录在推进事件中，旧交锋动作语义不变。

新 `original_duel_test.cpp` 与 `tests/original_duel.gd` 独立验证原版样本和会话/UI，`validate_runtime.py` 默认包含8组。详细范围与未还原的动画/音效/帧调度见 `../docs/duel-validation.json`。完整重制与发布条件仍未满足。


玩家君主失败由 `human_failure_step(loser,winner,inputs)` 判断原版AFA8是否允许离开全局16；inputs位0/1对应两名玩家的A/B确认，返回11或16且不修改SRAM/RNG。会话 `begin_human_failure` 从已结算全局15生成可重放报告，`finish_human_failure` 只在有玩家存活时恢复战场并转83/94战后流程。报告留在expedition战斗记录内以保持v2部署基底重放；全体失败保留终局现场，禁止继续。新事件显式区分旧版不能普通继续的clash_result，不修改旧事件含义。


电脑战术前段：`tactical_ai_assessment(side, round, points, cursor)` 返回kind=retreat或plan、slot、score（未计算时-1）、role_slot（未调整时-1），保留原版整数运算和SRAM角色字节。plan仅表示到达尚未实现的部队规划边界，不能解释成结束回合。`advance_computer_tactics` 为已存在的expedition电脑守方边界添加显式重放事件；实际撤军生成带computer标记的retreat结果，经既有确认/战后结算继续。计划边界只允许评估一次，避免重复消费随机数。旧版end_tactical_turn事件仍停在computer边界，保持存档含义。


电脑守军补位新增 `tactical_ai_fort(target, previous_slot, points, status)`，仅移植A0非负的A2D7普通扫描、ADF3主将锚点与A33F相邻空城堡规则，结果move或role_plan；role_plan仅是后续角色AI入口。`plan_computer_tactics` 通过新的computer_fort事件将已验证move交给tactical_step，原版方向0/1/2/3映射为原生3/2/1/0。规则和移动在会话副本中同时提交，computer_cursor保存已行动槽位。后续角色计划不反复抽随机数或跳过当前单位。旧computer_tactics事件的含义不变。

`tactical_ai_strategy(target, slot, points, status, cursor)` 在守方role_plan后执行原版角色前置规则与计策筛选，返回strategy、no_strategy或尚未移植的role_reassignment。包含B8E1/B614概率、B634智力/机动力掩码、B69B敌军筛选和B803的48格顺序，直接读取指定ROM的偏移表。`evaluate_computer_strategy` 使用独立computer_strategy事件保存入口随机游标并严格重放结果，不修改旧事件含义；选定后的计策执行与移动续算尚未接入，禁止重复评估。最新验证见 `../docs/computer-strategy-validation.json`。

后续新增 `resolve_tactical_strategy(city, side, slot, target_slot, strategy, frame, status, cursor)`，处理原版CE1E后的逻辑效果，以事务副本提交SRAM、状态与随机游标；参数错误不部分修改。消耗表、伤害表与距离门槛直接读取ROM。`execute_computer_strategy` 另执行CD76消耗并生成strategy_result；`finish_computer_strategy` 确认报告后调用已验证的战果检查/必要的回合交接。重放事件分别记录执行入口的随机游标、帧字节及确认步骤；旧computer_strategy选择事件的含义不变。逻辑折叠动画等待，玩家施计入口和帧间随机同步未完成。最新验证见 `../docs/strategy-effect-validation.json`。

## 电脑移动与主动交战（未发布）

`OriginalState::tactical_ai_motion` 是计策未选中后的守军决策入口；不会重复消费策略随机数。角色0的追击、角色2的城堡接近和相邻攻击返回原版方向0上/1下/2左/3右；会话层转换为现有方向接口。`scan` 和 `pending` 保留未实现的扫描/绕行边界，禁止伪造结束回合。

`OriginalSession::continue_computer_motion` 记录 `computer_motion` 事件，保存阵形所需的帧字节，接续移动或交战准备。`prepare_clash` 增加默认值为true的 `human_attacker` 参数，旧调用语义保持不变；电脑主动攻击传false并调整发起方阵形。新增 `original_computer_motion` 原生和Godot测试覆盖604个原版决策、两段连续命令执行、合法玩家接近敌军后的电脑交战、投降返回、继续扫描、严格回放与伪造拒绝。此范围不等于完整战场AI。

## 绕行、缓存目标与同队续行（未发布）

`OriginalState::tactical_ai_flank` 只接续已有 `fort_flank` 或 `cached_target` 入口，输出 `move`、`scan` 或 `retry_strategy`。缓存写入与清除保留原版发生时机；它本身不消费随机数。`tactical_ai_fort(..., reuse_unit=true)` 执行负 A0 同队选择；`tactical_ai_strategy(..., direct=true)` 是不经过角色概率门槛的 B61D 补充检查。默认参数保持以前调用语义。

会话增加 `continue_computer_flank`、`retry_computer_strategy`，并通过 `plan_computer_tactics(true)` 及Godot的 `resume_computer_unit` 衔接同队。分别使用新事件 `computer_flank`、`computer_strategy_retry`、`computer_reuse`，不重写旧事件。新增 `original_computer_flank` 测试核对1,316组原版样本及96帧连续运行，并验证连续两步到达、缓存清除、计策重试、存档回放与非法操作拒绝。`scan` 仍是下一步待实现的原版循环边界，不能当作结束回合。


### 无命令轮询与整圈结束（未发布）

`original_tactical_scan_ai.cpp` 实现 A2DF–A330 的空槽跳过、起点保留和命令4；1,658个原版片段覆盖368轮。`OriginalSession::continue_computer_scan` 在同一次检查中直接接续选中部队，不重复撤军评估。旧存档从 `computer_plan.previous_slot` 或 `reuse_unit` 重建起点，新事件独立回放；同队续行初始 FF 与普通空槽起点均有测试。

电脑结束走 DEAC–DEB5 清零机动力，再执行 C761/交接，不调用玩家结束的 `tactical_end_turn`，不额外储存剩余点数。43帧原版交接比较完整SRAM、部队顺序和交接字段；演出后的随机时序不计为已匹配。新增 `original_computer_scan` 测试也覆盖11回合PC轮换至限时边界及严格回放。界面可从旧停点点击“检查下一部队”。详细限制与验证见 `../docs/computer-scan-validation.json`。


### 角色1/3近敌搜索和耗尽攻击（未发布）

`tactical_ai_motion(..., true)` 在既有验证之后执行 A57C/ACE7 的近敌搜索；默认参数保持历史角色停点。`OriginalRom::tactical_nearby_offset` 按原版读取28个编码（搜索循环24项、最终评分循环28项）。角色0原有追击与角色1/3的失败回退共用只读实现。`continue_computer_nearby` 经新回放事件执行真实移动/交战，和旧移动流程共用 `apply_computer_motion`。

`finish_exhausted_computer_attack` 对应不足3点时的 DF27 路径，只对1或2点且目标有效的已决定攻击交接。新事件保留旧失败调用的原子语义。测试包含1,515条原版决策、9段连续运行、六/十二队真实布阵角色、交战和384事件存档回放。参见 `../docs/computer-nearby-validation.json`；此实现不宣称剩余战术AI或原版演出已完成。


### 敌军占堡后的守军续行（未发布）

`tactical_ai_occupied_fort(target, slot, points, status, cursor, scratch21=-1)` 移植 AAD7–ACDE 的缓存、攻击/计策门槛、转向和移动选择。参数 `scratch21` 明确表示该入口继承的临时字节；未知时在私有副本比较零/非零分支，只提交不依赖该字节的决定。不能推导的分支返回错误且不改 SRAM/RNG。

`continue_computer_occupied_fort` 从旧 `computer_strategy.kind=role_reassignment` 停点续行，新增独立事件并严格验证 cursor/frame；实际执行沿用移动、电脑主动交战和计策管线。当前首次选队后的战力统计可以恢复 $21，后续扫描及跳过统计的全部来源尚未接入。旧历史保存结果保持不变。1,200条原版样本、两段连续原版流程和三份完整操作历史及回放验证见 `../docs/computer-occupied-fort-validation.json`。


### 跨决策临时状态恢复（未发布）

`tactical_ai_motion` 和 `tactical_ai_strategy` 增加可选 `int *scratch21` 输出，不改变已有 JSON 结果、SRAM 或随机调用。原版保持字节的路径保持调用者的值；实际写入按原版覆盖，非法输入不修改它。`OriginalSession::computer_scratch21_` 是从历史事件推导的内部状态，不序列化、不信任存档提供的字节。重新开始、载入非战术状态及未验证的命令/演出边界清除其来源。

`continue_computer_occupied_fort(false)` 保持旧事件含义；传入 `true` 使用已恢复状态并写入新的 `computer_occupied_fort_resume` 事件。Godot 的 `resume_computer_occupied_fort` 接口用于界面续行。两份旧停点在载入后继续，与未重载的会话结果一致；篡改报告或游标会被严格回放拒绝。来源未知时的双分支一致性检查继续生效。详见 `../docs/computer-scratch-validation.json`。

### 角色调整返回

`OriginalState::tactical_role_return(argument, points, status)` 还原 A2D6 返回后的守军第 1 队移动请求；返回 `move`、`blocked` 或 `waiting`。空槽、无效参数和存在歧义的未知方向返回错误。`OriginalSession::continue_computer_role()` 接入 `move` / `blocked`，保存 `computer_role_return` 事件并保留原来的扫描游标。`can_continue_computer_role()` 在副本上验证可否续行；Godot 同名接口用于按钮可用性。

`computer_argument_` 仅为回放派生状态，不写入存档。已执行的移动更新它，已对照的撤军流程保持它，交战/计策等未完整跟踪的边界清除它。原有 `plan_computer_tactics()` 对角色返回停点的拒绝行为保持不变。新增回归包括原版状态等待与受阻返回的区分、跨存档清除旧方向、重复指令拒绝，以及实际指令槽与扫描槽的分离。


### 玩家施计入口（未发布）

`OriginalState::player_tactical_strategies(side, slot)` 读取武将智力及ROM菜单分组，返回按原版顺序排列的计策、费用和菜单页。`quote_tactical_strategy(side, slot, target_slot, strategy, points)` 只读验证计策开放、机动力、敌军槽/占位和目标地形；不消费随机数。

`OriginalSession::prepare_tactical_strategy`、`cancel_tactical_strategy`、`confirm_tactical_strategy`、`finish_tactical_strategy` 分别记录同名新事件。确认复用已验证的八计效果内核，事务提交状态和一次扣费；结果确认执行战果检查，必要时交接。未处理的施计锁住其他战术操作；帧字节、随机游标及最终战局均经严格历史回放验证。Godot同名接口驱动双方玩家操作。

`original_player_strategy` 回归覆盖580条受控原版记录、双方会话、存档篡改拒绝、耗尽交接和主将转属；界面测试36项检查。整体结果及限制见 `../docs/player-strategy-validation.json`，原版演出与全局帧调度仍未完成。


### 电脑进攻方及守城战入口（未发布）

`OriginalState::tactical_ai_attacker_plan(target, previous_slot, reuse_unit)` 对应A2D7–A3C5的进攻分支及ADE5/AE25目标定位；清理原版扫描经过的空槽角色，支持首次同队选择及正序选队。`tactical_ai_strategy` 增加末尾 `side=0, round=0`，`tactical_ai_motion` 增加末尾 `side=0, enemy_commander=-1`。默认值保持已有守軍事件和结果，进攻方显式传128；不按守军角色15跳过进攻主将。

`OriginalSession::begin_tactics()` 支持完成布阵后的battle阶段。`advance_computer_attack()` 按一个可保存逻辑边界推进，使用新的 `computer_attack` 事件记录入口帧与随机游标。玩家接手后沿用原有战术API；进攻撤军及战后战略续接仍保留边界，不调用玩家出征的错误返回路径。

守城战严格重放先将部署基底的游标还原为战略AI入场游标，再重放战术事件并比较完整保存对象。实际战术游标不覆写历史AI对象。新回归覆盖858条原版决策链、192帧连续四步移动、三种实际布阵/操作历史、严格恢复及篡改拒绝、施计、交战军令与回合交接。详见 `../docs/attacker-ai-validation.json`。


### 来袭撤军和战后返回（未发布）

`resume_computer_attacker_retreat()` 以独立事件续接 `held_retreat`。DEB8–DECC生成的电脑标记不含主将0x40位，统一调用 `retreat_attacker`，不调用玩家整军撤退内核。D30C总扣1点机动力；原城满员时仍扣点但不改变SRAM。报告确认保持选队游标，检查主将离场原因20或全军撤离原因36。

共同战果API支持 `battle` 和 `expedition`。`finish_withdrawal_result()` 对来袭战场清理后记录AI战果并调用 `rotate_turn()`，其他出征仍调用 `enter_turn()`。月份事件加入后再裁剪到64项，保证跨月存档有效。新事件对随机游标作严格范围检查，并经操作重放校验最终状态。

`original_invasion_result` 覆盖32组原版撤退、3组清场/战略轮换、玩家守军撤离、电脑普通及主将撤离、满员失败、跨月及存档历史容量。Godot新增87项结果控件与保存恢复检查。完整结果及未验证范围见 `../docs/invasion-result-validation.json`。


### 来袭限时与败北专项验收（未发布）

`original_invasion_endings` 比较8个原版败北边界及4条连续结果流程（96步），并严格重放607事件限时场景、普通守军全灭、单人终止、双人续局及来袭电脑君主败北。共同结算公式未更换；验证终点包括来源城满员离散、30城领地接收、跳过已败君主及下一战略回合。终局操作错误现在提示先确认报告，避免将已支持的分支标成未实现。

新增Godot专项61项，检查限时报告、玩家失败界面、领地接收锁定和战略返回的实际控件可达性；并非桌面视觉验收。样本受控起点、原版片段范围及当前限制见 `../docs/invasion-endings-validation.json`。


### 正常启动内存下的左军恢复

`recover_clash_strategy()` 仅处理现有 `ai_scratch` 停点。E02D–E042 将0004–07FF清零；正常十队以内兵队初始化覆盖04B0–04F1。534个参考场景的64帧标记写入追踪没有改变04F2–04F4。新API按这个正常启动内存模型填入三字节，并通过原有军令算法推进到phase4；只在整个操作成功后提交。`recover_clash_strategy` 事件严格重放，不接受存档直接指定尾部RAM，不改变旧 `resume_clash_strategy` 记录。界面先尝试原有全值一致性分析，必要时调用新API。

155条独立原版军令对照包含不修改尾部RAM的真实暂停场景；32条真实操作组成的新存档可恢复并连续执行112步到败北，再结算返回战略地图。534段场景追踪不是穷举所有原版写入路径；越界兵队、任意模拟器RAM导入、逐帧输入/RNG同步不在本次支持声明内。详见 `../docs/clash-tail-validation.json`。


### 空槽位残留移动命令

新增 `OriginalState::tactical_empty_role_return`、`OriginalSession::continue_empty_computer_role` 和独立 `computer_empty_role_return` 事件。仅允许守军首槽ID为FF；原有普通部队 `tactical_step` 和 `computer_role_return` 的校验与重放含义不变。`can_continue_computer_role` 的只读检查及界面按钮现在也覆盖新入口。

按C7EF原版访问6C36（FF号武将的偏移6字节），用C83A/C8A9/C8B7/C8CA逐方向边界判断与原版8位移动费用。移动成功仅更新6DAB和机动力；BA32检测FF后返回，不画出部队、不改变占位图。未知参数枚举4个低位方向，只有完整结果及SRAM都一致才提交；状态低四位非零保持原版等待。316条新原版样本包含37次位置字节变化、135次受阻、144次等待；另外4段连续原版记录支持实际空槽位会话续行。限制和全套回归见 `../docs/empty-role-validation.json`。

### 长战斗历史

原生调用者用 `session.perform_battle_action([](auto &s){ return s.advance_clash(); })` 包装单个已有战斗操作；Godot绑定已自动包装。活动记录达到768条时，归档和操作以同一副本提交。`archive_battle_history()` 也支持显式归档；直接操作API不自动分段，以保留旧记录重放语义。只读电脑角色续行检查使用同样的包装，避免记录写满时错误禁用界面。

v2存档的可选 `tactics.history` 最多256段，每段768至1024条，当前 `moves` 最多1024条。所有段都从原始布阵逐条重放校验，不信任归档中间状态；AI暂存值与随机状态跨段保留。Godot文件上限32 MiB，战斗结束后释放历史。`original_history` 覆盖3358步实际交锋、4次归档、32段存储压力、超过1 MiB的读档、错误操作与篡改拒绝。容量上限并非无限，完整上限性能及所有战斗分支未覆盖。详见 `../docs/history-validation.json`。


### 战术图像及自然失败验收

`OriginalRom::tactical_pixels(city,sram)` 只读解码256×160 NES调色板索引，`OriginalSession::tactical_pixels()` 从当前已布阵战斗取状态；Godot的 `tactical_image()` 转为RGB8。兵力、主将、兵种标记与配色均读取指定ROM表，未部署/离场不画出，非法名册/位置/势力拒绝。36张参考视图逐RGB像素校对，静态图像匹配不包含原版闪烁或全部动画。

`original_campaign_defeat` 从未经修改的刘备难度0开局，执行结束回合/撤退优先/失败后交战策略，经2245次操作和3场来袭到207年9月单人失败。每1000操作及终点严格重放，固定帧时钟，不修改战局初值。此路径不是原版同步回放或统一验收。限制和文件证据见 `../docs/tactical-picture-validation.json`。


### 统一结局评分与最后一城

`OriginalState::unification_summary()` 只读计算原版 A17B–A29F 的驻将数、忠诚平均值、招募分、统治平均值、最终分数及发言武将；按每一步整数除法截断，包含君主忠诚255，最高忠诚并列时取最后一个非君主。分数小于60、60至79、80以上分别选择原版画面/音乐编号0、2、1。它通过快照暴露，不修改存档格式或随机状态。

`original_unification_test` 核对27个原版受控端点、810个缺城边界，以及三次正式战斗操作完成末城俘虏/占领结算、终局保存/恢复和命令锁定。战斗初始29城归属是受控设置，不能记为自然通关。证据及未完成范围见 `docs/unification-validation.json`。
