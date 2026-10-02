# Part XI 入门系列 — 技术审校日志

范围：`docs/11-classic-autosar-primer/`（README + 01..10）。方法：对 R25-11 抽取文本（`artifacts/pdf-text/autosar-cp-R25-11/*.txt`）逐条 grep 验证 ID 与 PDF 页码；链接用脚本解析全部相对链接与锚点；Mermaid 用 `check.mjs` 校验。

## 1. 结果摘要

- 核对规范事实约 230 条（SWS/ECUC/CONSTR/METH ID + 页码，覆盖 EcuM、BswM、Os、Rte、Com、PduR、CanIf、Can、Adc、Mcu、Port、Dio、IoHwAb、BSWGeneral、Det、TR_Methodology、EXP、SWCT）。需修正的页码 1 处（重复出现，共 2 个位置）；其余全部与 R25-11 一致。
- 链接：全部相对链接与锚点可解析；5 处“占位链到 README”中 2 处是 Prerequisite/Next 头（已改），其余 3 处是合理的 README 引用。
- Mermaid：35 块，bad=0（审校前后均为 0）。

## 2. 修改清单（file:line → before → after → 证据）

| 文件:行 | before → after | 证据 |
|---|---|---|
| 01:4 | Prerequisite 指向 README → “本 Part 的 README 导读（无前置章节）” | 链条 01 起点 |
| 03:1 | 标题无编号 → `# 03 …`（04/05/06/10 同理） | 与 01/02/07/08/09 统一 |
| 03:4 | Prerequisite 为 README+“前两章”；Next=10 → Prerequisite=02；Next=04 | 链 01→02→…→10 |
| 03:499 | 下一章指向 10 → 指向 04 | 同上 |
| 04:4 | Prerequisite 含 05、06（与 05/06 的 Prerequisite 成环）→ Prerequisite=03，05/06 仅作“后文详述” | 消除环 |
| 10:4 | Prerequisite=03 → Prerequisite=09；Next 标明“无（终章）” | 链条终点 |
| 10:301 | “顺序 01→09→本章” → “顺序 01→10” | 终章 |
| README:~114 | 第 8 章简介写成“SWC 模型/contract phase” → “Rte_* API 场景/返回码、BSW 服务端口、demo 走读” | 与 08 章实际内容一致 |
| 06:108, 07:403 | `ECUC_Rte_09029` p.1180 → p.1181 | RTE SWS p.1181（及 p.1214） |
| 08:342 | “NvM SWS 不在本仓库，operation 名以所用 release 为准” → R25-11 NvM §8.7：`NvMService`（SWS_NvM_00734，p.136）operation 列表（ReadBlock/WriteBlock/GetErrorStatus/EraseBlock/InvalidateNvBlock/RestoreBlockDefaults/SetRamBlockStatus/Get/SetDataIndex/Read/WritePRAMBlock/RestorePRAMBlockDefaults），port `PS_{Block}`（SWS_NvM_00847，p.146），`NvMAdmin`（SetBlockProtection，p.133）等；`NvM_ReadAll` 不在 SWC 接口 | NvM SWS 文本 p.133–136、146 |
| 08:343 | “Dem SWS 不在本仓库” → R25-11 Dem §8.6：`DiagnosticMonitor`（SWS_Dem_00598，p.360）operation：SetEventStatus/ResetEventStatus/ResetEventDebounceStatus/PrestoreFreezeFrame/ClearPrestoredFreezeFrame/SetEventDisabled/ResetMonitorStatus；port `Event_{Name}`（SWS_Dem_01037，p.385）；`DiagnosticInfo`（SWS_Dem_00599，p.357）、`ClearDTC`（SWS_Dem_00666，p.353）；C API `Dem_SetEventStatus` p.273 | Dem SWS 文本 |
| 03:327 | “DCM 的 SWS 为 R20-11，页码不同”+ 无页码 → `DcmDspDataUsePort`=ECUC_Dcm_00713，R25-11 Dcm p.598 | Dcm SWS p.598（USE_DATA_ASYNCH_CLIENT_SERVER 存在） |
| 05:9 | “本教程的 MCU SWS 引用 R24-11 的 Part III…” → “本章的规范引用均按 R25-11 核对” | MCU R25-11 文本已在仓库 |
| 04:~11 | “截图里你可能见过” → “你可能听过” | 去除无上下文指代 |
| 04, 06, 07, 01 | 首次出现处补括号释义：post-build、callout、OSEK、partition、ARXML/ECUC | 初学者可读性 |

