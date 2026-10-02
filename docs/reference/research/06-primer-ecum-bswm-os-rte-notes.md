# Part XI（Classic AUTOSAR 入门：原理与工作流）Phase 1 事实研究笔记：EcuM / BswM / Os / RTE

> 来源：`artifacts/pdf-text/autosar-cp-R25-11/` 下 R25-11 SWS 文本。引用格式：`(文档简称 R25-11 p.<PDF页>, SWS ID)`；页码为 `=== PDF PAGE n ===` 的 PDF 页码，不是文档内印刷页码。
> 本文件只做事实归纳，不复制长段原文。标注"推断"的地方是研究者结论，不是 SWS 原文。
> 注意：`00-writing-conventions.md` 中"仓库无 EcuM/Os/Rte/BswM SWS"已过时，这四份 R25-11 SWS 现在可作为依据；但本仓库仍没有 CanIf/CanTp/Dem/NvM 的 R25-11 SWS（PduR/CanIf/COM 另有文件，未在本文覆盖）。
> 另：`AUTOSAR_CP_EXP_ModeManagementGuide`（EcuM/BswM SWS 反复引用的 "Guide to Mode Management"）不在仓库中，所以 BswM 的"典型启动 action list"没有官方示例可引，见 §2.6。

## 0. 文档身份

| 文档 | 标题 | Doc ID | Release | 状态 | 依据 |
|---|---|---|---|---|---|
| EcuM | Specification of ECU State Manager | 78 | R25-11（2025-11-27） | published | EcuM R25-11 p.1 |
| BswM | Specification of Basic Software Mode Manager | 313 | R25-11 | published | BswM R25-11 p.1 |
| Os | Specification of Operating System | 34 | R25-11 | - | Os R25-11 首页 |
| Rte | Specification of RTE Software | 84 | R25-11 | - | Rte R25-11 首页（1609 页） |
| Det | Specification of Default Error Tracer | 17 | R25-11 | - | |
| WdgM | Specification of Watchdog Manager | 80 | R25-11 | - | |
| ComM | Specification of Communication Manager | 79 | R25-11 | - | |

---

## 1. EcuM（ECU State Manager）

### 1.1 "fixed" 还是 "flexible"：R25-11 的结论

- **R25-11 的 EcuM SWS 只描述 Flexible ECU Manager，没有 fixed 版本。** 依据：Introduction 全篇按 flexible 写（EcuM R25-11 p.15-16）；Change History 在 4.4.0（2018-10-31）条目写明 "Removed EcuM fixed version references"（EcuM R25-11 p.2）；Functional Specification 开篇明确 "没有标准 ECU modes/states，由集成者决定"（p.30）。
- 全文检索未发现 `EcuM_GoDown`、`EcuM_GoHalt`、`EcuM_GoPoll`、`EcuM_KillAllRUNRequests`、`EcuM_GoToSelectedShutdownTarget`（旧 fixed API 在 R25-11 中不存在）。取而代之的是 `EcuM_GoDownHaltPoll(UserID)`（SWS_EcuM_91002, EcuM R25-11 p.111）+ `EcuM_SelectShutdownTarget`（SWS_EcuM_02822, p.118）。
- flexible 的核心思想：大多数 ECU state 不再由 EcuM 自己实现，而是变成由 BswM 控制的 generic modes；EcuM 只在"早期 STARTUP、晚期 SHUTDOWN、SLEEP 中调度器被锁住时"接管（EcuM R25-11 p.15）。RTE 与 BSW Scheduler 在 flexible 描述里已合并为一个模块（p.15）。
- 向后兼容：配置得当时可兼容旧版（p.16）；具体做法见 Guide to Mode Management（不在仓库）。
- **注意（推断）**：用户实际项目（RTA-CAR 12.9.0，AUTOSAR 版本未知）用的可能是旧 R4.x 的 fixed EcuM（`EcuM_GoDown` 等）。写教程时需说明"R25-11 只有 flexible；老工程可能是 fixed，名字不同"。

### 1.2 Phases（阶段）与术语

EcuM 把旧版的 "state / mode" 区分重新定义：旧 ECU States 现在变成经 RTE mode port 暴露的 **Modes**，旧 ECU Modes 变成 **Phases**（EcuM R25-11 p.30-31）。Figure 7.1（p.32）的 phases：

| Phase | 含义 | 依据 |
|---|---|---|
| STARTUP | 初始化到 mode management 设施可用为止：低层驱动 -> 起 OS -> 初始化 SchM、BswM。子阶段 **StartPreOS**（OS 之前）与 **StartPostOS**（OS 之后） | p.31, p.33 |
| UP | 从 SchM 启动、`BswM_Init` 调用后开始。此时 EcuM 被动，BswM 负责后续动作；"UP 阶段内容"包含旧版部分 sleep 状态 | p.31, p.33 |
| SHUTDOWN | STARTUP 的反向；`EcuM_GoDownHaltPoll` 且 shutdown target 为 RESET/OFF 时触发；子阶段 **OffPreOS**、**OffPostOS** | p.34, p.47, SWS_EcuM_03022 |
| SLEEP | 省电，通常不执行代码；子序列 GoSleep / Halt / Poll / WakeupRestart / WakeupValidation | p.34, p.52-60 |
| OFF | 断电；需硬件自断电能力，否则建议用 reset 代替 | p.34, p.20 |

Shutdown targets：OFF / SLEEP / RESET（EcuM R25-11 p.17, p.71）。EcuM 对 SW-C 暴露的 mode group `EcuM_Mode` = {STARTUP, RUN, POST_RUN, SLEEP, SHUTDOWN}，初始 STARTUP（SWS_EcuM_04132 p.162；SWS_EcuM_04107 p.163）。注意：EcuM 只请求 RUN 与 POST_RUN 的进出，SLEEP 由 BswM 设（p.100）。

### 1.3 EcuM_Init 的精确序列（StartPreOS）

SWS_EcuM_02411（EcuM R25-11 p.37-38）规定表 7.1 的顺序，SWS_EcuM_02684（p.39）要求 `EcuM_Init` 按此执行。`EcuM_Init` 签名 `void EcuM_Init(void)`，Service ID 0x01，**永不返回（因为它调用 StartOS）**（SWS_EcuM_02811, p.112）。前提：调用前已完成最小 MCU 初始化（栈、C 变量初始化）（p.37 §7.3.1）。

| # | 步骤 | Opt. | SWS ID / 页 |
|---|---|---|---|
| 1 | callout `EcuM_AL_SetProgrammableInterrupts`：有可编程中断优先级的 ECU 在起 OS 前设置 | 是 | SWS_EcuM_04085, p.136；表 p.38 |
| 2 | callout `EcuM_AL_DriverInitZero`（Init block 0）：只能初始化**不使用 post-build 配置**的 BSW；也可含任何 pre-OS 底层代码 | 是 | SWS_EcuM_02905, p.137；p.38, p.41 |
| 3 | callout `EcuM_DeterminePbConfiguration`：返回完全初始化的 `EcuM_ConfigType*`（post-build 配置根） | 否 | SWS_EcuM_02906, p.138 |
| 4 | 配置一致性检查：对比 `EcuM_ConfigType` 内 hash 与 `ECUM_CONFIGCONSISTENCY_HASH`；失败调 `EcuM_ErrorHook(ECUM_E_CONFIGURATION_DATA_INCONSISTENT)` | 否 | SWS_EcuM_02796, 02798, p.44；SWS_EcuM_02904, p.136；图 p.40 |
| 5 | callout `EcuM_AL_DriverInitOne`（Init block I） | 是 | SWS_EcuM_02907, p.138 |
| 6 | 取 reset reason：`Mcu_GetResetReason`，按 `EcuMWakeupSource` 配置映射为 wakeup source（与 `EcuM_SetWakeupEvent`/`EcuM_GetValidatedWakeupEvents` 语义相关）；EcuM 须记住该 wakeup source，稍后由 `EcuM_MainFunction` 验证 | 否 | 表 p.38；SWS_EcuM_02623, p.38；SWS_EcuM_02826, p.132 |
| 7 | 选默认 shutdown target（`EcuMDefaultShutdownTarget`） | 否 | SWS_EcuM_02181, p.41（该条文字写作 "call EcuM_GetValidatedWakeupEvents"，疑似 SWS 笔误；图 p.40 用的是 `EcuM_SelectShutdownTarget`，SWS_EcuM_02822 p.118） |
| 8 | callout `EcuM_LoopDetection`（`EcuMResetLoopDetection`=true 时） | 是 | SWS_EcuM_04137, p.139 |
| 9 | `StartOS(ECUM_DEFAULT_APP_MODE)` | 否 | SWS_EcuM_02603, p.41；图 p.40 |

