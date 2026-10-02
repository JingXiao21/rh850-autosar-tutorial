# Part XI 写作约定（Classic AUTOSAR 入门：原理与工作流）

> 所有 Part XI writer agent 必读。通用规则仍以 [00-writing-conventions.md](00-writing-conventions.md) 为准；本文件覆盖其中已过时的部分，并补充入门系列的特殊要求。

## 1. 定位

- 读者：有嵌入式 C 基础、**第一次系统学习 Classic AUTOSAR** 的工程师。
- 目标：讲清**原理**（为什么这样设计）和**工作流**（谁、在什么时候、用什么输入、产出什么）。深度细节链接到已有章节（Part I–X），不要重复。
- 已有深入章节（链接用）：
  - 架构/启动/配置/OS：`../02-autosar-classic/01..07`
  - MCAL：`../03-mcal/01..07`；CAN MCAL：`../04-can-mcal/`
  - 通信栈：`../05-can-stack/`；DCM：`../06-dcm/`
  - RTE/SWC：`../07-rte-swc/01..09`、`../autosar-swc-rte-tutorial.md`
  - 启动调试：`../10-boot-debug/`
  - 可运行 demo：`../../examples/uds_diag_demo/`（`integration/EcuM.c`、`integration/BswScheduler.c`、`rte/Rte_Dcm.c`、`swc/VehicleInfoSWC.c` 等）

## 2. 事实来源（优先级）

1. R25-11 规范文本：`artifacts/pdf-text/autosar-cp-R25-11/*.txt`（`=== PDF PAGE n ===`，引用写成 “EcuM SWS R25-11 p.n, SWS_EcuM_xxxxx”）。
2. 研究笔记：`docs/reference/research/06-primer-ecum-bswm-os-rte-notes.md`（EcuM/BswM/Os/RTE）、`07-primer-architecture-methodology-mcal-notes.md`（架构/VFB/方法论/ECUC/SWC 模板/BSW 通用/MCAL/Com 路径）。
3. 已有章节与 demo 代码（引用 path:line 前先 grep 确认）。

**现在本仓库已有** EcuM / BswM / Os / Rte / Com / PduR / CanIf / Port / Dio / Adc / IoHwAb / BSWGeneral / Methodology 等 R25-11 规范——00-writing-conventions.md 中“本仓库无该 SWS”的说法对这些模块已过时。旧章节引用的 DCM R20-11 / CAN R22-11 页码与 R25-11 不同，引用时写明 release。

## 3. 已确认的关键事实（不要写错）

- R25-11 的 EcuM **只有 flexible 形态**（fixed 在 R4.4.0 移除）；`EcuM_GoDown/GoHalt/GoPoll` 已由 `EcuM_GoDownHaltPoll` 取代；`EcuM_StartupTwo` 仍存在。
- `EcuM_Init` 不返回（最后调用 `StartOS`）。StartPostOS 中：`SchM_Start`、`BswM_Init`、`SchM_Init`、`SchM_StartTiming`。
- **谁调用 `Rte_Start`**：RTE SWS 写的是 EcuM；R25-11 EcuM/BswM SWS 中由 BswM 的 `BswMRteStart` action（ECUC_BswM_01073）完成。写作时如实说明这是规范间不一致，并说明真实项目以生成工具/集成方案为准。
- RTE 生成 task 与 ISR2 的**函数体**（SWS_Rte_06200、04560），但 **runnable→task 映射是配置输入**（`RteEventToTaskMapping` ECUC_Rte_09020、`RtePositionInTask` 09023、`RteActivationOffset` 09018），RTE 一般不创建 OsTask（strictConfigurationCheck 关闭时例外，SWS_Rte_05150）。BSW MainFunction 通过 `RteBswEventToTaskMapping` 映射。
- **方法论角色**（TR_Methodology R25-11）：规范定义的是 System Engineer、SWC Designer/Developer、BSW Designer/Module Developer、ECU Integrator 等**角色**，没有“BSW 厂商/工具厂商”角色；OEM/Tier1/supplier 只在少数条目（TR_METH_01047、01156、01157、01139）出现。“Tier1 = ECU Integrator”“OEM 写部分应用 SWC”属于 `[Industry Practice]`，不要写成规范规定。METH p.184：配置 MCAL、配置 IoHwAb 是 ECU Integrator 的任务。
- **R25-11 与旧版本差异**（入门章节提到时注明“R25-11 中”）：CanIf 的上层可以是 LSduR（R24-11 新增 L-SDU Router）；PduR API 是模板化命名 `PduR_<User:Up>Transmit`；`Com_MainFunctionRx/Tx` 带 `_<shortName>` 后缀；`Det_ReportError` 返回 `Std_ReturnType`；EXP 分层图已不再列 Fls/Eep（由 Mem 驱动取代）。
- BSWGeneral 没有规定 `Mod_Cbk.h`，`Mod_Cfg.h` 只出现在示例中——把它们写成“常见约定”。
- EXP_LayeredSoftwareArchitecture 的页码用 PDF 页码（幻灯片页脚 = PDF 页 + 10）。
- R25-11 CAN Driver / CanTp 文本也已抽取：`artifacts/pdf-text/autosar-cp-R25-11/AUTOSAR_CP_SWS_CANDriver.txt`、`AUTOSAR_CP_SWS_CANTransportLayer.txt`。
- `Rte_Pim` 的 SWS ID 抽取有歧义——除非你在 PDF 文本中亲自确认，否则不要写具体 ID。

## 4. 写法要求

- 全中文，术语保留英文；标记 `[AUTOSAR Standard]` / `[Conceptual]` / `[Educational Implementation]` / `[Real Project Consideration]` / `[Industry Practice]`（新增：行业惯例，非规范规定）。
- **入门友好**：先给“直觉/类比”，再给“规范怎么说”，再给“代码/配置长什么样”，最后“真实项目里你会看到什么”。类比只能帮助理解，不能替代事实。
- 每章开头：
  ```markdown
  # 标题
  > 本章回答：……（1–3 个问题）
  > Prerequisite: …   Next: …
  > 对应规范（R25-11）：…    深入阅读：…（链接到 Part I–X）
  ```
- 每章结构（可按需取舍）：1 本章要回答的问题 → 2 直觉理解 → 3 原理（规范依据） → 4 运行时/工作流细节（Mermaid sequence/flowchart，图后逐步解释） → 5 代码/配置示例 → 6 常见误解 → 7 真实项目里你会看到什么 → 8 一句话记住（3–6 条要点） → 9 自测题 → 10 下一章。
- 每章 300–600 行，宁深勿空；Mermaid 消息里不要用 `;`、不要嵌套引号。
- 只写分配给你的文件；不要修改其他文件。
- 如果 Write 工具拒绝写文件，把完整内容放在最终回复中返回。
