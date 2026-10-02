# [Legacy] RH850 + AUTOSAR 单页教程 v1（已拆分归档）

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件是 `docs/rh850-autosar-tutorial-content.md` 在重构为 [Master Index](../rh850-autosar-tutorial-content.md) 之前的**完整原文**（2026-10-02 归档）。**本仓库是个人学习用的教育项目**，文中“内部 agent / 交付”只是旧语境。
>
> - **已复审**：见 [研究笔记 01 §6](../reference/research/01-project-and-docs-review.md#6-rh850-autosar-tutorial-contentmd-章节大纲供重构为-master-index)（逐节去向表）。
> - **已知问题（本归档未修改）**：① 原 :231（本文件 :242）“本案例可为 OS 独占 OSTM0”与其他文档的“OSTM0=Gpt、OSTM1=OS 候选”不一致——这是**配置选择而非硬件事实**，见 [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md)、[03-mcal/05](../03-mcal/05-gpt-driver.md)；② 原 :333（本文件 :344）`return CAN_OK` 未标版本——R22-11 `Can_Write` 返回 `Std_ReturnType`（`E_OK`/`E_NOT_OK`/`CAN_BUSY`），`CAN_OK` 属早期规范的 `Can_ReturnType`，见 [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md)；③ `[Rnn](#ref-rNN)` 引用锚点在 Markdown 中**从未定义**（研究笔记 01 计为 24 处；逐链接计 33 个、21 个不同锚点）（原 HTML 构建的参考文献表），已知失效、刻意保留；④ 7 处 `<div id="...">` 是原 HTML 交互工具占位，在 Markdown 中不渲染任何内容。
> - **内容已迁移到**：§01→[00-learning-roadmap](../00-learning-roadmap.md)；§02→[01-rh850/01](../01-rh850/01-rh850-overview.md)、[02](../01-rh850/02-cpu-architecture.md)；§03→[02-autosar-classic/01](../02-autosar-classic/01-classic-platform-overview.md)、[02](../02-autosar-classic/02-layered-architecture.md)；§04→[研究笔记 03](../reference/research/03-openautosar-trace.md)、[09/01](../09-real-project-preparation/01-how-to-read-real-autosar-project.md)；§05→[01-rh850/04](../01-rh850/04-startup-process.md)、[02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md)；§06→[01-rh850/03](../01-rh850/03-memory-map.md)、[05](../01-rh850/05-linker-script.md)；§07→[03-mcal/01](../03-mcal/01-mcal-overview.md)；§08→[04-can-mcal/04](../04-can-mcal/04-can-pin-transceiver.md)、[03-mcal/03](../03-mcal/03-port-driver.md)、[04](../03-mcal/04-dio-driver.md)；§09→[02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md)、[07](../02-autosar-classic/07-mainfunction-scheduling.md)、[03-mcal/05](../03-mcal/05-gpt-driver.md)、[01-rh850/06](../01-rh850/06-interrupt-exception.md)；§10–§12→[04-can-mcal/02](../04-can-mcal/02-rh850-can-peripheral.md)、[03](../04-can-mcal/03-can-clock-bit-timing.md)、[05](../04-can-mcal/05-can-interrupt.md)、[06](../04-can-mcal/06-can-controller-init.md)、[07](../04-can-mcal/07-hoh-hrh-hth.md)、[09](../04-can-mcal/09-can-init-implementation.md)–[12](../04-can-mcal/12-can-interrupt-implementation.md)；§13→[05-can-stack/06](../05-can-stack/06-can-rx-path.md)、[07](../05-can-stack/07-can-tx-path.md)、[08-integration/05](../08-integration/05-uds-end-to-end.md)；§14（CAN FD）→[04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md)（位时间部分，其余未迁移）；§15→[02-autosar-classic/04](../02-autosar-classic/04-configuration-arxml.md)、[05](../02-autosar-classic/05-generated-code.md)、[04-can-mcal/08](../04-can-mcal/08-can-configuration.md)；§16→[08-integration/01](../08-integration/01-ecu-configuration-checklist.md)；§17→[04-can-mcal/15](../04-can-mcal/15-can-driver-debugging.md)、[调试手册](../debugging-autosar-diagnostics.md)；§18→[Part IX](../09-real-project-preparation/01-how-to-read-real-autosar-project.md)；§19–§20→[source-traceability](../reference/source-traceability.md)、[rh850-autosar-mapping](../reference/rh850-autosar-mapping.md)。
> - **相对原文的唯一改动**：在本 banner 之下逐字保留原文；仅把原 :523 的 4 个相对链接加上 `../` 以适应 `docs/legacy/` 位置。行号 = 原行号 + 11。

---

## 01 阅读路线与适用边界 {#scope}

本教程回答四个问题：RH850 如何执行程序；AUTOSAR Classic 的各层依赖什么；MCAL 与 CAN driver 具体要实现什么；怎样从配置推进到真实 CAN 诊断通信。贯穿案例是 **R7F701381 / RH850/P1M-E / 100 pin DPS**，不是整个 RH850 家族的通用寄存器模板。

建议先读架构与最小系统，再读启动、MCAL、CAN 驱动和诊断案例。已有 AUTOSAR 经验时，可直接从“参考项目移植差距”和“CAN driver 实现契约”开始。页内交互工具提供位时间计算、通信路径解释和寄存器检索；文末另嵌入详细硬件交接全文，离线打开也能查阅。

| 内容标记 | 含义 | 可以怎样使用 |
| --- | --- | --- |
| 手册事实 | 对照 P1M-E 手册得到的地址、位域和限制 | 核查对应器件与驱动实现 |
| 项目观察 | 本次读取 openAUTOSAR 实际源码得到的结论 | 评估移植差距，不等于代码已验证 |
| 教学设计 | 本文建议的分层、伪代码、样例 ID、阶段计划 | 按真实 OS/MCAL 版本和板卡适配 |
| 待绑定 | 原理图、收发器、网络配置、编译器或 OS ABI 才能确定 | 由内部 agent 从工程提取，不能猜值 |

三份用户资料均有用途：`r01ds0505…` 是 P1M-E 数据手册；`r01ds0506…` 是 P1x-C 数据手册；`r01uh0436…` 是旧 P1x 硬件手册。当前案例的寄存器依据是补充的 **R01UH0585EJ0120，P1M-E 硬件手册 Rev.1.20**。P1x-C 的 M-CAN 与本案例 RS-CANFD 不共用驱动寄存器布局。如果实际 MCU 改成 P1x-C，应另外建立器件后端。

本次交付是教学与移植指导，没有修改参考项目，没有提供已通过上板的完整 MCAL。源代码引用以本机仓库快照为准；官方 AUTOSAR 文档用于说明分层和契约，不能替换内部工程所采用版本的头文件。

## 02 RH850 架构：CPU、存储、总线与外设 {#architecture}

RH850 是一个 MCU 家族，G3M/G3MH/G4MH 等是不同 CPU 核系列。P1M-E 使用 G3M。做移植时必须同时知道“CPU 核”和“具体芯片”：核决定指令、异常和上下文模型；芯片决定 CAN 类型、地址、引脚、容量和时钟树。

### CPU 真正在保存什么

G3M 有 32 个 32 位通用寄存器 r0…r31 和 PC。r0 恒为0；r3 通常为 SP，r4 为 GP，r5 为 TP，r30 为 EP，r31 为 LP；r1 是汇编器保留用途，r2 可能由 OS 使用。具体调用约定还要与编译器 ABI 一致。程序能进入 `main()`，不代表这些 ABI 前提在中断、任务切换后仍成立。依据：HW-E pp.190–192。

CPU 系统寄存器通过 LDSR/STSR 以及 `(regID, selID)` 访问，与 `*(volatile uint32_t*)地址` 的外设 MMIO 不同。

| 系统寄存器 | 编号 `(regID,selID)` | 在移植中的用途 |
| --- | --- | --- |
| EIPC / EIPSW | (0,0) / (1,0) | EI 级异常保存的返回地址和状态 |
| FEPC / FEPSW | (2,0) / (3,0) | FE 级异常的返回信息 |
| PSW | (5,0) | CPU 状态、中断/特权相关控制 |
| EIIC / FEIC | (13,0) / (14,0) | 异常原因记录 |
| RBASE / EBASE | (2,1) / (3,1) | 复位/异常入口相关基址 |
| INTBP | (4,1) | 表参考方式的中断入口表基址 |
| FPSR | (6,0) | 浮点设置与状态，需要纳入浮点上下文策略 |

不要把这张表直接翻译成一段通用启动汇编。端口还要遵守异常入口同步、嵌套、寄存器初始化、栈对齐和编译器限制。HW-E p.256 特别指出部分异常入口有 SYNCP 要求，具体依赖 G3M 软件手册。

### 锁步、FPU、MPU 与 ECC 各自负责什么

- **锁步 checker**：跟随主 CPU 检查执行结果，不是一个可单独运行任务的第二应用核。此 P1M-E 教学配置使用一个应用核；不能因为宣传资料写“双核锁步”就配置两个 OsCore。
- **FPU**：P1M-E 支持浮点运算。G3M 的浮点操作使用通用寄存器及其配对，并非另一套独立浮点数据寄存器；同时存在 FPSR 等浮点系统状态。是否启用硬件浮点，要统一任务、ISR、库与 OS 的保存策略。HW-E p.213。
- **MPU**：控制 CPU 访问区域的权限；P1M-E 的 MPU 有16个区域。OS 使用内存保护时须为代码、栈、应用数据、内核与异常处理设置可达范围。MPU 不替代各总线 Guard。HW-E pp.214–220、260。
- **ECC/错误管理**：检测或纠正存储错误，错误可能被 ECM 转成中断或复位。首次使用 RAM 前必须有合法数据/ECC；读取未初始化的 CPU 寄存器再压栈也可能引起锁步比较问题。HW-E pp.250、2890。

### 字节序和寄存器访问宽度是两件事

G3M 数据采用 little-endian：字 `0x11223344` 在连续字节地址上依次为 `44 33 22 11`。依据 Renesas **R01US0123EJ0140 Rev.1.40 pp.23–24**，本次通过分销商托管的原厂 PDF 核实：[G3M 软件手册](https://www.mouser.com/pdfDocs/REN_r01us0123ej0140-rh850g3m_MAS_20180729.pdf)。CAN 信号的网络字节序是另一层协议配置，不能因此全部改成 Intel 格式。

即使 CPU 能装载一个32位数，也不意味着每个外设都允许32位访问。P1M-E 的 CAN TMC/TMSTS 必须8位，OSTM 时钟选择16位，CMP32位，Port 的 PINV/PODC/PDSC/PUCC32位。用同一个 `REG32()` 宏操作所有寄存器会破坏相邻控制位或产生非法访问。

<div class="architecture-map" aria-label="架构关系图"><div class="arch-top">应用 CPU：G3M 主核 + 锁步检查核<br><small>程序执行 · FPU · MPU · 异常上下文</small></div><div class="arch-bus">地址 / 数据总线与访问保护</div><div class="arch-grid"><div><b>存储</b><span>Code Flash · Data Flash<br>LRAM · GRAM · ECC</span></div><div><b>时间与控制</b><span>Clock · Reset · INTC<br>OSTM · WDTA</span></div><div><b>外设</b><span>RS-CANFD · PORT<br>ADC · SPI · PWM …</span></div></div><p>CPU 通过 MMIO 控制外设；中断将外设事件送回 CPU。OS 维护执行上下文，MCAL 管理对应硬件资源。</p></div>

## 03 AUTOSAR Classic 分层与最小可运行集合 {#layers}

AUTOSAR Classic 主要分为应用、RTE、BSW；BSW 中再有服务层、ECU 抽象层与 MCAL 等部分。RTE 为应用提供生成的接口；OS 调度任务和中断；MCAL 封装微控制器外设。**OS 不属于 MCAL；CAN driver 也不负责解释 UDS 服务。** 参考：[AUTOSAR Classic 平台说明](https://www.autosar.org/standards/classic-platform/)、[分层架构 R23-11](https://www.autosar.org/fileadmin/standards/R23-11/CP/AUTOSAR_CP_EXP_LayeredSoftwareArchitecture.pdf)。

<div id="layer-explorer" class="interactive"></div>

### 按运行目标决定启用模块

| 目标 | 需要形成的基本链路 | 此时不必为演示而全部启用 |
| --- | --- | --- |
| OS 周期任务运行 | 启动/链接/编译器适配、CPU/中断端口、系统 timer、任务/栈/Counter/Alarm | CAN、诊断、NvM、复杂 RTE |
| 原始 CAN 收发 | Mcu/时钟、Port、Can、所需 Dio/Spi/CanTrcv、异常日志 | CanTp、Dcm；可先用受控测试 harness |
| Classic 诊断演示 | OS/SchM、Can、CanIf、CanTp、PduR、Dcm、启动与通信状态控制 | COM/CanNm 不是每个纯诊断演示的数据必经层 |
| 完整 ECU 集成 | 按项目增加 EcuM/BswM/ComM/CanSM、Dem、WdgM/WdgIf/Wdg、NvM/Fee/Fls、RTE | 根据功能与平台配置裁剪，而非“所有模块都开启” |

Gpt 是通用计时驱动；某些 OS 使用专有 timer 端口，不经 Gpt。可使用同一外设类型，但不能让 Gpt 和 OS 同时拥有同一个 timer 实例。CanTrcv 管理外部 CAN 收发器，在分层中属于外部设备驱动范畴；它常依赖 Dio/Spi，不能误认为初始化 CAN 控制器就一定唤醒了收发器。

应用信号链通常为 `SWC → RTE → COM → PduR → CanIf → Can`；诊断请求链通常为 `Can → CanIf → CanTp → PduR → Dcm`。不要将诊断请求强行经过 COM。CanSM/ComM/BswM 主要管理通信状态，并非每个 payload 字节都必须穿过它们。

## 04 openAUTOSAR 实际提供了什么 {#reference}

本次只读检查 `D:\side_project\openAUTOSAR`。Git HEAD 为 `13499119e06e8c81f9470dc1e9b280c22cbfb0b3`；工作区有已有的未跟踪 `docs/` 和 `Autosar_SecOC/`，它们不能当作该提交自带的完整集成。以下讨论主要针对主线 `communication/`、`system/`、`boards/`、`include/`。

README 将仓库定位为教学平台，并明确说明没有可直接使用的硬件支持，部分模块可能只是 stub。源码也印证了这些边界。[R01](#ref-r01)

| 实际观察 | 对 RH850 移植意味着什么 | 代码证据 |
| --- | --- | --- |
| `boards/linuxOs/MCAL/Can` 只有配置头和 CMake 文件 | 没有可直接改寄存器就能复用的 Can.c，需要补目标驱动或接供应商 MCAL | [R02](#ref-r02) |
| `Mcu.c` 使用 SCB/RCC，Port/Gpt 使用 STM32 GPIO/TIM | 借鉴配置和错误处理结构；硬件后端必须替换 | [R03](#ref-r03)、[R04](#ref-r04)、[R05](#ref-r05) |
| `Platform_Types.h` 写 HIGH_BYTE_FIRST | 与本案例 G3M 的字节序不符，必须核对目标平台类型与所有序列化代码 | [R06](#ref-r06) |
| `Compiler.h` 没有清晰的 GHS 专用分支 | 对齐、weak、section、内联和汇编语法需适配；不能假定编译器兼容宏足够 | [R07](#ref-r07) |
| kernel 有任务/事件/Alarm/Counter 逻辑及 `Os_Arch*` 接口 | 可以学习 OS 结构；RH850 上下文、向量和异常返回端口仍要提供 | [R08](#ref-r08) |
| Can/CanIf 头声明 AR 3.1.5，包含旧 API | 不能与 RTA/MCAL 的4.x或更新接口直接拼接 | [R09](#ref-r09)、[R10](#ref-r10) |
| CanTp 的帧缓冲为8B，SF/FF/CF 按传统长度设计 | 本教程以 Classical 500k 作为复用案例；FD不是只把8改成64 | [R11](#ref-r11) |
| CanIf 默认配置为 STM32_F107，CanTp 默认文件是旧 MPC551x 示例 | 需要重建 Controller、HOH、PDU、回调与周期配置，不照搬默认值 | [R12](#ref-r12)、[R13](#ref-r13) |
| CanIf 对 CAN_BUSY 返回 E_NOT_OK，未实现 Tx buffering | 必须设计明确的忙状态处理，不能默认上层已排队 | [R14](#ref-r14) |
| 默认 bus-off 通知指针为 NULL | 即使驱动检测到 bus-off，也不代表 CanSM 会收到通知 | [R12](#ref-r12)、[R15](#ref-r15) |
| SchM_BswService 调模块 MainFunction；SchM_MainFunction 本体为空 | 调空函数不会推动 CanTp/DCM；应接入实际 BSW 任务及宏配置 | [R16](#ref-r16) |
| CMake 的交叉编译分支仍是占位，kernel 引用旧 arch/x64 路径 | 需建立 RH850 目标工具链、目标 include 与最终 ELF 链接，不能只生成静态库 | [R17](#ref-r17) |

### 两条实现路线，必须选清楚 OS 和接口所有者

**路线一：已有 RTA/供应商工程。** 保留匹配 RH850 的 OS 端口和 MCAL，在生成器里配置；openAUTOSAR 用来理解模块职责和调用关系。不要再链接它自己的 kernel，避免两个 OS 同时定义启动/ISR/调度对象。

**路线二：移植 openAUTOSAR 教学平台。** 新增 RH850 BSP、CPU/OS 端口、MCAL 后端和板级配置，并审查旧 BSW API。工作量包含汇编/链接/中断上下文，远大于实现一个 CAN 寄存器初始化函数。下文目录为建议，不表示已经创建或编译通过。

```text
boards/rh850_p1me/             # 建议新增的目标板层
  startup/                    # reset entry、异常入口、C runtime
  linker/                     # GHS 或选定编译器的链接文件
  config/                     # Mcu/Port/Can/Os/EcuM/CanIf/CanTp/PduR/Dcm
  mcal/{Mcu,Port,Dio,Gpt,Can,Wdg,Fls}/
arch/rh850_g3m/                # 仅在移植该开源 OS 时新增
  context/                    # task context、ISR、异常返回
  interrupt/                  # EIC / vector / critical section
  timer/                      # OSTM 与 OS counter 的契约
communication/ diagnostic/    # 上层逻辑，保留并逐项做版本适配
```

## 05 从复位到第一个任务：启动是怎样接起来的 {#startup}

先把启动过程分成硬件复位、汇编入口、C 运行时、板级初始化、OS 启动、BSW 服务就绪六段。每段都建立一个可观测检查点，例如调试器断点、RAM 日志序号或在 Port 初始化后使用 GPIO 标记。

| 阶段 | 必须完成 | 失败时观察 |
| --- | --- | --- |
| 复位入口 | 入口地址/镜像正确，选择目标器件启动包，按要求初始化 CPU 状态 | PC 是否到正确地址，复位是否反复发生 |
| 建立执行环境 | 按 ABI 设置 SP/GP/TP/EP 等；按锁步要求处理未定义寄存器 | 进入 C 后异常、首次压栈即错误 |
| 存储初始化 | RAM/ECC 条件成立，复制 `.data`、清应清的 `.bss`，保留区有明确复位策略 | 全局变量初值错、ECC/ECM 异常、栈损坏 |
| CPU/板级前置 | 时钟依据明确、向量合法、早期 WDTA 服务有时限、Port/收发器安全状态 | 超时、异常入口丢失、意外 watchdog reset |
| OS 初始化 | 配置核、任务/栈/资源、ISR、Counter/Alarm，完成真实端口初始化 | StartOS 后不调度、切换后寄存器损坏 |
| BSW 初始化及开放通信 | 初始化所有可能被回调的模块、配置状态管理、注册周期任务，再允许接收事件上送 | ISR 进入未初始化 CanIf/DCM、状态一直 offline |

openAUTOSAR 的 `EcuM_Init()` 在 DriverInitOne 后调用 `StartOS()`，后续 StartupTwo 使用 DriverInitTwo；这是该仓库的组织方式，不是所有供应商唯一的启动顺序。[R18](#ref-r18) DriverInitOne 的无限等待 PLL 模板必须在移植时改成适合 P1M-E 的状态依据和有界失败处理；DriverInitTwo 中的 CanTrcv 初始化仍留有 TODO。[R19](#ref-r19)

```c
/* 教学依赖顺序；不是任何特定版本的可直接编译 EcuM callout。 */
reset_entry:
    establish_cpu_state_and_stack_using_verified_port();
    establish_ram_ecc_and_c_runtime();
    capture_reset_reason_before_clear();
    init_board_clock_and_safe_pins();
    install_os_exception_and_interrupt_port();
    start_os_using_its_generated_configuration();

os_startup_context:
    initialize_bsw_and_driver_configuration();
    bind_real_rx_tx_error_callbacks();
    schedule_configured_bsw_mainfunctions();
    request_communication_via_project_mode_manager();
```

“初始化完成”和“允许通信”应分开。底层控制器可以完成 reset 内配置，但上层尚未准备好时不能开放会调用 CanIf 的中断。真实 EcuM 分阶段安排和状态管理以所选栈为准。

## 06 链接、平台类型、编译器与内存布局 {#memory}

R7F701381 的布局已按具体1MB型号核对：Code Flash `0x00000000–0x000FFFFF`；LRAM self `0xFEDE0000–0xFEDFFFFF`；GRAM A `0xFEEF8000–0xFEEFFFFF`；GRAM B `0xFEF00000–0xFEF07FFF`；Data Flash `0xFF200000–0xFF207FFF`。LRAM 的 `0xFEBE0000–0xFEBFFFFF` 是同一 RAM 的别名，不能算作另一块128KiB。依据 HW-E pp.257–258。

| 软件对象 | 常见放置设计 | 必须检查 |
| --- | --- | --- |
| reset/异常入口/中断表 | 链接到启动与 OS 要求的位置 | 对齐、入口模式、链接器不丢弃、表项真实地址 |
| `.text` / 常量 / 配置 | Code Flash | 总容量不是2MB；加载区与执行区关系正确 |
| `.data` / `.bss` | 已合法初始化的 RAM | 初始化表覆盖全部段；不可清掉需要保留的数据 |
| OS 任务栈/ISR 栈 | 由端口与链接配置决定 | 对齐、独占、最坏嵌套、填充水位和越界检测 |
| CAN/CanTp/DCM 软件缓冲 | RAM，生命周期明确 | 不与 CAN 外设内部 FIFO RAM 混淆；长度与并发匹配 |
| 持久数据 | NvM→Fee/Fls 等设计的 Data Flash 区 | 擦除粒度、异步作业、掉电一致性、保留块 |

`MemMap.h` 与链接文件是一对契约。把 section 宏定义成空可以暂时让 C 编译通过，却可能使中断表、no-init、代码/数据分区失去语义。针对 GHS 应检查 section pragma、weak/inline/对齐和汇编符号修饰；不要把 GCC attribute 全部原样复制。[R07](#ref-r07)、[R20](#ref-r20)

平台层至少验证 `sizeof(uint8/uint16/uint32)` 为1/2/4、指针及地址类型可容纳32位地址、布尔类型与 ABI 一致、字节序宏正确。处理 CAN payload 时显式提取字节，避免用未对齐的 `uint32*` 强转外部数据，避免用编译器相关 C 位域表达 MMIO 寄存器。`volatile` 只表达编译器访问约束，不能替代锁、I/O 顺序、总线完成等待或 W0C 语义。

P1M-E 的 CPU/外设时钟基线为160/80/40MHz，CAN 位时钟只从40/16MHz选。不能把参考项目的 STM32 RCC 初始化改个宏名字就保留；也不能凭其他 RH850 系列代码创造本器件不存在的 PLL0/PLL1 配置地址。

## 07 MCAL 要实现哪些模块 {#mcal}

以下是围绕“OS + CAN 诊断运行”的实现清单。API 名称表示职责，签名和可选开关以目标 AUTOSAR/供应商版本为准。

| 模块 | 实现职责与典型接口 | 关键配置 | 最小验证证据 |
| --- | --- | --- | --- |
| Mcu | 初始化配置、时钟/复位原因、RAM 段、允许的模式与复位请求；Mcu_Init/InitClock/GetResetReason | derivative、时钟源/频率引用、RAM 区、复位策略 | 实测时基、复位原因、初始化读回 |
| Port | 初始化复用/方向/电气属性；可选运行时方向/模式变更和刷新 | pin、ALT、方向、PIPC、pull、drive、可变更标志 | 实际管脚与读回一致；其他 pin 不被覆盖 |
| Dio | GPIO channel/port/group 读写，管理并发位更新 | STB/EN/LED 等通道映射、有效电平 | 上下电和切换时输出无意外脉冲 |
| Gpt | 定时器资源、起停、elapsed/remaining、通知、所需唤醒 | 通道类型、输入频率、预分频、周期、ISR | GPIO/仪器计时；边界、停止与回绕 |
| Can | 控制器状态、位时间、HOH、过滤、buffer、收发、完成与错误事件 | Controller、baudrate、Rx/Tx objects、poll/IRQ、FD能力 | 外部总线收发与正确完成回调 |
| Wdg | 初始化/允许的模式、触发预算到硬件服务、错误处理 | 选项字节、OVF、窗口、VAC、周期 | 正常服务不复位；停止合法服务触发预期处理 |
| Fls | 异步读/写/擦除/比较、busy/job结果、通知及底层库适配 | Flash 区、块/页、作业预算、库/执行位置 | 读写校验、边界与掉电场景，不阻塞全部 BSW |
| Spi（按板卡） | 同步/异步收发、通道/作业/序列、完成通知 | 外部智能收发器/PMIC 所需 SPI 配置 | 对端寄存器通信、电气/时序正确 |
| Adc / Pwm / Icu 等 | 按应用功能增加采样、输出和边沿测量 | 引脚、时钟、触发、量程、周期 | 与 CAN 诊断最小演示分阶段验收 |

### 每个驱动都要有的公共行为

1. 检查初始化状态、指针、索引、配置范围和资源冲突；开发错误按项目 DET 策略报告，运行错误按支持的版本处理。
2. 将配置视为稳定对象，明确哪些是编译期、链接期、post-build；运行期修改只能走硬件允许的状态。
3. 维护独立的软件状态，不以“寄存器写成功”代替所有状态转换完成；等待必须有超时与失败结果。
4. 区分 ISR、任务、DMA 的所有权；同一完成事件只能上报一次。回调可能触发新的请求，要避免持锁回调造成死锁或重入破坏。
5. 明确静态内存、缓冲大小和数据生命周期。默认不需要动态堆；上层传入的数据指针不应被驱动无限期保存并假定仍有效。
6. 不支持的硬件行为明确拒绝或在配置阶段禁止，不能空函数返回成功。以 WDTA 为例，不能为满足一个 Mode API 虚构可随时关闭硬件的能力。

DET 用于发现错误调用和配置问题；DEM 管理运行中的诊断事件与相关信息。两者与 DCM 的诊断协议服务不是同一个模块。为让演示运行可选择精简错误上报后端，但必须留下可观察信息，不能吞掉全部错误。

## 08 Port、Dio、收发器：CAN 开始之前 {#pins}

P1M-E 的 CAN 是芯片内控制器，CANH/CANL 由板外收发器驱动。软件需要同时正确配置 MCU TX/RX 和收发器供电/模式。CAN 控制器已 STARTED 而收发器仍 standby，是常见的“软件看起来没问题、总线不通信”。

| 控制器 | RX / ALT | TX / ALT |
| --- | --- | --- |
| CAN0 | P2_0 / 1 | P2_1 / 1 |
| CAN0 | P3_7 / 3 | P3_8 / 3 |
| CAN0 | P4_5 / 3 | P4_6 / 3 |
| CAN1 | P2_2 / 1 | P2_3 / 1 |
| CAN1 | P3_12 / 3 | P3_13 / 3 |
| CAN1 | P4_2 / 1 | P4_3 / 1 |
| CAN2 | P5_6 / 1 | P5_5 / 6 |

这是100 pin器件的候选表，必须选择原理图真正连接的一组；不能同时启用多个相同 RX 功能。P5_7 的其他封装配置不能照搬。HW-E pp.151–154；DS-E p.23。

`PORT=0xFFC10000`；组n步长0x40。PM=+0x10，PMC=+0x14，PFC=+0x18，PFCE=+0x1C，PFCAE=+0x28，PIPC=+0x4008。这些按16位访问。`[PFCAE,PFCE,PFC]`：ALT1=000、ALT3=010、ALT6=101。RX 的 PM=1、TX 的 PM=0、PMC=1；**CAN 的 PIPC 必须为0**，因为它不在 P1M-E 允许直接 I/O 控制的功能清单中。HW-E p.131。

初始化先将目标 pin 置安全输入/端口状态，配置电气与 ALT，再进入复用和 TX 输出。仅修改本模块拥有的位；PSR/PMSR/PMCSR 支持高16位掩码、低16位值的32位更新形式，可用于对应寄存器的受控位修改。PODC/PODCE 决定 push-pull/open-drain，PUCC/PDSC 决定 SLOW/FAST/MIDDLE，这些是32位寄存器；RS-CANFD 电气时序的 fast/middle 条件见 DS-E p.57。

收发器若由 GPIO EN/STB 控制，则 Dio 实现必须匹配实际有效电平；若通过 SPI 管理，则需要 Spi 及设备驱动。初始化输出值应先于输出使能，避免短暂进入错误模式。内部 agent 应记录 transceiver 型号、RX/TX 与 EN/STB 接线、电源域、Normal 状态读回和终端情况；这些不是从 RH850 手册能推断的值。

## 09 OS、OSTM、中断和 SchM 的连接 {#os}

用同一条因果链检查时间：`时钟 → OSTM 计数 → 中断 → OS Counter → Alarm/任务 → BSW MainFunction`。每一环都可能有单位和所有权错误。软件打印“1 ms”不证明中断就是1ms。

| OSTM0 寄存器 | 地址 | 宽度 | 本文1ms候选 |
| --- | --- | --- | --- |
| IC0CKSEL0 | 0xFFDD6000 | 16 | 0：PCLK=80MHz |
| CMP | 0xFFDD8000 | 32 | 79999，即0x1387F |
| CNT | 0xFFDD8004 | 32只读 | 当前计数 |
| TE / TS / TT | 0xFFDD8010 / 8014 / 8018 | 8 | 状态 / 写1启动 / 写1停止 |
| CTL | 0xFFDD8020 | 8 | 0：interval、无启动中断 |
| EIC74 | 0xFFFFB094 | 16 | OSTM0 EI通道74，由OS端口配置 |

OSTM1 基址0xFFDD9000、时钟选择0xFFDD6004、EI75/EIC=0xFFFFB096。周期模式 `(CMP+1)/counter_hz`；自由运行比较模式 CTL=2，可运行中更新CMP。来源 HW-E §22。

本案例可为 OS 独占 OSTM0；如果沿用现有 RTA 设计使用 OSTM1，也可，只要配置、回调和资源表一致。20MHz 计数必须证明 TAUD/TAUJ 计数使能链路，不能仅改一个软件频率常量。若用自由运行32位80MHz计数，53.687秒就回绕；1ms16位OS tick要65.536秒才回绕。应按无符号相邻差值累加并保留除法余数，维护间隔小于一个硬件回绕；Set/Cancel还要处理晚匹配与并发。全文算法见附录和现有 counter-design.md。

### CPU 端口和中断包装

移植 openAUTOSAR kernel 时，应实现 arch.h 中的 `Os_ArchInit`、`Os_ArchSetupContext`、`Os_ArchSwapContext`、`Os_ArchSwapContextTo` 等契约，并核对任务初始栈帧、ABI、异常返回、ISR嵌套及调度点。[R08](#ref-r08) 使用现成 RTA-OS 时，使用其真实端口和生成头，不能用同名空函数替代。

EIC 的 EIP[3:0] 为硬件优先级，0最高、15最低；EIMK[7]=1屏蔽；EITB[6]控制直接/表参考入口。OS配置里的优先级未必直接等于EIP。CAN是高电平源，须清CAN外设请求；OSTM0/1是同步边沿源，处理方法不同。不能从其他芯片复制 NVIC/SysTick 代码。

### SchM 的两种职责要分开

SchM 既涉及 BSW MainFunction 的调度，也涉及模块 exclusive area 的互斥组织。为每个 MainFunction 配置实际周期与执行上下文；为共享驱动状态选择短临界区或OS支持的保护。不能把长时间等待总线发送放在关闭全局中断的区间里。

教学调度表可从“OS tick=1ms、CanTp/DCM相关任务按其配置周期运行”开始，但不是所有模块都必须1ms；如果CanTp的超时以MainFunction次数递减，真实调用周期必须与配置一致。参考项目 CanTp_Cfg.c 的 main_function_period=20 只是示例，不可照搬成当前网络需求。[R11](#ref-r11)、[R13](#ref-r13)

openAUTOSAR 的实际 `SchM_BswService` 任务会调用CAN、CanTp、Dcm等宏，而 `SchM_MainFunction()` 是空体。[R16](#ref-r16) 同时要检查编译开关、宏节拍门控、Alarm到任务映射，避免“任务运行了，但所需模块被宏屏蔽”。

## 10 CAN driver 必须实现的契约 {#can-driver}

CAN driver 的职责是把上层的 L-PDU 请求转换成一个受控的硬件操作，并在正确时刻回报结果。至少管理三类对象：**控制器 Controller、硬件对象 HOH、正在进行的发送请求**。HRH是接收对象句柄，HTH是发送对象句柄；它们不必等于外设buffer编号，更不等于CAN ID。

| 接口/内部功能 | 实现要点 | 常见错误 |
| --- | --- | --- |
| Can_Init | 校验静态配置、单元全局资源、模式/位时间/过滤/缓冲；初始化软件状态 | 初始化一个channel时重置整个单元，破坏另一个channel |
| Can_SetControllerMode | 只接受合法转换；请求与实际硬件状态同步；按版本报告状态变化 | 写了CHMDC就立刻声称已STARTED |
| Can_Write | 校验HTH/格式/长度；分配空闲buffer；保存swPduHandle；复制并提交；忙则明确返回 | 返回成功后再读取已经失效的上层sdu指针 |
| 接收服务 | 读取完整一致帧、校验格式与长度、映射HRH、复制/交接、pop FIFO | 把DLC10当10字节，或pop后仍使用硬件窗口指针 |
| 发送完成服务 | 依据硬件成功/取消状态消费一次结果、释放owner、回调正确handle | 发请求就回调成功，或取消完成当发送成功 |
| Can_MainFunction_Read/Write | 当对应处理选择polling时消费同一事件状态机 | ISR与轮询同时消费同一个事件 |
| BusOff/Wakeup/Mode服务 | 捕获状态、通知规定回调、配合状态管理；处理版本可选功能 | 清标志后不通知CanSM，或无限自动restart |
| Disable/EnableControllerInterrupts | 处理嵌套禁用计数与配置允许的来源，恢复之前状态 | 第二层Enable提前打开仍需屏蔽的中断 |
| DET/运行错误 | 区分调用错误、硬件失败、资源忙、数据丢失 | 为避免报错而始终返回CAN_OK |

旧参考头声明 `Can_Write(Can_Arc_HTHType, Can_PduType*)`；Rx回调是 `CanIf_RxIndication(uint8 Hrh, Can_IdType, uint8 CanDlc, const uint8*)`。这些签名属于该仓库，不应冒充当前供应商的统一AUTOSAR4.x接口。[R09](#ref-r09)、[R10](#ref-r10)

教学运行时可维护：`controller_state[m]`、`disable_depth[m]`、`tx_owner[p]`（occupied、swPduHandle、长度/格式、必要的generation）、Rx FIFO owner、错误快照。全局配置独占；发送buffer可各自拥有受控的互斥状态。复位/停止时必须清理或通知所有未完成请求，不把旧请求结果附到新PDU上。

标准CAN Driver说明要求保留请求中的swPduHandle直到对应TxConfirmation；此关系也是参考项目CanIf发送路径明确提供的字段。[CAN Driver R23-11](https://www.autosar.org/fileadmin/standards/R23-11/CP/AUTOSAR_CP_SWS_CANDriver.pdf)、[R14](#ref-r14)。成功确认的含义是CAN发送完成，不是对端已经完成UDS业务处理。

## 11 P1M-E CAN 硬件配置：先选择模式，再选地址 {#can-config}

RS-CANFD 单元只有一个基址 **B=0xFFD20000**，内部有3个控制器。`GRMCFG=0xFFD204FC` bit0 RCMC选择接口布局：0为Classical，1为FD；`CANFDMDR=0xFFD28000` bit0只读确认。

| 对象 | Classical接口 RCMC=0 | FD接口 RCMC=1 |
| --- | --- | --- |
| CAN0 位时间 | CFG=0xFFD20000 | NCFG=0xFFD20000，DCFG=0xFFD20500 |
| 全局控制/状态 | GCTR=0xFFD20088，GSTS=0xFFD2008C | 相同地址，但部分附加字段不同 |
| 过滤窗口第0条 | 0xFFD20500…050C | 0xFFD21000…100C |
| Rx FIFO0 头 | 0xFFD20E00 / 0E04 | 0xFFD23000 / 3004 / 3008 |
| Tx buffer0 头 | 0xFFD21000 / 1004 | 0xFFD24000 / 4004 / 4008 |
| TMC0/TMSTS0 | 0xFFD20250 / 02D0，8位 | 相同地址和8位宽度 |

CAN 的 `GCFG=0xFFD20084` bit4 DCS：0=fCAN40MHz；1=fCAN16MHz。80MHz是pclk接口频率，不用于本文位时间分频。时序公式 `bitrate=fCAN/[divider×(1+TSEG1+TSEG2)]`；`TSEG1=PROP_SEG+PHASE_SEG1`，不要把配置器中分开的段值各自又重复加到总长度。物理量与寄存器编码差1。

| 模式 | divider / TSEG1 / TSEG2 / SJW | 位率、采样点 | 编码 |
| --- | --- | --- | --- |
| Classical | 4 / 15 / 4 / 3 | 500k，80% | CFG=0x023E0003 |
| FD nominal | 2 / 31 / 8 / 4 | 500k，80% | NCFG=0x071E1801 |
| FD data | 2 / 15 / 4 / 3 | 1M，80% | DCFG=0x023E0001 |

Classical的TSEG1最多16，不能使用FD nominal的31。FD两阶段必须使用相同BRP；TDC使能时物理divider≤2。手册文字与图对SJW边界有差异，本文采用严格 `TSEG1>TSEG2>SJW` 的保守候选。以上是算术候选，网络采样点和实际传播时序仍须匹配。

<div id="timing-calculator" class="interactive"></div>

### 接收规则和 FIFO 的完整小配置

教学单控制器可先让所有诊断标准数据ID进入FIFO0；其余FIFO、共用FIFO、Rx buffer、Tx queue/history关闭。`GAFLM=0xC00007FF` 表示精确比较标准ID并比较IDE/RTR；全零mask是通配。GAFLID标准ID放低11位，GAFLP1=1路由FIFO0。每页16条，通过GAFLECTR.AFLPN分页，不是无限连续数组。

本教程诊断案例仅用两条接收规则：物理请求`0x7E0`、功能请求`0x7DF`，**均为教学假设**。设置GAFLCFG0=`0x02000000`（CAN0两条，CAN1/2零条），global reset中GAFLECTR=`0x100`启用第0页写入；j=0/1分别填ID，mask相同，GAFLP0可分别为0/0x10000作为标签，GAFLP1=1；完成后GAFLECTR=0。

Classical FIFO0配置：RFCC0=`0xFFD200B8`，8B×8条，每帧IRQ的配置值先写`0x1202`（RFE=0），进入global operating后再单独写`0x1203`（RFE=1）。轮询则先`0x1200`后`0x1201`。RFSTS0=`0xFFD200D8`，RFPCTR0=`0xFFD200F8`；后者写32位0xFF弹出一条。FIFO满丢帧是错误事件，不是上层缓存自动扩展。

### 冷启动顺序

1. 上层回调和中断入口尚未开放；先确认时钟、引脚、资源owner与收发器状态。
2. 等待GSTS.GRAMINIT=0；清GSLPR并保持GMDC=reset，轮询确认global reset。
3. 在global reset先写GRMCFG选择模式；使使用的通道进入channel reset并确认。
4. 配置GCFG、CFG或NCFG/DCFG、分页规则、缓冲、错误处理策略与对应中断使能；FD另配置FDCFG。
5. 完成ISR/向量准备，global模式切到operating并等待状态；此后单独置RFE。
6. 使用的channel切到communication并确认；按板级时序让收发器进入Normal。
7. 上层就绪后开放所需通知；外部节点收发验证，记录寄存器和总线结果。

每一步都有超时与失败日志；不要一直等待到WDTA触发才知道失败。GRMCFG是整个单元的接口模式，不能给CAN0用Classic布局、CAN1同时用FD布局。运行后的全局重配置另需停机协调，不适用此冷启动流程。

## 12 发送、接收、完成回调：驱动的核心循环 {#can-io}

### 发送路径：接受请求与完成请求分开

`CanIf_Transmit` 将上层PDU映射成CAN ID、HTH、length、sdu与swPduHandle；`Can_Write` 分配硬件buffer并提交。[R14](#ref-r14) 本例CAN0 local0对应全局p0；CAN1 local0是p16。不能用软件channel编号直接代替全局buffer索引。

```c
/* 伪代码：接口与锁实现由目标栈提供。 */
Can_Write(hth, pdu):
    validate_initialized_controller_hth_id_length(pdu);
    enter_short_tx_exclusive_area();
    p = find_free_buffer_owned_by(hth);
    if (p is unavailable) { leave(); return CAN_BUSY; }
    consume_or_reject_any_stale_hw_result(p);
    tx_owner[p] = { occupied: true, swPduHandle: pdu.handle };
    copy_id_length_format_and_payload_to_hw(p, pdu);
    apply_required_io_ordering();
    write8(TMC[p], 1);  /* TMTR：提交后不能覆盖该buffer */
    leave();
    return CAN_OK;      /* 已接受，不在这里报告发送完成 */
```

TmSTS的TMTRF[2:1]：00无完成结果；01取消完成；10发送成功；11取消竞争中仍发送成功。成功处理应在保护范围内取得handle、消费硬件结果、释放owner，之后按约定回调CanIf。回调可能立即触发下一帧发送，因此顺序和锁范围要经过验证。仅看到TMTRM请求位变0不能宣布成功。

### 接收路径：搬走报文，再弹出硬件 FIFO

```c
rx_service():
    while (budget_available && RFSTS0.RFEMP == 0):
        header = read_fifo_header_for_selected_interface_mode();
        bytes = decode_length_and_validate_allocated_payload(header);
        frame = copy_header_and_payload_to_owned_software_storage(bytes);
        write32(RFPCTR0, 0xFF);  /* 当前消息已复制，弹出 */
        hrh = map_rule_label_and_controller_to_hrh(frame);
        deliver_rx_to_canif_under_its_callback_contract(hrh, frame);
    acknowledge_only_consumed_interrupt_flags();
    recheck_fifo_and_flags_and_schedule_remaining_work();
```

Classical 8B数据窗口有两个32位字；FD有不同的头部/步长。FD DLC9/10/11/12/13/14/15分别对应12/16/20/24/32/48/64字节，不能把DLC直接当字节长度。软件缓冲必须覆盖真实帧长；不能静默截断64B为16B后送CanTp。

W0C标志是“写0清除”，不同于很多外设的W1C。RFSTS.RFIF bit3、RFMLT bit2皆为W0C；仅清RFIF可写0x4，保留RFMLT。不要用通用读改写清标志模板，也不要将整寄存器读取值当合法回写值。先快照丢帧/错误，再清已处理源。

### 中断映射

| 本例使用的源 | EI通道 | EIC地址 | ISR职责 |
| --- | --- | --- | --- |
| CAN0 channel error | 183 | 0xFFFFB16E | 错误快照、bus-off等事件 |
| CAN0 Tx完成 | 185 | 0xFFFFB172 | 扫描已启用Tx源、完成确认 |
| CAN global error | 189 | 0xFFFFB17A | RAM/消息丢失等全局错误源 |
| **Rx FIFO0…7** | **190** | **0xFFFFB17C** | 消费所分配Rx FIFO |

EI184对应CAN0的common Rx FIFO，本例没有使用。CAN源为电平型，应清外设源；给EIC写0不能代替源消费。轮询与中断应复用同一个内部服务逻辑，但配置上指定唯一消费者。ISR预算耗尽后可靠地安排继续消费，避免遗留消息失去通知。

<div id="flow-explorer" class="interactive"></div>

## 13 从 CAN 到 UDS：一个完整配置例子 {#diagnostics}

以下是**教学网络假设**：CAN0、Classical 500k、11位ID；物理请求0x7E0、功能请求0x7DF、物理响应0x7E8。这些不是从截图认定的生产网络配置，实际项目要替换为其诊断描述和网络数据库中的ID。

### 四种编号不要混为一谈

| 编号类型 | 例子 | 由谁解释 |
| --- | --- | --- |
| 总线CAN ID | 0x7E0 / 0x7DF / 0x7E8 | CAN仲裁/过滤及CanIf映射 |
| Controller / HRH / HTH | Controller0、RxDiagHrh、TxDiagHth | Can与CanIf的硬件资源映射 |
| 模块内PduId | CanIfRxPhys、CanTpRxPhys、DcmRxConnection等符号 | 各模块内部的配置索引，不保证数值相同 |
| swPduHandle | 对应本次CanIf Tx PDU的标识 | 驱动保存，发送完成时原样返回 |

### 必须闭合的数据路径

| 行为 | 配置链 | 关键核查 |
| --- | --- | --- |
| 收诊断请求 | AFL→FIFO0→HRH→CanIf Rx PDU→CanTp Rx NSdu→PduR→Dcm连接 | 接收ID/格式、Rx indication目标、buffer容量 |
| ECU发诊断响应 | Dcm→PduR→CanTp Tx NSdu→CanIf Tx PDU→HTH→Tx buffer | 响应ID、控制器STARTED、CanIf PDU mode online |
| ECU接收长请求后的FC | CanTp Rx连接的FC Tx PDU→CanIf→Can | FC从正确的响应方向发出，不能漏配HTH |
| ECU发送长响应时收FC | 请求方向ID→CanIf Rx→对应CanTp Tx连接的FC处理 | 不能把同一接收ID只分配给一种不含FC的处理路径 |
| 每个CAN帧发送成功 | TMTRF→驱动→CanIf_TxConfirmation→CanTp对应确认 | 使用保存的handle，不能仅使用buffer编号 |

CanTp 根据PCI区分单帧SF、首帧FF、连续帧CF、流控FC，并处理SN、BlockSize、STmin及N_*定时。Dcm 处理诊断会话、服务分发、P2/P2*和应用回调；PduR连接两者。参考项目使用 `ProvideRxBuffer/ProvideTxBuffer` 一类旧接口，现代栈可能采用不同的buffer交接契约；适配时同时检查CanTp、PduR、Dcm，不能只改一个函数原型。[R21](#ref-r21)

功能寻址有其服务、响应和分段限制；本文仅用单帧功能请求理解路径，不将物理多帧逻辑无条件复制到功能请求。是否响应0x7DF及用哪个响应ID由诊断配置决定。

### 一个可抓包验证的单帧示例

假定DCM支持普通寻址的UDS DiagnosticSessionControl，且已配置允许进入扩展会话，则测试仪可发送CAN ID0x7E0、payload `02 10 03`（再按项目策略填充到8B）。02是CanTp单帧长度，10是UDS服务，03是请求会话。ECU的肯定响应业务数据以 `50 03` 开头，包含配置的会话时序参数，经CanTp封装从0x7E8发出。若该服务未配置、会话条件不满足，应按DCM策略回应或处理，不能伪造固定成功帧绕过栈。

验证时同时保存：外部总线记录、RxIndication计数、CanTp状态、Dcm服务入口、Can_Write返回、TxConfirmation计数。原始CAN回显通过不等于UDS通过；收到单帧响应也不等于多帧FlowControl和超时路径正确。

### 定时配置示例与陷阱

先选择一个能满足内部栈要求的调度基准，再把CanTp/DCM配置参数转换为其明确的时间单位。例如配置器填写10ms而代码每1ms递减一次计数，可能把超时缩短10倍。STmin还可能有亚毫秒编码，不能只用“1ms任务一定够”做结论。长处理使用Dcm支持的异步/response-pending机制，避免在CAN ISR内擦写Flash或执行耗时服务。

## 14 从 Classical 升级到 CAN FD，要改哪些层 {#fd}

P1M-E硬件有FD能力，但参考项目的CanTp明确以8B帧设计。[R11](#ref-r11) 因此先跑通Classical诊断可复用其已有模型；FD扩展必须作为独立版本适配工作。

| 层 | FD需要确认/实现 |
| --- | --- |
| Can配置类型 | 能表达nominal/data位时间、FDF/BRS、payload存储、TDC和相应开关 |
| Can硬件后端 | RCMC=1的新布局、NCFG/DCFG、FDCFG、FD头、DLC映射、容量和完成处理 |
| CanIf | ID/帧格式表示、真实字节长度、Tx/Rx配置、过滤和回调类型与目标版本兼容 |
| CanTp | SF/FF/CF的FD格式规则、容量、padding、流控、长度/边界处理、所需协议版本 |
| PduR/DCM | buffer交接与最大NSdu、动态长度、异步传输、时序以及版本接口 |
| 收发器/测试仪 | 数据速率、电气传播、FD协议兼容设置、实际波形与ACK |

普通FD Tx buffer最多20B，因此16B样例不需要merge；发送64B时仅支持每通道local0+1+2或local3+4+5的合并方式。Rx FIFO可以另设至64B，但其RAM预算与Tx不同。FD接收共享RAM预算为 `Σdepth×(12+payload)+RxBuffer占用≤5376B`；不能把Classical的buffer计数公式照搬。

TDC的TDCO是fCAN周期编码，不直接等于TSEG1。TDCOC选择“测得延时+offset”或“仅offset”；TDCO字段+1才是物理offset周期数。需要传播时序与供应商实现验证。FDOE=1表示FD-only，会对Classical接收有影响；RCMC=1时若仍需Classic帧，FDOE必须保持0。完整字段和限定见附录。

**不要仅将 `MAX_SEGMENT_DATA_SIZE 8` 改成64。** 参考CanTp还存在7/6字节payload常数、8字节局部数组和旧PCI/缓冲API。应先列出所有受影响点，再选择支持目标FD协议的兼容栈或系统性实现和验证；任何一层仍假定8B都会截断或误解释数据。

## 15 配置如何变成寄存器：构建与生成闭环 {#configuration}

ARXML是配置交换/描述形式，不是芯片直接执行的脚本。配置工具依据模块定义、目标版本和供应商约束生成C配置、头文件、OS对象、RTE接口等；驱动读取这些对象，再执行合法的寄存器序列。开源参考中的若干`*_Cfg.c`是遗留生成结果，不代表当前目录内包含能重新生成目标工程的完整工具链。

```text
器件/BOM/原理图 + 网络/诊断需求 + OS/MCAL版本
                  ↓
配置源：ECUC/ARXML 或该平台支持的配置描述
                  ↓
生成头/配置C + OS/RTE输出 + 目标BSP与驱动
                  ↓
RH850编译器 → 目标文件/库 → 链接脚本 → ELF / map
                  ↓
初始化时按状态机写MMIO → 读回 → 引脚波形 / CAN抓包
```

### 配置对象应包含什么

| 配置对象 | 内容 | 与其他配置的连接 |
| --- | --- | --- |
| McuClock | 输入源、CPU/HSB/LSB频率依据 | Can的fCAN引用，Gpt/OS实际计数源 |
| PortPin | port/pin、ALT、方向、pull/drive | CanController对应实际TX/RX；CanTrcv控制脚 |
| CanController | 硬件m、模式、nominal/data、处理方式、bus-off策略 | CanIf controller ID与OS ISR |
| CanHardwareObject | Rx/Tx、HRH/HTH、ID格式、过滤、buffer owner | CanIf Rx/Tx PDU引用 |
| OsCounter/Isr | 硬件timer、单位、ISR类别/优先级、回调 | SchM任务周期；CAN事件入口 |
| CanIfPdu | ID、长度、HRH/HTH、上层目的地、确认回调 | CanTp NSdu / COM等上层对象 |
| CanTpConnection | Rx/Tx/FC PDU、寻址、最大长度、padding、BS/STmin/N_* | PduR route、DCM connection和调度周期 |
| Dcm/PduR | 协议连接、buffer、服务、会话和路由 | 应用回调、传输方向、持久服务需求 |

一个工程只有一套实际生效的配置来源。直接改生成C之后再运行生成器，改动可能被覆盖；若教学项目选择手写配置，要明确标记并把所有派生值检查纳入构建。不要同时保留STM32与RH850同名头文件，让include搜索顺序决定芯片类型。

### 编译成功以后仍要检查什么

检查最终ELF和map里的reset入口、向量/INTBP目标表、任务与ISR符号、栈区、ROM/RAM边界、配置对象、被GC丢弃的段和未使用的旧架构符号。库可以包含未解析引用，生成若干`.a`不能证明最终固件可链接；链接成功也不能证明CPU异常帧正确。CMake跨编译分支、GHS工程或供应商make系统必须真实指定RH850工具链和目标芯片，而不是沿用占位编译参数。[R17](#ref-r17)

## 16 实施与验收：让每一步有可观察结果 {#bringup}

| 阶段 | 实施内容 | 进入下一阶段的证据 |
| --- | --- | --- |
| A 平台确认 | 固定器件/工具链/OS路线/原理图、接口版本 | HardwareBinding表无关键猜测值 |
| B 裸启动 | 启动/链接/RAM/异常日志/时钟、GPIO | 冷启动与所需复位路径稳定到main |
| C OS | 端口、任务切换、timer、Alarm、ISR | 两任务和中断并发，周期/上下文/栈正确 |
| D CAN轮询 | Classic500k、少量规则、单Tx buffer | 外部节点ACK；匹配接收；真实成功/失败可区分 |
| E CAN中断 | 正确EI190/185/错误入口与事件消费 | 与轮询等价，不重复确认、不漏帧、无中断风暴 |
| F CanIf | ID/HOH/PDU mapping、模式online | 正确上送Rx PDU并返回Tx confirmation |
| G 诊断 | CanTp/PduR/DCM配置与周期 | 项目允许的单帧、多帧、流控、超时可验证 |
| H ECU集成 | 状态管理、看门狗、持久化、故障恢复 | 负载、bus-off、复位/断线下行为符合项目要求 |
| I FD扩展 | 各层FD能力与硬件数据相位 | FD16等目标长度与混合帧条件独立通过 |

每阶段记录“配置值 → 实际寄存器 → 外部现象”，三者必须一致。板上只有一个节点且无ACK时反复发送，不应被当作软件一定错误；内部环回则不能验证外部引脚、收发器和终端。

建议自动化用例覆盖：非法HTH/Controller/长度、busy不覆盖旧数据、多个Tx完成顺序、取消与发送竞争、Rx满与丢帧、W0C并发、标准/扩展/RTR过滤、模式转换超时、OS tick回绕、停止与重启残留事件。使用fake MMIO可以检查地址、位值、访问宽度和顺序；不能模拟出收发器延时或真实CAN仲裁来替代上板。

<div id="acceptance-checklist" class="interactive"></div>

## 17 常见失败：沿链路定位，而不是反复改波特率 {#debugging}

| 现象 | 优先查哪一层 | 具体证据 |
| --- | --- | --- |
| 到不了main | 启动/链接/复位 | PC、RESF、RAM/ECC、入口/栈/ABI |
| 第一次任务切换后崩溃 | OS CPU端口 | 保存/恢复寄存器、栈对齐、异常返回、FPU状态 |
| CAN TX无波形 | Pin/控制器/Tx对象 | ALT/PM/PIPC、Normal模式、CHMDC、TMC写8位 |
| 总线一直重发 | ACK/时钟/收发器 | AERR、TEC、外部活动节点与总线波形 |
| Rx FIFO已有报文但CanIf不收 | IRQ/模式/映射 | EI190、RFIE/RFIF、HRH、CanIf PDU mode |
| 原始收发通过但诊断无响应 | CanTp/PduR/DCM | Rx PDU route、服务配置、MainFunction是否实际运行 |
| 单帧通过，多帧失败 | FC/确认/定时/缓冲 | 请求方向FC接收映射、FC发送PDU、TxConfirmation、SN/BS/STmin |
| 发几帧后CAN_BUSY | owner/完成消费 | TMTRF、TMIEC、EI185、软件handle是否释放 |
| Bus-off后永远不恢复 | 通知/状态管理 | CanIf默认NULL通知是否被替换；BOM与CanSM策略 |
| 约几十秒后timer异常 | 时间换算/回绕 | CNT delta、采样维护、Set/Cancel竞态 |
| FD小帧可收但大帧异常 | 跨层长度支持 | RFPLS、DLC、软件数组、CanTp旧常数、merge分配 |
| 只在优化编译后失败 | ABI/并发/访问语义 | volatile、内联/屏障、生命周期、未对齐强转、临界区 |

内部agent应保留失败当时快照，不先清光所有错误再排查。WDTA、ECM和Guard是可观测问题的一部分；禁用全部保护可能改变症状，却不能证明根因被修复。

## 18 给内部 agent 的实施清单与交付物 {#agent}

把本HTML和现有寄存器JSON/CSV一同交给内部agent。先选择“供应商栈配置”或“开源OS/MCAL移植”路线；不在同一个目标里无意混入两套OS、两套中断管理或两套CAN驱动。

1. 形成绑定表：具体芯片/封装、硅版本、HW资料版本、时钟来源、board pins、收发器、网络ID、帧格式、OS/MCAL/编译器版本。
2. 给每个资源唯一owner：CAN单元全局配置、每个Tx buffer、Rx FIFO、OSTM实例、EIC入口、Port pin、WDTA、Flash区域。
3. 形成接口差异表：当前头文件签名、参考项目签名、目标回调与配置类型。决定适配或替换的模块，不用强制类型转换掩盖ABI差异。
4. 按模块表完成配置与实现；按阶段A至H提供可验证结果。Classic诊断先闭合后，再对FD扩展做独立验收。
5. 输出实际构建目标与map、配置来源与生成差异、寄存器有效位读回、ISR/任务周期记录、CAN抓包、诊断会话和故障恢复结果。
6. 无板卡或工具链时，完成可审核代码/配置与静态检查，并列出缺失证据；不要把文档里的候选值当作实测值。

理解检查：为什么OS不是MCAL？为什么HTH不等于CAN ID？为什么Can_Write返回成功仍可能没有TxConfirmation？为什么Rx FIFO用EI190而不是EI184？为什么硬件FD能力不能证明旧CanTp支持FD？如果能沿本文的资源、状态和回调链解释这五点，就已经掌握移植最容易出错的边界。

## 19 源码证据、手册与后续阅读 {#sources}

下表由构建工具读取本地参考项目文件生成，包含文件位置、行号、用途与文件哈希。它用于追溯本次教学依据，不表示已经完成该项目的编译或功能测试。HTML离开当前目录后，本地源码链接可能不可用，但表中路径/行号和本文解释仍保留。

<div id="reference-sources"></div>

硬件主要依据：P1M-E HW-E Rev.1.20 §2（PORT）、§3（CPU）、§4（地址）、§6（中断）、§8（复位）、§12（时钟）、§17（CAN）、§21（WDTA）、§22（OSTM）、§36（RAM）；DS-E Rev.1.00用于具体型号、电气和封装限制。文末交接附录包含对应页码与更完整地址。三份用户提供文档的系列边界仍按第01节执行。

官方结构与契约参考：[AUTOSAR Classic](https://www.autosar.org/standards/classic-platform/)、[Layered Architecture R23-11](https://www.autosar.org/fileadmin/standards/R23-11/CP/AUTOSAR_CP_EXP_LayeredSoftwareArchitecture.pdf)、[CAN Driver R23-11](https://www.autosar.org/fileadmin/standards/R23-11/CP/AUTOSAR_CP_SWS_CANDriver.pdf)。这些版本用于查证概念，不代表参考仓库已经支持该版本。

项目内进一步阅读：[计数器设计](../counter-design.md)、[七模块MCAL设计](../mcal-reference-guide.md)、[硬件复审](../hardware-review.md)、[内部agent任务指令](../internal-agent-task.md)。现有C示例仅涵盖OSTM底层、FD位时间计算与tick累计，不是完整Can驱动或完整AUTOSAR MCAL。

## 20 P1M-E 硬件交接全文与寄存器检索 {#hardware-appendix}

下方地址索引按 `SYSTEM + CAN_BOTH + 所选一种CAN接口模式` 查询。选择模式只影响检索结果，不会执行任何硬件操作。部分地址在两种模式中代表不同寄存器；这也是本工具要求先选择模式的原因。目录中的数据窗口仅在已配置并满足状态条件时使用，不能批量遍历读写。

<div id="register-explorer" class="interactive"></div>

<div id="embedded-hardware-guide"></div>