补充：
- Figure 7.4（p.40）未画出步骤 1（SetProgrammableInterrupts），以表 7.1 为准（它标 Opt.=yes）。
- StartPreOS 应尽量短；**不要用中断；若必须用，仅允许 category 1 ISR**（Category II 需运行中的 OS；每个中断向量只能属一种 category）（EcuM R25-11 p.41 及脚注 3）。
- `Mcu_Init` 不提供完整 MCU 初始化，硬件相关步骤须放进两个 DriverInit callout（p.41）。
- 多核：EcuM_Init 在每个核上运行，DriverInitZero/One 只初始化当前核的 MCAL（SWS_EcuM_04145, p.83）；master 核循环 `StartCore` 再 `StartOS`（p.82）；OS 在 `StartOS` 内两次核间同步（p.81）。

### 1.4 `EcuM_StartupTwo` 与 StartPostOS

- 控制流：`EcuM_Init` -> `StartOS` -> OS 调 StartupHook -> **autostart 的 OS task 作为第一个动作调 `EcuM_StartupTwo`**（EcuM R25-11 图 7.3 p.37）。"集成者必须实现一个自动启动的 OS task 并以 `EcuM_StartupTwo` 为首个动作"（p.37）。
- `EcuM_StartupTwo`：`void`，Service ID 0x1a，Non Reentrant（SWS_EcuM_02838, p.112）。SWS_EcuM_02806（p.112-113）：必须在 StartOS 直接引起的 task 里调用——autostart task（隐式激活）或被显式激活的 task；默认配置为默认 application mode 下的 autostart task。
- StartPostOS Sequence（SWS_EcuM_02934/02932, p.41-42；图 7.5 p.42）：
  1. 启动 BSW Scheduler：`SchM_Start()`
  2. 初始化 BSW Mode Manager：`BswM_Init(ConfigPtr)`
  3. 初始化 BSW Scheduler：`SchM_Init()`（初始化 BSW 模块用的临界区信号量等）
  4. 启动 scheduler 定时：`SchM_StartTiming()`（启动 BSW/SWC 的周期事件）
  - 一致性：Rte SWS 要求 `SchM_Start` 在 `BswM_Init()` 之前调用（SWS_Rte_91171, Rte R25-11 p.927）；`BswM_Init` 只要求 OS 与 SchM 已初始化（SWS_BswM_00118, BswM R25-11 p.69）。
  - 多核：每个核各自 `SchM_Start` -> 循环 `BswM_Init`（该核上的每个 BswM 实例）-> `SchM_Init` -> `SchM_StartTiming`（SWS_EcuM_04016/04018, SWS_EcuM_04014, p.81-84）。"每个 partition 里有一个 BswM 负责在该核启动 RTE"（p.81）。

### 1.5 EcuM 在 OS 启动之后还拥有什么

EcuM R25-11 没有自己的状态机（p.98），OS 启动后它仍然负责：

1. **StartPostOS 序列**（上节）。EcuM 之后"把控制交给 BswM"（p.36 §7.2.1）；UP 阶段起点 = SchM 已起 + `BswM_Init` 已调（p.33）。刚进 UP 时：没有内存管理（NvM）、没有通信栈、没有 SW-C 支持（RTE）、SW-C 未启动，只有 BSW MainFunctions 按初始 mode 的 runnable 在跑（p.33）。
2. **`EcuM_MainFunction`**（SWS_EcuM_02837, ID 0x18, `SchM_EcuM.h`, p.150）：OS 运行期间所有 EcuM 活动，主要是 wakeup validation；周期建议约为最短 validation timeout 的一半；**不应放在会执行 runnable 的 task 里**（p.151）。
3. **Wakeup 源管理**：`EcuM_SetWakeupEvent`（SWS_EcuM_02826, p.132）、`EcuM_ValidateWakeupEvent`（SWS_EcuM_02829, p.134）、`EcuM_ClearWakeupEvent`（SWS_EcuM_02828, p.124）等；状态 NONE/PENDING/VALIDATED/EXPIRED（SWS_EcuM_04091, p.62）；状态变化时向 BswM 发 `BswM_EcuM_CurrentWakeup`（SWS_EcuM_04003, p.63），VALIDATED 时调 `ComM_EcuM_WakeUpIndication`（图 p.62/65）。最多 32 个 wakeup source（p.64）。
4. **RUN / POST_RUN 请求仲裁**（"ECU Mode Handling"，`EcuMModeHandling`=true 时，SWS_EcuM_04115, p.99）：SW-C 经 `EcuM_RequestRUN/ReleaseRUN`（SWS_EcuM_04124/04127, p.114-115，同一 user 不可嵌套请求）和 `EcuM_RequestPOST_RUN/ReleasePOST_RUN`（SWS_EcuM_04128/04129, p.116-117）；端口接口 `EcuM_StateRequest`（SWS_EcuM_04131, p.161）。EcuM 把仲裁结果用 `BswM_EcuM_RequestedState(ECUM_STATE_RUN, ECUM_RUNSTATUS_REQUESTED/RELEASED)` 告知 BswM（SWS_EcuM_04144/04117/04118/04119, p.99），由 BswM 决定何时切换；BswM 通过 `EcuM_SetState` 通知 EcuM（SWS_EcuM_04122/04123, p.114），EcuM 再经 RTE mode port 通知 SW-C：`Rte_Switch_currentMode_currentMode`（SWS_EcuM_04133, p.162）。
5. **SLEEP**：`BswM` 选好 shutdown target 后调 `EcuM_GoDownHaltPoll`（SWS_EcuM_91002, p.111；p.53, p.71）；EcuM 做 GoSleep（`EcuM_EnableWakeupSources`、占用 `RES_AUTOSAR_ECUM_<core#>`，SWS_EcuM_02389/03010, p.54）-> Halt（`EcuM_GenerateRamHash`、`Mcu_SetMode(HALT)`、`EcuM_CheckWakeup`…，SWS_EcuM_02960/02863/02961, p.55-56）或 Poll（循环 `EcuM_SleepActivity`/`EcuM_CheckWakeupHook`，SWS_EcuM_03020, p.57）-> WakeupRestart（恢复 MCU normal mode、`EcuM_DisableWakeupSources`、`EcuM_AL_DriverRestart`、释放资源、解锁调度，p.58-60；SWS_EcuM_02923 p.148）。**SLEEP 不关 OS**，sleep mode 对 OS 透明（SWS_EcuM_02951, p.54）。WakeupValidation 序列见 p.64-66（`EcuM_StartWakeupSources`/`CheckValidation`/`StopWakeupSources`，`EcuMValidationTimeout`，SWS_EcuM_02566/02565, p.66）。
6. **SHUTDOWN**：BswM 先 `EcuM_SelectShutdownTarget`，再 `EcuM_GoDownHaltPoll`（RESET/OFF）（图 7.7 p.48）。
   - OffPreOS（SWS_EcuM_03021, p.48；图 p.50）：`EcuM_OnGoOffOne`（SWS_EcuM_02916, p.140）-> `BswM_Deinit` -> `SchM_Deinit` -> 检查 pending wakeup（按 `EcuMIgnoreWakeupEvValOffPreOS`，SWS_EcuM_04151/04152, p.49；此时 SchM 已 deinit，无法再验证）-> 若有 wakeup 则改 target 为 RESET -> `ShutdownOS`（最后一个动作，SWS_EcuM_02952, p.49）。
   - OS 在 `ShutdownOS` 末尾调 ShutdownHook，**ShutdownHook 必须调 `EcuM_Shutdown`**，它不返回，复位或断电（SWS_EcuM_02953, p.49；`EcuM_Shutdown` SWS_EcuM_02812 ID 0x02, p.113）。
   - OffPostOS（表 7.3 p.51）：`EcuM_OnGoOffTwo`（SWS_EcuM_02917）-> RESET 时 `EcuM_AL_Reset`（SWS_EcuM_04065/04074, p.142/52）；OFF 时 `EcuM_AL_SwitchOff`（SWS_EcuM_02920/04075, p.141/52）。
   - 关机期间发生 wakeup：完成 shutdown 后立即重启（SWS_EcuM_02756, p.47）。
7. 回调/错误：`EcuM_ErrorHook`（SWS_EcuM_02904, p.136）。

### 1.6 Driver init list 概念：EcuM init list vs BswM-driven init

EcuM 配置有四个有序列表（SWS_EcuM_02559/04142, p.45/47；p.21 §5.1.2 说明初始化顺序由集成者负责）：

| 列表 | 何时/谁执行 | 备注 |
|---|---|---|
| `EcuMDriverInitListZero` | `EcuM_AL_DriverInitZero`，StartPreOS | 不能依赖 post-build 配置或 OS 功能（p.46 表 7.2） |
| `EcuMDriverInitListOne` | `EcuM_AL_DriverInitOne`，StartPreOS | 此时已有 post-build 配置 |
| `EcuMDriverRestartList` | `EcuM_AL_DriverRestart`，WakeupRestart | 通常是 Zero+One 合并列表的子集，顺序同合并列表（SWS_EcuM_02947/02561/02562, p.46） |
| `EcuMDriverInitListBswM` | BswM 的 action `BswMEcuMDriverInitListBswM`，UP 阶段 | 实际调用 `EcuM_AL_DriverInitBswM_<shortName>(void)`（BswM R25-11 p.150；EcuM p.47 SWS_EcuM_04142） |

