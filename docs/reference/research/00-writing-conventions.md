# 教程写作约定（所有 chapter agent 必读）

> 本文件是多 agent 协作写作的共同约定。来源：`claude_plan.md`（用户原始要求）+ Phase 1 research 结论。

## 1. 事实来源与优先级

AUTOSAR SWS > RH850 hardware manual > actual source code > project docs > previous agent output。

Phase 1 research notes（必须先读与你章节相关的部分）：

- `docs/reference/research/01-project-and-docs-review.md` — 现有项目/文档 review、已验证事实清单、已发现错误
- `docs/reference/research/02-autosar-sws-notes.md` — SWS requirement 笔记（含 PDF 页码）
- `docs/reference/research/03-openautosar-trace.md` — openAUTOSAR path:line trace
- `docs/reference/research/04-rh850-hardware-notes.md` — RH850/P1M-E 硬件事实（含手册页码）

原始资料（可 grep 复核）：`artifacts/pdf-text/*.txt`（`=== PDF PAGE n ===` 分页）。

## 2. 关键已确认事实（不要写错）

- 教学参考器件：**R7F701381 = RH850/P1M-E**，core **RH850G3M**（lock-step），**不是 G4MH**。
- P1M-E **没有软件可编程 PLL 寄存器**，**没有 PROTCMDn/PROTSn**。时钟固定（CPU 160 MHz / HSB 80 MHz / LSB 40 MHz / MainOSC 16 MHz）。写保护是 P-Bus Guard / Slave Guard；0xA5 解锁序列只用于 CLMA/ECM/FLMDCNT。讲 `Mcu_InitClock` / PLL 时：先讲 AUTOSAR 通用模型和"典型 RH850（如 P1x/F1x/U2A 系列有 PLL/PROTCMD）"的概念，再明确 **P1M-E 上的实际情况**；其他 derivative 的寄存器细节必须标注"需根据实际芯片手册确认"。
- CAN 外设：**RS-CANFD**，1 unit，3 channels，base 0xFFD2_0000；Classical / FD 模式寄存器偏移不同；fCAN = clkc 40 MHz 或 clk_xincan 16 MHz（GCFG.DCS），**不是 80 MHz**；中断 channel 183–193（global error 189，RX FIFO 190；EI184 是 CAN0 common FIFO，不是 RX FIFO）；GAFLM bit=1 表示"比较"。
- OSTM0/OSTM1 归属在不同文档中不一致——写作时说明这是配置选择，不是硬件事实。
- AUTOSAR 版本不统一：MCU SWS = R24-11，CAN SWS = R22-11（`Can_Write` 返回 `Std_ReturnType` E_OK/E_NOT_OK… 具体见 notes），DCM SWS = R20-11，IoHwAb = R24-11。`AUTOSAR_SWS_Diagnostics.pdf` 是 **Adaptive Platform**，不能作为 Classic DCM 依据。引用 SWS 时写明 release。
- 仓库中**没有** CanIf/CanTp/PduR/Dem/NvM/Rte/Os/EcuM/BswM 的 SWS。涉及这些模块的 SWS ID 不得编造；可描述公认的 R4.x API 形态，但标注"本仓库无该 SWS，需以真实项目所用 release 的 SWS 确认"。
- openAUTOSAR = Arctic Core 2.18.0，主体 R3.1.5 风格（`Dcm_ProvideRxBuffer` 等旧 API）；没有 Can driver、没有 CAN 仿真、配置大量缺失、无法链接运行；CanIf→CanTp 路由缺失；`DCM_USE_SERVICE_*` 未定义。引用 openAUTOSAR 时必须给 path:line 并说明与 R4.x 的差异。
- 目标工程背景（来自截图，本仓库不存在）：RTA-CAR 12.9.0、RTA-OS RH850GHS port、GHS 编译器、Renesas P1M MCAL（AR 4.2.2 API）。只能作为"可能的真实环境"提及，不能当事实断言其内部实现。
- 当前教学项目的可执行 C 代码：`examples/rh850_mcal_reference/`（OSTM、CAN FD bit timing、tick accumulator、MMIO shim）。Phase 5 新增 `examples/uds_diag_demo/`（host 可运行的教学诊断栈：Mock Can → CanIf → CanTp → PduR → Dcm → Rte → VehicleInfoSWC），引用其路径前先确认文件存在。

## 3. 标记（必须使用）

代码块和关键段落前标注：`[AUTOSAR Standard]` / `[AUTOSAR API]` / `[RH850 Hardware]` / `[Educational Implementation]` / `[Conceptual]` / `[Real Project Consideration]`。教学伪代码绝不能看起来像 production code。

## 4. 语言与风格

- 全中文，专业术语保留英文（"DCM 的 Diagnostic Service Dispatcher（DSD）负责根据 SID 将 request dispatch 到对应 service handler"）。
- **不要写 API 字典**。每个 API 讲：谁调用、何时调用、输入/输出、sync/async、callback、interrupt/MainFunction 位置、configuration 来源、runtime state 位置、出错时 ECU 会发生什么、对应 RH850 硬件。
- 多用 Mermaid sequenceDiagram / flowchart，diagram 之后逐个 transition 解释对应 API。
- 不复制大段 SWS 原文；引用 SWS ID 时解释：要求什么 / 为什么这样设计 / 实现中通常如何做 / RH850 对应什么 / openAUTOSAR 或教学项目中对应哪里。
- 不猜文件路径、行号、寄存器地址、位域。拿不准就去 grep；仍无依据就写"需根据实际芯片手册确认"或"需在真实项目环境中确认"，并说明为什么重要、去哪里找、怎么映射回本教程。

## 5. 章节结构

文件开头：

```markdown
# 标题

> Prerequisite: [..](..), [..](..)
> Next: [..](..)
> 对应规范: ...（release + PDF 页码）
> 对应源码: openAUTOSAR path / 本项目 path
```

核心章节尽量遵循：1 本章目标 → 2 为什么需要 → 3 系统位置 → 4 AUTOSAR 如何定义 → 5 核心数据结构 → 6 初始化流程 → 7 Runtime Flow → 8 RH850 Hardware Mapping → 9 openAUTOSAR 实现 → 10 当前教学项目实现 → 11 Code Walkthrough → 12 Debug 方法 → 13 常见问题 → 14 实验 → 15 对未来真实项目的意义 → 16 本章总结 → 17 下一章。不必机械全写，但"对未来真实项目的意义"必须有。

每篇核心章节应实质完整（通常 400–1200 行 Markdown），宁可少而深，不要多而空。

## 6. 协作规则

- 只写分配给你的文件。不要修改 `docs/rh850-autosar-tutorial-content.md`（最后由 index agent 统一更新）、不要修改其他 agent 的章节、不要修改 openAUTOSAR。
- 相对链接指向的其他章节即使尚未存在也按目标目录结构写（见 `claude_plan.md` "推荐目录结构"），index agent 最后会校验。
- 完成后给协调者的回复：≤250 字，列出写了哪些文件、行数、未解决问题。