## 3. 重点项核对结论（均无需改动，列证据）

- **EcuM_Init 序列（04 §4.2）**：SWS_EcuM_02811 p.112；02411 p.37；02684 p.39；04085 p.136；02905 p.137；02906 p.138；02796/02798 p.44；02904 p.136；02907 p.138；02623 p.38；02826 p.132；02181 p.41；02822 p.118；04137 p.139；02603 p.41；04145 p.83。全部吻合。
- **StartupTwo / StartPostOS**：SWS_EcuM_02838 p.112；02806 p.112–113；02934/02932 p.41–42；SchM_Start/Init/StartTiming = SWS_Rte_91171/91170/91172（p.927/926/928）；BswM_Init SWS_BswM_00002 p.69。吻合。
- **Rte_Start 调用者**：RTE CONSTR_09035（p.805）写 “EcuStateManager”；EcuM StartPostOS 无 Rte_Start；BswM ECUC_BswM_01073（p.169）。04 §4.6、07 §8.2、10 Q18、README、demo 对照表（03:217、07 §12）均按同一口径：规范间不一致，R25-11 以 BswM action 理解，项目以生成/集成代码为准。无矛盾。
- **RTE 事件与映射**：Table 4.1/4.2 p.142–143 事件名全部吻合；ECUC_Rte_09018/09019/09020/09021/09023/09024/09025/09026/09027/09063/09064/09067/09068/09133（p.183–191、1156–1223）；SWS_Rte_06200/04560（p.134）、05150（p.111 等）、07843（p.1159）。
- **Rte API/返回码（08）**：SWS_Rte_01071/01091/01072/01092/01102/01111/02631/02628/01118（Rte_Pim 签名，经 p.743 原文确认）/01252/03928/03741/03744/01120/01123 页码全部吻合；RTE_E_* 取值（0/1/128–136/141/64）与 SWS_Rte_01058.. 表一致。
- **通信栈（09）**：Com_SendSignal 0x0a、Com_ReceiveSignal 0x0b、Com_RxIndication 0x42、PduR transmit 0x49 / RxIndication 0x42、CanIf_Transmit 0x49、CanIf_RxIndication 0x14、Can_Write 0x06 均吻合；LSduR 于 R24-11 新增、Fls/Eep 于 R25-11 移除（EXP p.2）吻合。
- **METH（02/03/05）**：TR_METH_01023/01024(p.30)、01047(p.40)、01049(p.41)、01109/01110(p.41)、01111/01112(p.42)、01114/01087(p.94)、01116(p.96)、01092(p.103)、01093(p.107)、01096(p.110)、01104/01105(p.116)、01139(p.137)、01156/01157(p.71/72)；“Configure MCAL / IO Hardware abstraction” 属 ECU Integrator，p.184。吻合。
- **OS（06）**：§3.5 两条 spinlock 论述（天花板协议不足以保护跨核临界区；GetSpinlock/ReleaseSpinlock/TryToGetSpinlock = SWS_Os_00686/00695/00703 p.199/200/202）由 Os p.109（§7.9.21）与 p.199–202 支持，已有页码，无需软化。其余 Os ID（00001、00424、00500、00607–00610、00299、00445–00448、00538–00541、00241、00327 等）页码吻合。

## 4. 一致性检查

- EcuM flexible-only：04 开篇、§3.1、§4.5；GoDownHaltPoll 取代 GoDown/GoHalt/GoPoll；10 Q 条目同口径。
- MCAL 由 ECU Integrator 配置（METH p.184）：02/03/05/09 一致；“芯片厂交付/Tier1=Integrator”均带 `[Industry Practice]`。
- RTE 生成 task 体、映射为集成者配置：06 §4、07 §0、README §7、10 一致。

## 5. 遗留问题

- 05 §7 总表中 Mem/MemAcc 初始化者“BswM list”为推断，规范未明确，建议读者以项目为准（未改动，已标注“典型”）。
- 09 的 LSduR 仓库内无 SWS，仍只能描述到 CanIf 文本层面。
- Gpt/Wdg/Spi/Icu/Pwm 的 R25-11 SWS 不在仓库，05 仅写概念层（已声明）。