各 init 调用的参数来自驱动的 `EcuMModuleService` 配置容器（SWS_EcuM_02730, p.46）。

表 7.2 的"推荐"示例（EcuM R25-11 p.46-47；注意这是 sample，不是强制）：
- **Init block 0**：Default Error Tracer（"应始终第一个初始化，以便其他模块报开发错误"）、Dem Pre-Initialization、访问 post-build 配置所需的驱动（不依赖 post-build/OS）。
- **Init block I**：MCU Driver、Port Driver、General Purpose Timer、Watchdog Driver（仅内部看门狗，外部的可能要先 SPI）、SPI、Watchdog Manager、ADC、ICU、PWM、OCU。
- **BswM 驱动的初始化（UP 阶段）**：其余 BSW。SWS 明说：初始化 NvM 并调 `NvM_ReadAll` 成为集成代码；**集成者负责在 `NvM_ReadAll` 结束后触发 Com、DEM、FIM 的初始化**，NvM 完成时通知 BswM；RTE 须在 NvM 与 COM 初始化之后才能启动；通信栈不必完全初始化 COM 就能初始化（EcuM R25-11 p.33）。
- 参见 `Det_Init`（SWS_Det_00008）/`Det_Start`（SWS_Det_00010）（Det R25-11 p.21-22）、`WdgM_Init`（SWS_WdgM_00151, WdgM R25-11 p.71）、`ComM_Init`（SWS_ComM_00146, ComM R25-11 p.113；注意 "ComM_Init 的 caveat：NvM 须已初始化"，ComM p.113）——这些是 EcuM 列表/BswM 列表里实际会放的 init。
- 对 RH850 教学的含义（推断）：Mcu/Port/Gpt/Wdg 属于 EcuM StartPreOS 的 init block；Can/CanIf/PduR/Com/ComM/NvM/Dcm 属于 BswM 阶段（Mcu_InitClock、PLL 等 MCAL 细节另见 04 号笔记）。

---

## 2. BswM（BSW Mode Manager）

### 2.1 角色

- 两个任务：**Mode Arbitration**（基于规则对 mode request / mode indication 做仲裁）+ **Mode Control**（执行 action list，调用其他 BSW 模块或 RTE/SchM 的 mode switch）（BswM R25-11 p.24 §7）。BswM 是"行为完全由配置决定的 mode 管理框架"，规范不规定实现方式（可整体生成，也可做规则解释器）（p.24）。
- Request 来源：SW-C（经 RTE）、BSW（如 Dcm 的 request）；Indication 来源：EcuM、ComM、各 BusSM、NvM 等（p.26 §7.1.3）。

### 2.2 Mode Arbitration：rules / conditions

- Rule = 由若干 mode condition 组成的布尔表达式，操作符 AND/OR/XOR/NOT/NAND；condition 对 `BswMModeRequestPort` 判 EQUAL/NOT_EQUAL；对 `BswMEventRequestPort` 判 SET/CLEAR（p.25; SWS_BswM_00252/00253/00254/00255, p.26）。
- 禁止用上一次规则评估结果作为后续规则输入（SWS_BswM_00117, p.27）。action 只能在 action list 上下文调用（SWS_BswM_00147, p.27）。
- 处理时机：`BswMRequestProcessing` = **IMMEDIATE**（在请求者上下文立刻仲裁、执行 action list，若请求者在中断里要注意限制）或 **DEFERRED**（延到 `BswM_MainFunction`）（SWS_BswM_00013/00014, p.28；事件端口 SWS_BswM_00257/00258, p.29）。含任何 DEFERRED condition 的规则每次 MainFunction 都评估（SWS_BswM_00060, p.29）；主函数处理期间到来的请求被推迟（SWS_BswM_00068/00069, p.29）。
- 初始化后的仲裁：`BswMModeInitValue`；未更新的 condition 视为 undefined，含 undefined condition 的规则不仲裁（SWS_BswM_00064/00241, p.30；SWS_BswM_00203, p.30）；所有 event port 初始化为 CLEAR（SWS_BswM_00251, p.31）；Rte/SchM mode 总有初始值（p.31）。
- 定时：`BswMTimer`（STOPPED/STARTED/EXPIRED），每个 `BswM_MainFunction` 递减，总是 DEFERRED（SWS_BswM_00261-00265, 00220, p.38-39）。

### 2.3 Mode Control：action list

- Action list = 有序 action 列表；元素可以是具体 action、对另一个 action list 的引用、或需评估的 rule；最多 7 层嵌套（SWS_BswM_00016/00017/00018/00019/00037, p.34；p.31-32）。
- 触发/条件执行：`BswMActionListExecution` = TRIGGER（评估结果变化时才执行）或 CONDITION（每次评估都执行）（SWS_BswM_00011/00023/00115/00116, p.36）；首次评估行为由 `BswMRuleInitState` 决定（SWS_BswM_00066, p.38）。
- 顶层 action list 按 `BswMActionListPriority` 由高到低执行；列表内按 `BswMActionListItemIndex` 升序（SWS_BswM_00275/00260, p.35-36）；`BswMAbortOnFail`=true 时某 action 返回 E_NOT_OK 终止列表（SWS_BswM_00055, p.37）。
- BswM 可调用任意 BSW 函数及用户自定义函数（`BswMUserCallout`）（SWS_BswM_00039/00040/00054, p.37）。

`BswMAvailableActions`（BswM R25-11 p.140-141，ECUC 表）中与启动/关机相关的预定义 action：

| Action 容器 | 作用 | 页 |
|---|---|---|
| `BswMEcuMDriverInitListBswM` | 调 `EcuM_AL_DriverInitBswM_<name>(void)` 执行一个 EcuM 管理的 BswM 驱动 init 列表 | p.150 |
| `BswMEcuMGoDownHaltPoll` | 调 `EcuM_GoDownHaltPoll` | p.151 |
| `BswMEcuMSelectShutdownTarget` / `BswMEcuMStateSwitch` | `EcuM_SelectShutdownTarget` / `EcuM_SetState` | p.140 |
| `BswMRteStart` / `BswMRteStop` | 调 `Rte_Start(void)` / `Rte_Stop` | ECUC_BswM_01073, p.169 |
| `BswMRteSwitch` / `BswMSchMSwitch` / `BswMRteModeRequest` | 通过 `Rte_Switch_*` / `SchM_Switch_*` 切 mode，或向 mode-manager SW-C 发 mode request | p.141, 45 |
| `BswMComMAllowCom` / `BswMComMModeSwitch` / `BswMComMModeLimitation` | `ComM_CommunicationAllowed` / `ComM_RequestComMode` / `ComM_LimitChannelToNoComMode` | p.140 |
| `BswMPduRouterControl`、`BswMPduGroupSwitch`、`BswMNMControl`、`BswMTimerControl`、`BswMClearEventRequest`、`BswMUserCallout` 等 | 见 p.140-141 | |

**注意：BswM 没有专用 `NvM_ReadAll` action**（全文只有 NvM 的 mode indication：`BswM_NvM_CurrentBlockMode`、`BswM_NvM_CurrentJobMode`）；调用 `NvM_ReadAll` 要走 `BswMUserCallout` 或"可调用任意 BSW 函数"这一条（SWS_BswM_00039, p.37）。

### 2.4 接口：mode request/indication 端口与 SW-C 侧

| API | 谁调 | ID / 页 |
|---|---|---|
| `BswM_Init(const BswM_ConfigType*)`，ID 0x00，Conditionally Reentrant（多 partition） | EcuM（StartPostOS） | SWS_BswM_00002, p.69；SWS_BswM_00043/00118 |
| `BswM_Deinit` | EcuM（OffPreOS） | SWS_BswM_00119, p.64 |
| `BswM_MainFunction`，ID 0x03，`SchM_BswM.h` | SchM（周期） | SWS_BswM_00053, p.78；SWS_BswM_00075：评估所有用 DEFERRED request 的规则 |
| `BswM_EcuM_CurrentState` / `_CurrentWakeup` / `_RequestedState` | EcuM | SWS_BswM_91003 p.65 / 00131 p.66 / 91004 p.66 |
| `BswM_ComM_CurrentMode` / `_CurrentPNCMode` / `_InitiateReset` | ComM | SWS_BswM_00047 p.62 / 00148 / 00217 p.63 |
| `BswM_CanSM_CurrentState`、`_EthSM_`、`_FrSM_`、`_LinSM_` | 各 BusSM | SWS_BswM_00049 p.61 等 |
| `BswM_Dcm_CommunicationMode_CurrentState` / `BswM_Dcm_ApplicationUpdated` | Dcm | SWS_BswM_00048 p.64 / 00158 p.63 |
| `BswM_NvM_CurrentBlockMode` / `_CurrentJobMode` | NvM | SWS_BswM_00104 p.74 / 00152 p.75 |
| `BswM_RequestMode`（generic request） | 其他 BSW | SWS_BswM_00046, p.75 |

- 无 callback notification（BswM R25-11 p.78 §8.4）。
- SW-C 侧 mode request port：BswM 作为 Service Component，用 `Rte_Read_modeRequestPort_..._requestedMode()` 读 request；用 `Rte_Switch_modeSwitchPort_..._currentMode()` / `SchM_Switch_...` 在 action list 里切 mode；用 `Rte_Mode_modeNotificationPort_..._currentMode()` 读已激活 mode（BswM R25-11 p.45-46 §7.6.1-7.6.3）。
- 处理周期（p.32-33 §7.2.1）：请求方 SW-C 经 Sender Port -> RTE -> BswM Receiver Port -> 规则评估（请求上下文或 MainFunction）-> action list 执行 -> 调 `Rte_Switch` 通知 SW-C。序列图 9.1（Deferred，p.91）/9.2（Immediate，p.92）。
- 多 partition：每个 partition 一个 BswM 实例，各有独立配置，action list 在 partition 本地执行（SWS_BswM_00320, p.40；p.39）。

### 2.5 EcuM 把控制交给 BswM 之后

- `BswM_Init` 之后 BswM 开始仲裁；"所有进一步 BSW 初始化、启动 RTE、(隐式)启动 SW-C 都成为 BswM action list 里的代码或 mode 驱动的调度"（EcuM R25-11 p.33 §7.1.2）。
- RUN 请求处理：SW-C 向 EcuM 请求/释放 RUN -> EcuM 仲裁 -> `BswM_EcuM_RequestedState` -> BswM 规则决定动作；BswM 在 mode 变化时 `EcuM_SetState`（EcuM R25-11 p.98-99）。BswM 在 RTE 执行完某 mode 的 runnable 时也会被通知（p.98，mode switched ack）。
- 关机：BswM 在 OS 关闭前"把执行权交还 EcuM"（p.36）：典型 `EcuM_SelectShutdownTarget` + `EcuM_GoDownHaltPoll`（p.48；p.53）。

### 2.6 典型启动 action list 示例

**SWS 中没有具体的启动 action list 示例**（BswM 只有 §9 的两张 immediate/deferred 时序图，p.91-92；"Guide to Mode Management" 不在仓库）。可由 SWS 句子拼出的**推断性**顺序（教程中须标"示意，非 SWS 示例"）：

1. 启动 mode：BswM 收到初始 mode / EcuM 状态后 -> `BswMEcuMDriverInitListBswM`（初始化 EcuM 管理的 BswM 驱动列表，如 SPI/EEP/Fls 等 memory 栈所需驱动）
2. 初始化 NvM、`NvM_ReadAll`（用户 callout）-> 等待 `BswM_NvM_CurrentJobMode` 指示完成（EcuM p.33）
3. `Com_Init` 等（集成者在 ReadAll 完成后触发：Com、DEM、FIM；EcuM p.33）
4. `BswMRteStart`（`Rte_Start`，ECUC_BswM_01073 p.169）—— "RTE 可在 NvM 与 COM 初始化后启动"（EcuM p.33）
5. `BswMEcuMStateSwitch` -> `EcuM_SetState(RUN)`（SWS_EcuM_04122）；ComM：`BswMComMAllowCom` / `BswMComMModeSwitch`

---

## 3. Os（AUTOSAR OS，基于 OSEK）

### 3.1 基线

- 以 OSEK/VDX OS（ISO 17356-3）为核心；API 向后兼容 OSEK（SWS_Os_00001, Os R25-11 p.36）；特性：静态配置、固定优先级调度、中断优先于任务、StartOS/StartupHook、ShutdownOS/ShutdownHook、保护设施（Os p.17, p.36）。
- OSEK 概念（task 状态 suspended/ready/running/waiting、ActivateTask/TerminateTask/ChainTask、SetEvent/WaitEvent、GetResource、SetRelAlarm 等）**不在本 SWS 重述**（本 SWS 只描述扩展与限制）；Glossary 仅定义：Basic Task = 不能自己阻塞（不能 wait Event）；Extended Task = 可阻塞并等待 Event（Os R25-11 p.21）。task 状态属 OSEK 知识，教程引用时应标明来源是 OSEK 而非本 SWS。

### 3.2 对象与扩展要点

| 主题 | SWS 事实 | 依据 |
|---|---|---|
| StartOS | 首次 `StartOS` 不返回（SWS_Os_00424）；ShutdownHook 返回后 OS 关中断并死循环（SWS_Os_00425）；`StartOS(AppMode)` 的 AppMode 决定哪些 autostart 的 Task/Alarm/ScheduleTable 被启动；多核时各核都要调，AppMode 必须一致或为 DONOTCARE（SWS_Os_00607-00610） | p.38, p.105 |
| 中断控制 | `DisableAllInterrupts/EnableAllInterrupts/SuspendAllInterrupts/ResumeAllInterrupts` 在 StartOS 前、ShutdownOS 后也可用（SWS_Os_00299） | p.38 |
| 启动后的 OS-App 状态 | StartOS 之后、任何 StartupHook 之前所有 OS-Application 置 `APPLICATION_ACCESSIBLE`（SWS_Os_00500） | p.62 |
| Counter / SWFRT | `IncrementCounter`（SWS_Os_00399 p.190）、`GetCounterValue`（SWS_Os_00383 p.191）、`GetElapsedValue`（SWS_Os_00392 p.192）；alarm 到期可递增 software counter（SWS_Os_00301）；OS 自己管理它直接用的 timer（SWS_Os_00374） | p.38-39 |
| Alarm | `SetRelAlarm` increment=0 返回 E_OS_VALUE（SWS_Os_00304）；允许 autostart 绝对 alarm（SWS_Os_00476）；alarm callback 仅 SC1（SWS_Os_00242） | p.37-38 |
| ScheduleTable | 一组 expiry point 的静态封装；每个 expiry point 含要激活的 Task 集、要设置的 Event 集（SWS_Os_00401/00402/00403），也可补充 execution budget（SWS_Os_00876）；由 Counter 驱动；`StartScheduleTableRel`（SWS_Os_00347）/`Abs`（00358）/`Synchron`（00201）、`NextScheduleTable`、`SyncScheduleTable`… | p.39-41, 179-189 |
| Resource | `RES_SCHEDULER` 不再自动存在，当普通 resource 处理（p.37）；Resource 不属于任何 OS-App，访问需显式授权（p.59）；多核另有 spinlock：`GetSpinlock`（SWS_Os_00686 p.199）、`ReleaseSpinlock`（00695）、`TryToGetSpinlock`（00703） | |
| Event | 属于 Extended Task；ActivateTaskAsyn/SetEventAsyn 为异步跨核版本（SWS_Os_91022/91023, p.213） | |
| ISR | category 1：OS 不感知，不能受保护，若与 OS-Application 共用则须属 trusted OS-App；category 2：OS 管理，可调多数 OS 服务（Os p.63；RTE p.183）。EcuM 补充：Cat II 需运行中的 OS，每个向量只能属一类（EcuM p.41 脚注） | |
| 中断源 API | `EnableInterruptSource`/`DisableInterruptSource`/`ClearPendingInterrupt`（SWS_Os_91020/91019/91021, p.211-212） | |
| 多核 | `StartCore`（SWS_Os_00676 p.198）、`GetCoreID`、`ShutdownAllCores`（00713 p.204）；StartCore 须在该核 StartOS 前调用（SWS_Os_00606）；IOC（Inter-OS-Application Communicator）见 §7.10（p.122-131） | |
| 外设访问 | `ReadPeripheral8/16/32`、`WritePeripheral*`、`ModifyPeripheral*`（SWS_Os_91013-91018, p.205-210） | |

### 3.3 OS-Application 与保护（"partition" 的含义）

- OS-Application = 一组 Task、ISR、Alarm、ScheduleTable、Counter、hook、trusted function 的内聚单元（Os R25-11 p.59；SWS_Os_00445）。若使用 OS-Application，所有这些对象必须属于某个 OS-App；同一 OS-App 内互相可访问，跨 OS-App 访问须配置授权（SWS_Os_00448, p.62）。
- Trusted（可特权运行、保护可关）vs Non-trusted（受限访问、timing 被强制）（SWS_Os_00446, p.59/61）。状态 `APPLICATION_ACCESSIBLE/TERMINATED`；`TerminateApplication`（SWS_Os_00258, p.194）终止其所有 Task/ISR、关中断、停 alarm 与 schedule table（SWS_Os_00447, p.62）。
- "partition" 一词在 OS SWS 里不是独立对象；RTE/BswM 把 OsApplication 对应的运行单元称 partition（RTE p.82：inter-partition = 跨保护边界；BswM p.39）。
- 保护设施：Memory Protection（MPU，SC3/4）、Timing Protection（SC2/4，需高优先级 timer 中断）、Service Protection（SC3/4）、`CallTrustedFunction`（SWS_Os_00097 p.172）、ProtectionHook（SWS_Os_00538, p.234：返回 PRO_IGNORE/PRO_TERMINATETASKISR/PRO_TERMINATEAPPL/PRO_SHUTDOWN；SC2/3/4 可用，SWS_Os_00542）。

### 3.4 Scalability Classes（SWS_Os_00241, Os R25-11 p.133）

| 特性 | SC1 | SC2 | SC3 | SC4 |
|---|---|---|---|---|
| OSEK OS、Counter/SWFRT、ScheduleTable、Stack monitoring | 是 | 是 | 是 | 是 |
| ProtectionHook | - | 是 | 是 | 是 |
| Timing protection；Global time / 同步 | - | 是 | - | 是 |
| Memory protection(MPU)、Service protection、CallTrustedFunction | - | - | 是 | 是 |
| OS-Application | 脚注：见 SWS_Os_00764 | 同 | 是 | 是 |

最低对象数量见表 7.4（p.132）；SC3/SC4 总是 extended status（SWS_Os_00327, p.133）。

### 3.5 Hook

| Hook | 规则 | 依据 |
|---|---|---|
| `StartupHook`（系统级 OSEK）+ `StartupHook_<App>` | 系统级先于应用级；应用级以所属 OS-App 权限运行 | SWS_Os_00060/00226/00236, p.134；接口 SWS_Os_00539 p.235 |
| `ShutdownHook` + `ShutdownHook_<App>` | 应用级先于系统级 | SWS_Os_00112/00225/00237, p.134-135；SWS_Os_00541 p.236 |
| `ErrorHook` + `ErrorHook_<App>` | 应用级在系统级之后；返回 StatusType 的服务才触发（ActivateTaskAsyn/SetEventAsyn 例外） | SWS_Os_00246/00085/00367, p.135；SWS_Os_00540 |
| `ProtectionHook` | 严重错误（超 WCET、违反内存保护）时调用 | SWS_Os_00538, p.234 |

EcuM 对 hook 的用法：ShutdownHook 里调 `EcuM_Shutdown`（EcuM p.49）；StartupHook 之后由 autostart task 调 `EcuM_StartupTwo`（EcuM p.37）。**RTE SWS 不标准化任何 OS hook 的使用**（Rte R25-11 p.141）。

---

## 4. RTE（Runtime Environment）+ BSW Scheduler (SchM)

### 4.1 RTE 与 OS 的总体关系（核心结论）

1. OS 在架构上对 SW-C 是不可见的；RTE 用 OS 做两件事：跨 core/partition 传信号、**调度 runnable——RTE Generator 生成 Task/ISR2 的 body，body 里按配置调用各 runnable**（Rte R25-11 p.133）。
2. **谁写 `TASK(...)` body：RTE Generator 生成**（SWS_Rte_06200：为含 RunnableEntity 的 task 构造 task body；SWS_Rte_04560：为含 runnable 的 ISR2 构造 ISR body；SWS_Rte_06201/04561：BSW Schedulable Entity 同理，p.134-135）。生成的 task body 用 `OS_START_SEC_<sadm>` / `Os_MemMap.h` 包起来（SWS_Rte_04557 p.135；ISR2 SWS_Rte_04562）；OsTask/OsIsr/OsApplication 必须引用 SwAddrMethod，否则拒绝（SWS_Rte_08923, p.137）。**runnable 本身与 OS 无关，不允许直接调 OS 服务**，要经 RTE API / BSW Scheduler API（Rte p.139）。
3. **runnable -> task 的映射不是 RTE Generator 决定，是输入**（RTE-Configuration 阶段由集成者做）；RTE Generator 不创建 task/ISR，只生成 body；"RTE configurator 必须在 OS 配置中分配所需 task"（Rte p.135, p.139-140）。例外：`strictConfigurationCheck`=false 时，RTE Generator 可能补建/修改 OS 配置里缺的 OsTask（SWS_Rte_05150, p.111，示例 1）。
4. RTE 只能使用 AUTOSAR OS、COM、Efficient COM for Large Data、Transformer、NvM；BSW Scheduler 只能使用 OS（SWS_Rte_02250/07519, p.134）——"BSW Scheduler 不是与 OS 调度器竞争的实体"（p.134）。
5. Os SWS 的对应表述：配置把 SW-C 的 runnable 映射到一个或多个 Task；同一 Task 内所有 runnable 共享同一保护边界；**SW-C 不能含中断处理函数，只能实现为 Task（或一组 Task）里的 runnable**（Os R25-11 p.23 §4.3）。

RTE 使用的 OS 对象（Rte R25-11 p.139-141 §4.2.2.1）：

| OS 对象 | RTE/SchM 怎么用 | RTE 配置里谁给 |
|---|---|---|
| Task | 控制激活/恢复：直接调 `ActivateTask`/`SetEvent`，或间接用 alarm/schedule table；task 终止时生成体内含 `TerminateTask()`/`ChainTask()` | OsTask 由集成者在 OS 配置里建，`RteMappedToTaskRef` 引用 |
| ISR2 | 生成 ISR2 body 内调 runnable；ISR 由 OS 配置、RTE 不创建 | `RteEventToIsrMapping` |
| Event | 实现抽象 RTEEvent/BswEvent；可调 `SetEvent/WaitEvent/GetEvent/ClearEvent` | `RteUsedOsEventRef`（ECUC_Rte_09025） |
| Resource | 数据一致性（exclusive area）；`GetResource/ReleaseResource` | `RteExclusiveAreaOsResourceRef`（ECUC_Rte_09031）；机制枚举 NONE/ALL_INTERRUPT_BLOCKING/OS_INTERRUPT_BLOCKING/OS_RESOURCE/OS_SPINLOCK/RTE_PLUGIN（ECUC_Rte_09029, p.1180；SWS_Rte_CONSTR_03510 对 OS_SPINLOCK 有限制） |
| 中断开关 | 另一种一致性手段，必须用 OS 提供的服务，不得用编译器/处理器相关函数 | — |
| Alarm | `SetRelAlarm/SetAbsAlarm/CancelAlarm/GetAlarm`：周期触发 TimingEvent；异步 C/S 调用超时 | `RteUsedOsAlarmRef`（ECUC_Rte_09024） |
| ScheduleTable | 周期触发 runnable | `RteUsedOsSchTblExpiryPointRef`（ECUC_Rte_09026） |
| Spinlock / IOC | 多核 exclusive area / inter-partition 通信 | p.372-374 |
| Hook | RTE SWS 不标准化 | p.141 |

### 4.2 RTEEvent 列表与 runnable 的激活

R25-11 的 RTEEvent 全集（Rte R25-11 p.142-143 表 4.1）：

| 缩写 | 事件名 | 备注 |
|---|---|---|
| T | TimingEvent | 周期，见 4.4 |
| BG | BackgroundEvent | 最低优先级、无固定周期 |
| DR | DataReceivedEvent | 仅 S/R |
| DRE | DataReceiveErrorEvent | 仅 S/R |
| DSC | DataSendCompletedEvent | 仅 explicit S/R |
| DWC | DataWriteCompletedEvent | 仅 implicit S/R |
| OI | OperationInvokedEvent | 仅 C/S（server runnable） |
| ASCR | AsynchronousServerCallReturnsEvent | 仅 C/S 异步 |
| MS | **SwcModeSwitchEvent**（R25-11 命名；activation=onEntry/onExit/onTransition） | mode |
| MSA | ModeSwitchedAckEvent | mode |
| MME | SwcModeManagerErrorEvent | mode |
| ETO / ITO | ExternalTriggerOccurredEvent / InternalTriggerOccurredEvent | trigger |
| I | InitEvent | 见 4.4 |
| THE | TransformerHardErrorEvent | |
| TEE | OsTaskExecutionEvent | 随 OS task 执行到该位置而激活；有 AUTOSAR 接口访问权（与 VFB tracing 的 `Rte_Task_Dispatch` 不同）；主要用途：CDD 对 task 执行作出反应（SWS_Rte_82006, p.162；p.804） |

- 所有事件都可 ACT（激活 runnable）；仅 DR、DSC、MSA、ASCR 可 WUP（唤醒 WaitPoint）（p.143 表 4.2）。**激活 runnable 不需要 runnable 里调用任何 RTE API，由 RTE 按 SWC 描述实现**；WaitPoint 要在 runnable 里显式调：`Rte_Receive`（DR）、`Rte_Feedback`（DSC）、`Rte_SwitchAck`（MSA）、`Rte_Result`（ASCR）（p.143-144）。
- 若某 runnable 没有任何 RTEEvent 以 startOnEvent 引用，RTE 永远不会激活它（p.142）。
- DataReceivedEvent 激活但没有 WaitPoint 引用时：数据到达即激活 runnable（SWS_Rte_01292, p.144），但读数据仍需 `Rte_Read`/`Rte_IRead`。
- 只有 server runnable（OperationInvokedEvent）会排队；其他 ExecutableEntity 不排队；`minimumStartInterval` 可延迟激活（p.165）。ExecutableEntity 的激活/执行实例状态机（suspended/to be started/running/waiting/preempted；debounce activation/activated）见 SWS_Rte_07707（p.167-168）。
- 同一 runnable 可被多个 RTEEvent 触发；可查询是哪个事件触发的（Provide activating RTE event：SWS_Rte_08051-08054, p.177-178）。
- **直接函数调用**：server、triggered、on-entry/transition/exit、ModeSwitchAck 这类 ExecutableEntity 可被 RTE 实现成调用者上下文里的 direct/trusted function call，不进 task（p.171-173；SWS_Rte_06798-06800, p.1161-1162）；此时仍要提供 RteEventToTaskMapping（不带 RteMappedToTaskRef）表示"已考虑"（SWS_Rte_06798）。

### 4.3 映射配置：RteEventToTaskMapping / RtePositionInTask / RteActivationOffset

`RteEventToTaskMapping`（ECUC_Rte_09020，父容器 `RteSwComponentInstance`，ECUC_Rte_09005；Rte R25-11 p.183, p.1156-1158）：把一个 RunnableEntity 的实例按触发它的 RTEEvent 映射到一个 OsTask。

| 参数 | ECUC ID | 含义 |
|---|---|---|
| `RteEventRef` | ECUC_Rte_09019 | 被映射的 RTEEvent（1..*；多个 ref 只允许全是指向同一 server 的 OperationInvokedEvent，CONSTR_08613） |
| `RteMappedToTaskRef` | ECUC_Rte_09021 | 目标 OsTask；`RteEventIsMappedToTask`=TRUE 时必须引用 OsTask，否则必须不引用（ECUC_Rte_09221；CONSTR_08935/08936, p.1169） |
| `RtePositionInTask` | ECUC_Rte_09023 | task 内的评估/执行顺序，0..65535；同一 task（或同一 direct call 范围）内必须唯一（SWS_Rte_CONSTR_09082, p.1162） |
| `RteActivationOffset` | ECUC_Rte_09018 | 激活偏移，秒；仅用于时间触发的 runnable；task 周期取所有 runnable 周期与偏移的 GCD，**WCET 须小于该 GCD**（SWS_Rte_07000/07520, CONSTR_09010, p.175-176） |
| `RteOsSchedulePoint` | ECUC_Rte_09022 | NONE/CONDITIONAL/UNCONDITIONAL：在 runnable 后生成对 `Schedule` 的调用（非抢占调度）（SWS_Rte_05113-05115, p.1163-1164） |
| `RteUsedOsEventRef` / `RteUsedOsAlarmRef` / `RteUsedOsSchTblExpiryPointRef` | 09025 / 09024 / 09026 | 指定用哪个 OS 对象实现该事件；alarm 与 schedule table expiry point 不能同时配（SWS_Rte_07808, p.1166）；只能用于 TimingEvent/BackgroundEvent（SWS_Rte_07809） |
| `RteVirtuallyMappedToTaskRef` | ECUC_Rte_09027 | 时间保护用：在 virtual task 里评估事件，但在 `RteMappedToTaskRef` task 里执行（SWS_Rte_07801-07803, p.1164-1165） |
| `RteServerQueueLength`、`RteQueuedReceiverQueueLength` 等 | 09133 / 09227 | |

补充约束：
- 同一 RTEEvent 不可被多个 mapping 引用（SWS_Rte_07843, p.1159）；缺失映射信息拒绝生成（SWS_Rte_02254, 08417, p.137）；映射缺失不是总是错：用 direct/trusted call 的 runnable 不需要 task 映射（SWS_Rte_02254 注）。
- `canBeInvokedConcurrently`=false（或 BSW `isReentrant`=false）的 ExecutableEntity，其事件不得映射到可互相抢占的不同 task/ISR2（SWS_Rte_05083, p.1167）。
- 用于 wake-up WaitPoint 的 OsEvent 不应计入激活映射（SWS_Rte_05082, p.1167）。
- Category 2 runnable（含 WaitPoint）**必须映射 extended task**（p.147 场景总述；场景 4 p.149-150），不可映射到 ISR2（SWS_Rte_CONSTR_09120, p.183）。cat 1 runnable 可映射 basic task（同周期或同优先序）或 extended task（p.147-148，场景 1：extended task 时不终止，靠不同 OS Event/alarm 区分入口，需 `RteUsedOsEventRef`）。
- **Basic/Extended 的判定**：task 类型由 OsTask 是否关联 OS Event 决定（p.148）。
- `SWS_Rte_82005`：在 Cluster Generation Phase，若 runnable 映射需要 extended task 则拒绝（该条属 Application Software Cluster 场景，p.134，不要泛化为"RTE 不支持 extended task"）。
- ISR2 映射：`RteEventToIsrMapping`（`RtePositionInIsr`）；TimingEvent 映射到 ISR2 要求 `OsIsrPeriod` 且周期为其整数倍（SWS_Rte_04563, CONSTR_09122, p.159）；**category 1 中断不得访问 RTE**（SWS_Rte_CONSTR_09012, p.183）。
- TimingEvent：按有效周期激活（SWS_Rte_06728-06730, p.158）；映射到 OsTask 时周期须是 `OsTaskPeriod` 整数倍，strict 模式下 OsTaskPeriod 必须存在（CONSTR_09123/09124, p.159）；相同时基同步靠共用 alarm/expiry point 或用绝对 offset 启动（SWS_Rte_07804/07805, p.160-161），`RteUsedOsActivation` 容器（ECUC_Rte_09060）含 `RteExpectedActivationOffset`（09048）、`RteExpectedTickDuration`（09049）——这是 RTE 对 OS/MCU 设置的**要求**，RTE Generator 视为已满足（p.1171-1172）。
- BackgroundEvent：要么映射到真正的后台 task（该核优先级最低、每核仅一个，SWS_Rte_07181, p.1166-1167），要么像 TimingEvent 一样用 alarm/expiry point 周期触发（SWS_Rte_07179/07180, p.161）；basic task 实现时终止后立即重激活（如 `ChainTask`）（p.161）。
- 任务链：`RteOsTaskChain`（`RtePredecessorOsTaskRef`/`RteSuccessorOsTaskRef`），RTE 在 task body 末尾生成 `ChainTask`；被触发的是链首 task（SWS_Rte_04558/04559, p.137-138）。

### 4.4 BSW Scheduler（SchM）与 BswEvent 映射

- BswSchedulableEntity = 带 MainFunction 的 BSW 入口，由 BswEvent 触发（BswTimingEvent、BswBackgroundEvent、BswDataReceivedEvent、BswOperationInvokedEvent、BswModeSwitchEvent、Bsw(External/Internal)TriggerOccurredEvent 等，图 4.11 p.145）；**BswEvent 不支持 WaitPoint**（p.146）。未映射到 runnable 的 BswSchedulableEntity 不允许进入 wait 状态，相当于 cat 1 runnable；可映射到 basic task、extended task、cat 2 ISR（p.141-142）。
- `RteBswEventToTaskMapping`（p.1211-1218）：容器在 `RteBswModuleInstance`（ECUC_Rte_09001/09066 等）下；参数 `RteBswEventRef`（09064）、`RteBswMappedToTaskRef`（09067）、`RteBswPositionInTask`（09068）、`RteBswActivationOffset`（09063）、`RteBswEventPeriod`（覆盖 `BswTimingEvent.period`，SWS_Rte_02323, p.159）。SWS_Rte_07520：SchM 须遵守 task 内 BSW entity 的 activation offset（p.175）。"BSW MainFunction 放进哪个 task"就是这里的配置；**BSW MainFunction 与 SWC runnable 可以映射在同一个 OsTask 中，靠 position 排序**（例：Example 8.2 中同一 OsTask 内按 position 依次放 BswA_ProcessBigBang 与 SwcA/SwcB 的 runnable，Rte p.1162-1163；该示例无独立 SWS ID，并依据 SWS_Rte_CONSTR_09082 的 position 唯一性）。
- SchM 生命周期 API（RTE R25-11 p.926-929）：`SchM_Init`（SWS_Rte_91170，ID 0x00）、`SchM_Start`（SWS_Rte_91171，ID 0x70）、`SchM_StartTiming`（SWS_Rte_91172）、`SchM_Deinit`（SWS_Rte_91173）。均由 EcuM 在每个核上各调一次（SWS_Rte_CONSTR_09055/09057）。`SchM_Init` 之前只有 `SchM_Enter/Exit` 可用（SWS_Rte_07578, p.459）；`SchM_StartTiming` 启动 BswTimingEvent 触发的 entity（SWS_Rte_07574）；`SchM_Init` 启动 BswBackgroundEvent（SWS_Rte_07584, p.460）。

### 4.5 Rte_Start / Rte_Stop / 谁来调

| API | 事实 | ID / 页 |
|---|---|---|
| `Rte_Start` | 返回 `Std_ReturnType`（RTE_E_OK/RTE_E_LIMIT），ID 0x70，Non-reentrant；分配并初始化 RTE 的系统/通信资源；**只能调一次、从 trusted OS 上下文、每个核各调一次、在 OS/COM/memory services 初始化后、在 `SchM_Init` 之后**；不得由 SW-C 调用；须有限时间内返回 | SWS_Rte_91137, p.805；CONSTR_09035/09036/09037, p.805-806；SWS_Rte_02585 |
| `Rte_Stop` | ID 0x71；释放该核上 RTE 资源；**在 OS/COM/memory services 关闭前调用**；RTE 在 Rte_Stop 后不再激活/启动 runnable（但不 kill 正在运行的 task）；停止后忽略来的 S/R 与 C/S 请求 | SWS_Rte_91138, p.807；CONSTR_09038 p.807；SWS_Rte_02538/02535/02536, p.461-462 |
| `Rte_Init_<InitContainer>` | 调用映射在 `RteInitializationRunnableBatch` 中的初始化 runnable；仅能在 `Rte_Start` 之后用 | SWS_Rte_91142, p.808 |
| `Rte_StartTiming` | 释放 TimingEvent/BackgroundEvent 触发的 runnable 的激活；若存在 Rte_Init 则必有；无此 API 时由 `Rte_Start` 承担（SWS_Rte_07575/07178） | SWS_Rte_91143, p.810；SWS_Rte_06759/06760, p.461 |

- RTE 与 SchM 生命周期嵌套：先 `SchM_Init` 再 `Rte_Start`；`Rte_Stop` 后才 `SchM_Deinit`（SWS_Rte_CONSTR_09036/09056, p.458-459 图 4.62）。SchM 须阻止 RTE 初始化之前激活 runnable（SWS_Rte_07580, p.460）。
- InitEvent 激活的 runnable 在 `Rte_Start` 执行时激活一次（SWS_Rte_06761, p.161）：可映射到 OsTask（task 活跃时执行，顺序由 RtePositionInTask 决定）或 `RteInitializationRunnableBatch`（在 `Rte_Init_*` 里执行）（p.161-162）。SW-C 的初始化/结束也可用进入初始 mode 的 SwcModeSwitchEvent（p.462，SWS_Rte_02562/02503）。RTE 初始化 mode machine instance，触发初始 mode 的 on-entry runnable（SWS_Rte_02544, p.459-460）。
- `Rte_Start` 说明里"SW-C 初始化发生在 Rte_Start 返回后，由进入 run 状态的 mode change 事件触发"（p.806 §5.8.1.5）；EcuM 把 EcuM_Mode 初始值 STARTUP 标为 "Set by Rte when Rte_Start() has been called"（EcuM p.100）。
- **谁调 `Rte_Start`（需谨慎表述）**：RTE SWS 写的是 "by the EcuStateManager"（CONSTR_09035 p.805；p.460：EcuM 在 startup phase II 末尾调）。R25-11 的 flexible 设计下，EcuM 的 StartPostOS 并不调 `Rte_Start`（EcuM p.41-42 无此步骤），而是**由 BswM 的 `BswMRteStart` action 调**（BswM ECUC_BswM_01073 p.169；EcuM p.33）。这是 RTE SWS 措辞沿用旧版而 EcuM/BswM SWS 已更新的**文档间不一致**——教程应按 EcuM/BswM（flexible）写，并说明老工程/厂商实现可能由 EcuM 或集成代码直接调。

### 4.6 Contract phase 与 Generation phase

- **RTE Contract Phase**：由 SW-C Type 描述 + Internal Behavior（runnable、RTEEvent 的定义）生成 **Component API（application header 文件）**，让 SW-C 开发者不依赖最终通信位置而编写代码；编译后可生成 Implementation Description（Rte R25-11 p.93-95 §3.1.1）。BSW Scheduler 有对应的 "BSW Scheduler Contract Phase"（p.95）；还有 PreBuild Data Set Contract Phase（p.95）。
- **Generation Phase**：ECU 配置完成后生成实际 RTE 代码（含 task body、`Rte.c`、与 OS/COM 的粘合），输入含 ECU Extract、RTE 配置（映射到 OsTask 等）与 OS/COM 配置（p.97-99 §3.4）；还有 BSW Scheduler Generation Phase（SWS_Rte_07569, p.97）、Cluster Generation Phase（SWS_Rte_82000, p.100）、PostBuild Data Set Generation（p.109）。RTE Generator 把配置错误视为非法配置（SWS_Rte_05149, p.111）；`strictConfigurationCheck`=true 时不得创建或修改任何输入配置（SWS_Rte_05150, p.111）。

### 4.7 RTE API 语义：隐式 / 显式、port、IRV、PIM、Param

（签名 ID / 存在性 ID 来自 Rte R25-11 §5.6，p.696-787；以下"创建条件"以 SWS 的 Existence 规则为准。）

| API | 语义 | 签名 / 存在性 ID，页 |
|---|---|---|
| `Rte_Write_<p>_<o>` | **explicit** S/R 发送：调用点即发送；by value/reference 由 ImplementationDataType 决定 | 01071 / 01280, p.698-699 |
| `Rte_Send_…` | explicit，用于 queued（event 语义）发送 | 01072 / 01281, p.703 |
| `Rte_Read_…` | explicit，**非阻塞**读 data 语义；Rte_Read 无阻塞版本 | 01091 / 01289, p.719 |
| `Rte_Receive_…` | explicit，queued/event；非阻塞 01288，**阻塞版（对应 WaitPoint）01290**，阻塞版只能用于 cat 2（需 extended task） | 01092 / 01288, 01290, p.726 |
| `Rte_IRead_…` | **implicit** 读：runnable 启动时拷贝一份，整个运行期不变（`dataReadAccess`）；runnable 必须终止才算"看到更新"，cat 2 永不终止则看不到新数据；不适用于 queued | 03741 / 01301, p.746；语义 p.292 §4.3.1.5.1 |
| `Rte_IWrite_…` | **implicit** 写：runnable 终止后才发送（`dataWriteAccess`）；多次写取最后一次（last-is-best） | 03744 / 01302, p.749；语义 p.292 |
| `Rte_IStatus` / `Rte_IInvalidate` | implicit 的状态/失效 | p.752-754 |
| `Rte_Call_…` | C/S 调用：同步版 01293；异步版 01294（配合 `Rte_Result` 取结果，ASCR 事件唤醒） | 01102 / 01293, 01294, p.731；`Rte_Result` 01111 / 01296, p.737 |
| `Rte_Mode_…` | 读当前 mode（mode user） | 02628 / 02629, p.768 |
| `Rte_Switch_…` | mode manager 切换 mode（异步通知；mode queue 满时丢弃并返回错误 SWS_Rte_02720/02675, p.323；ack 用 `Rte_SwitchAck`） | 02631 / 02632, p.707 |
| `Rte_Pim_…` | per-instance memory（SW-C 实例私有状态） | 01118 / 01299, p.742（该页提取文本与 CData 相邻，ID 请对照 PDF 复核） |
| `Rte_CData_…` | calibration data（ParameterDataPrototype，RAM/ROM 视配置） | 01252 / 01300, p.743 |
| `Rte_Prm_…` | port 上的 parameter；原始类型返回值，复合类型返回 const 指针 | 03928 / 03929, p.745（返回值规则 SWS_Rte_03930） |
| `Rte_IrvRead/IrvWrite`（explicit）、`Rte_IrvIRead/IrvIWrite`（implicit） | Inter-Runnable Variable | 03560 / 01305, p.763；p.758-761 |
| `Rte_Enter_<ea>` / `Rte_Exit_<ea>` | Exclusive Area 临界区 | 01120 / 01307；01123 / 01308, p.766-767 |
| `Rte_Trigger`、`Rte_IsUpdated`、`Rte_IsAvailable` 等 | trigger / 更新标志 / 可选元素 | 07200 / 07201, p.773 等 |

- 隐式 vs 显式总则：SWS_Rte_06011（p.291）。implicit = "runnable 不主动发起收发，数据在启动时自动取到、在终止时自动发出"；explicit = runnable 调 API，按 runnable 类别与端口配置可阻塞或非阻塞（p.291-292）。数据一致性：implicit 缓冲/拷贝由 RTE 保证；若数据可能被其他 runnable 修改则拷贝（p.292）。
- **Inter-ECU**：S/R 数据元素经 DataMapping 映射到 COM signal/signal group，RTE 通过 COM 发送；本地通信可用 COM 或 RTE 自己的直接实现（SWS_Rte_04504, p.323）；COM 的回调由 RTE 生成：`Rte_[<Partition>_]COMCbk…`（SWS_Rte_91123/91124, p.813-814；以及 TAck/TErr/Inv/RxTOut 变体 p.814-817）。SW-C 对 COM 不可见（p.133）。inter-partition 用 IOC 或 BSW Scheduler 路径（p.372-374）。
- **Exclusive Area**：ExclusiveArea 是"工作模型"，实现机制由 `RteExclusiveAreaImplementation` 配置（p.201-202 §4.2.6.5；ECUC 见上表）。
- **OS 的参与**：RTE 用 OS event/alarm/resource/spinlock/schedule table 实现上述语义（见 4.1 表）。

---

## 5. 简明答案（供 Part XI 使用）

### (a) EcuM 接管后做什么？
先澄清措辞：是"EcuM 先于 OS，OS 启动后把 UP 阶段交给 BswM"。
1. **OS 之前**：`EcuM_Init` 依次 SetProgrammableInterrupts -> DriverInitZero -> DeterminePbConfiguration + 一致性检查 -> DriverInitOne -> reset reason -> default shutdown target -> LoopDetection -> `StartOS`，永不返回（EcuM R25-11 p.37-41, SWS_EcuM_02411/02811）。
2. **OS 起来之后**：StartupHook -> autostart task 调 `EcuM_StartupTwo`（SWS_EcuM_02838/02806, p.112-113）-> `SchM_Start` -> `BswM_Init` -> `SchM_Init` -> `SchM_StartTiming`（SWS_EcuM_02934/02932, p.41-42）。到此 UP 阶段开始，EcuM "被动"，BswM 通过 action list 驱动其余 BSW 初始化（`EcuM_AL_DriverInitBswM_*`、NvM_ReadAll、Com/Dem/FIM 初始化、`Rte_Start`）（EcuM p.33；BswM p.150, p.169）。
3. **EcuM 仍然拥有**：`EcuM_MainFunction`（wakeup validation）、wakeup 源状态管理与向 BswM/ComM 的通知、RUN/POST_RUN 请求仲裁（`EcuM_RequestRUN` 等）并向 BswM 汇报、`EcuM_SetState` 反映 BswM 的状态切换并经 RTE mode port 通知 SW-C、SLEEP 序列（GoSleep/Halt/Poll/WakeupRestart）、SHUTDOWN 序列（OffPreOS -> ShutdownOS -> ShutdownHook -> `EcuM_Shutdown` -> OffPostOS -> Reset/SwitchOff）（EcuM p.47-60, p.98-99, p.150）。
4. 版本提醒：R25-11 只有 flexible EcuM；旧 fixed EcuM（`EcuM_GoDown/Halt/Poll`、固定 STARTUP I/II、RUN 状态）已移除（EcuM p.2, p.15）。

### (b) RTE 与 OS 是什么关系？
- RTE 是**为特定 ECU 生成**的代码，对 SW-C 提供与 VFB 相同的接口，**底层使用** OS 与 COM；OS 对 SW-C 不可见（Rte p.133-134）。RTE 只能使用 OS/COM/Transformer/NvM（SWS_Rte_02250），SchM 只能使用 OS（SWS_Rte_07519）。
- 分工：**OS 提供调度原语**（task、ISR、event、alarm、schedule table、resource、spinlock、IOC）；**RTE Generator 生成 task/ISR2 的 body**（SWS_Rte_06200/04560），body 里按顺序调 runnable，并在 RTE API 内部调 `ActivateTask/SetEvent/WaitEvent/GetResource…`；**OS task 本身（OsTask、优先级、autostart、OsEvent、OsAlarm、OsScheduleTable）由集成者在 OS 配置里创建**，RTE 不创建（Rte p.139；例外见 SWS_Rte_05150）。**runnable 到 task 的映射是配置输入，不是 RTE Generator 决定的**（Rte p.135, p.139）。
- RTE Start/Stop 与 SchM 生命周期嵌套在 OS 运行期内：先 `SchM_Init`、再 `Rte_Start`；关机时先 `Rte_Stop`、再 `SchM_Deinit`、再 `ShutdownOS`（Rte p.458-462；EcuM p.48-50）。RTE 不标准化 OS hook 的使用（Rte p.141）。

### (c) runnable 绑定 RTE event，那 OS task 做什么？
- **绑定关系有两层**：SWC 描述里 "RTEEvent --startOnEvent--> Runnable"（语义层，Rte p.142）；ECU 配置里 "RteEventToTaskMapping: RTEEvent -> OsTask + RtePositionInTask (+ RteActivationOffset / OsEvent / OsAlarm / ScheduleTable ExpiryPoint)"（物理层，ECUC_Rte_09020, Rte p.183, p.1156-1158）。
- **OS task 的作用**是给 runnable 提供"执行上下文（优先级/栈/保护边界）+ 激活机制"。task body 由 RTE 生成：被激活（TimingEvent 靠 alarm/schedule table 周期 `ActivateTask`/`SetEvent`；DataReceivedEvent 靠发送端 `Rte_Write/Send` 内部 `ActivateTask`/`SetEvent`；OperationInvokedEvent 靠 client 的 `Rte_Call`；ModeSwitchEvent 靠 `Rte_Switch`；InitEvent 靠 `Rte_Start`）后，依次按 `RtePositionInTask` 评估各事件是否发生并调相应 runnable，随后终止（basic task：`TerminateTask`/`ChainTask`）或等待下一个 OsEvent（extended task：`WaitEvent`，用于多周期共 task 或含 WaitPoint 的 cat 2 runnable）（Rte p.139-141, p.147-150, p.137-138；示意代码见 p.157-158）。
- BSW MainFunction（`Dem_MainFunction`、`Com_MainFunctionRx`、`BswM_MainFunction`、`EcuM_MainFunction` 等）同理：通过 `BswTimingEvent` + `RteBswEventToTaskMapping` 放进 task，由 SchM 调度（Rte p.141-146, p.1211-1218）。**同一个 OsTask 可同时放 BSW MainFunction 与 SWC runnable**，用 position 排序。
- 注意：并非所有 runnable 都在 task 里：server / triggered / mode 的 entry-exit 等可被实现成 direct function call（Rte p.171-173）；cat 2 runnable 必须在 extended task，不能在 ISR2（SWS_Rte_CONSTR_09120）；category 1 ISR 不得访问 RTE（SWS_Rte_CONSTR_09012）。

---

## 6. 写作时的注意事项与未决点

1. **Fixed vs Flexible**：教程写 R25-11 版本时应明确"只有 flexible"；用户项目（RTA-CAR）实际行为需以其 EcuM/BswM 配置确认（本仓库无 RTA-CAR 资料）。
2. **`Rte_Start` 的调用者**：RTE SWS 写 EcuM；EcuM/BswM SWS 写 BswM action（`BswMRteStart`）。采用后者，并标注 RTE SWS 措辞未同步。
3. **EcuM SWS_EcuM_02181** 文字与 Figure 7.4 不一致（文字提 `EcuM_GetValidatedWakeupEvents`，图用 `EcuM_SelectShutdownTarget`），引用时以序列图/表 7.1 的"select default shutdown target"为准。
4. **Rte_Pim / Rte_CData 的 SWS ID**：文本抽取时相邻页顺序混杂，Pim 的 01118/01299 需对照 PDF 页 742-743 复核后再引用。
5. **Task 状态、ISR 分类细节**来自 OSEK，不是本 SWS；OS SWS 里只有 glossary 与保护章节涉及。
6. **EcuM_StartupTwo 在 R25-11 仍存在**（SWS_EcuM_02838，"implements the STARTUP II state"）；"Startup I/II"这套叫法是旧 fixed 版遗留，flexible 版改称 StartPreOS/StartPostOS 序列。
7. 未覆盖/未读：`AUTOSAR_CP_EXP_LayeredSoftwareArchitecture`（可能有 mode management 概览）、`AUTOSAR_CP_TR_SWCModelingGuide`、`AUTOSAR_CP_TPS_ECUConfiguration`（OsTask 等 ECUC 细节）、OS SWS 的 timing protection 与 multicore 细节、RTE mode machine（§4.4）与 NvBlock（§4.2.10）全文。
8. RTE SWS 的"场景 1-9"（p.147-153）自称仅为示例，不是规范（p.146-147）；教程引用时同样标"informative"。
