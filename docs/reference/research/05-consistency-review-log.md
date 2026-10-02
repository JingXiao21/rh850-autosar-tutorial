# 05 — 跨章节一致性技术复审日志（Phase 6 Review）

> 复审日期：2026-10-02
> 角色：技术审阅 / 编辑（不新增章节内容，只做最小化、可追溯的修正）
> Source of Truth 优先级：AUTOSAR SWS 文本（`artifacts/pdf-text/AUTOSAR_*.txt`）> RH850/P1M-E 硬件手册 HW-E（`artifacts/pdf-text/r01uh0585ej0120.txt`）/ 数据手册 DS-E > 源码 > 文档
> 范围：`docs/01-rh850` … `docs/09-real-project-preparation` 全部章节，`docs/00-learning-roadmap.md`、`analysis-and-learning-plan.md`、`debugging-autosar-diagnostics.md`、`dcm-upgrade-guide.md`、`autosar-swc-rte-tutorial.md`，以及 legacy 文档。**未修改** `docs/rh850-autosar-tutorial-content.md`、任何 README（例外：`examples/uds_diag_demo/README.md` 末尾追加一节）、`docs/reference/` 下其他文件、`examples/` 下任何代码。
> 行号约定：表中行号为**修改后**的当前行号。例外：legacy 文档（`agent-guide.md`、`hardware-findings.md` 等）的 D 类修改先于 banner 插入，表中写的是**插入 banner 前**的行号，当前行号需 **+6**（banner 自身也写明了这一点）。

## 0. 方法

- 冲突项 a–j：先 grep 全部章节，再对照手册/SWS 原文页（`=== PDF PAGE n ===`）确认，只改与原文矛盾或缺少限定条件的句子。
- Mermaid：先用 Python 启发式扫描（`;`、未加引号的括号、裸 `end` 等），再在 scratchpad 中安装 `mermaid@11` + `jsdom`，对 `docs/**`（不含 `reference/`）与 demo README 中的全部 mermaid 块调用 `mermaid.parse()`。修复前 14 处失败（12 处启发式发现 + 2 处仅解析器发现）；修复后 **246 个块全部通过**。
- 标记：扫描 C/H 代码块前 4 个非空行与块内前 3 行；只对“所在小节没有任何标记、且不是 openAUTOSAR 摘录”的块补标记（68 处）。补法不改变行数：写进块内首行注释，或在首行末尾追加 `/* [Educational Implementation] */`。openAUTOSAR 摘录（14 处）已在首行注明 `[openAUTOSAR]` 或 path:line，保持不变。

## 1. 已知冲突 a–j 的结论

| 项 | 结论 | 依据 |
|---|---|---|
| a. BOM=11 与 `SWS_Can_00274` | **13 章正确**：`BOM=01b/10b` 无条件满足“禁止自动恢复”；`BOM=11b` 只在软件于 128×11 隐性位检测完成前写 CHMDC=10B 时才满足，否则硬件自行回到 error active（“However, if 11 consecutive recessive bits are detected 128 times … before the CHMDC[1:0] bits are set to 10B, a bus off recovery interrupt request is generated”）。修正 04-can-mcal/01 两处；06-can-controller-init 未发现无条件断言；legacy handoff 在 banner 中注明 | HW-E p.807 BOM[1:0] 段；SWS CAN R22-11 p.43 |
| b. 时钟寄存器写保护 | 时钟控制器寄存器：“protected … by configuration of the **Slave Guards**”（HW-E p.471 §12.3.1）；复位寄存器：“… of the **P-Bus Guards**”（p.420）；PBG/HBG 是 slave guard 的两种（§31.4.1.2）。07-clock-system:389 把时钟写成“Slave Guard（PBG）”、analysis-and-learning-plan 与 09/02 把两者并列混写，已分别标注。03-mcal/02:301/642/650/778、03-mcal/07:119、01-rh850/04:568 原本正确 | HW-E p.420、p.471、txt:177413 起 |
| c. RESF 看门狗标志 | 全部章节已一致：RESF 无独立 WDT 位；WDTA 错误 → ECM（错误源 0）→ SRESF4/ARESF2（取决于 RESC0，默认 ARESF2）。无需修改 | HW-E p.418–422、p.1524、p.2790、p.2817 |
| d. EI184 vs EI190 | 章节全部正确；legacy `agent-guide.md:71`、`hardware-findings.md:86/89` 已就地修正/补注；04-can-mcal/02:383 对旧错误的引用改为“曾…已修正” | HW-E p.285–286 Table 6.11、p.792 Table 17.8 |
| e. OSTM0/OSTM1 归属 | 章节已统一为“配置选择，不是硬件事实”；legacy `counter-design.md`、`agent-guide.md`、`mcal-reference-guide.md`、handoff 在 banner 中注明 | 00-writing-conventions §2 |
| f. `Can_Write` 返回值 | 章节中 `CAN_OK` 只出现在 R3.x/openAUTOSAR/4.2.x 对照语境；补了 04-can-mcal/06:466（`Can_SetControllerMode` 返回类型）的版本限定 | SWS CAN R22-11 p.66（`Std_ReturnType Can_SetControllerMode`）、p.80–81 `SWS_Can_00233` |
| g. CmCFG 示例值 | `0x023E0003` = Classical、fCAN 40 MHz、500k、div4/TSEG1 15/TSEG2 4/**SJW 3**；`0x003E0003` 只差 **SJW 1**。补齐 4 处缺少 SJW/上下文的地方 | HW-E p.803–804（SJW[25:24]）；04-can-mcal/03 §6.1 |
| h. demo 偏差 | 6 项偏差逐一对照源码确认，在 `examples/uds_diag_demo/README.md` 末尾新增“已知与规范的偏差”表；补 3 处章节描述使其一致 | 见 §2 H 类 |
| i. DCM API 事实 | 无矛盾：`Dcm_ExternalProcessingDone` 在 R20-11 txt 中 0 命中；`Dcm_TxConfirmation` 在 R20-11 是 IF/周期传输（`SWS_Dcm_01092` p.247）；`BswM_Dcm_RequestSessionMode` 不存在（R20-11 只有 `BswM_Dcm_ApplicationUpdated`、`BswM_Dcm_CommunicationMode_CurrentState`，p.262） | SWS DCM R20-11 |
| j. PROTCMD/PLL/G4MH/80 MHz | 无章节对 P1M-E 作出肯定断言；所有出现均在“典型 RH850 / [Conceptual] / 常见误区”语境 | HW-E §12.3；DS-E p.1–2 |

## 2. 变更清单

### 2.1 冲突修正与核对（a–j）

| ID | 位置（file:line） | before → after | 证据 |
|---|---|---|---|
| A1 | `docs/04-can-mcal/01-can-hardware-basics.md:348` | "BOM=01 或 BOM=11 都能满足" -> BOM=01/10 无条件满足；BOM=11 有条件（软件需在128x11前请求halt） | HW-E p.807 BOM 11B 原文；与13章表5.3一致 |
| A2 | `docs/04-can-mcal/01-can-hardware-basics.md:572` | "RS-CANFD 用 CmCTR.BOM 满足" -> 加"（01b/10b；11b 有竞争窗口）" | 同上 |
| B1 | `docs/01-rh850/07-clock-system.md:389` | "Slave Guard（PBG）" -> "Slave Guard（p.471 未指名具体 guard），P-Bus Guard 字样属于复位寄存器 p.420" | HW-E p.420 / p.471 / §31.4.1.2 |
| B2 | `docs/analysis-and-learning-plan.md:46` | "时钟控制器靠 P-Bus Guard / Slave Guard" -> "时钟控制器靠 Slave Guard(p.471)，复位寄存器靠 P-Bus Guard(p.420)" | 同上 |
| B3 | `docs/analysis-and-learning-plan.md:148` | "写保护 = PBG / Slave Guard" -> 分别标注 | 同上 |
| B4 | `docs/09-real-project-preparation/02-how-to-read-mcal.md:148` | "P-Bus Guard / Slave Guard" -> 分对象标注 + 加 p.420 | 同上 |
| C0 | — | (RESF/WDT) 全部章节已一致（02-mcu-driver:481/652/731、04-startup:232/601、03-ecu-startup:525、07-mapping:232、08-peripheral:346），无需修改 | — |
| D1 | `docs/hardware-findings.md:86` | "CAN0 Tx/Rx FIFO receive completion" -> 加"（common FIFO）——不是 Rx FIFO，Rx FIFO0–7 见 EI190" | HW-E p.285–286, p.792 |
| D2 | `docs/hardware-findings.md:89` | "CAN receive FIFO" -> "CAN receive FIFO（Rx FIFO0–7 共用，INTRCANGRECC）" | 同上 |
| D3 | `docs/agent-guide.md:71` | "CAN0 error/RxFIFO/Tx=183/184/185，CAN 全局 error/RxFIFO=189/190" -> "CAN0 error/common(Tx/Rx) FIFO receive/Tx…（EI184 不是 Rx FIFO 中断）… Rx FIFO0–7=189/190" | 同上 |
| D4 | `docs/04-can-mcal/02-rh850-can-peripheral.md:383` | '把 EI184 称为 RxFIFO，是错误的' -> '曾把…；已在一致性复审中修正' + 链接本日志 | D3 |
| E0 | — | OSTM0/1 归属：所有章节已统一为“配置选择，不是硬件事实”（01-rh850/06:370、08:100/357；02-autosar-classic/06:284；03-mcal/01:236、05:273、07:145/290；05-can-stack/03:321；06-dcm/01:305 等），无需修改；legacy counter-design.md/agent-guide.md 由 banner 说明 | — |
| F1 | `docs/04-can-mcal/06-can-controller-init.md:466` | '返回 Can_ReturnType（CAN_OK/CAN_NOT_OK），不是 Std_ReturnType' -> 加版本限定 'R3.x/早期 R4 风格…R22-11 返回 Std_ReturnType（E_OK/E_NOT_OK，p.66）' | SWS CAN R22-11 p.66 Can_SetControllerMode 签名 |
| F2 | — | 其余 CAN_OK 出现处（04-can-mcal/10:91/395、05-can-stack/01:358、08-integration/02:101、09/02:72、analysis:177）已带版本限定，未改 | — |
| G1 | `docs/02-autosar-classic/04-configuration-arxml.md:350-351`（代码块注释，新增 2 行） | 解码 0x003E0003 未说明与 0x023E0003 关系 -> 追加两行注释：本例 SJW=1 Tq；04-can-mcal Demo 取 SJW=3 Tq → 0x023E0003 | HW-E p.803–804（CmCFG SJW[25:24]）；04-can-mcal/03 §6.1:199-213 |
| G2 | `docs/03-mcal/07-rh850-hardware-mapping.md:268` | 解码步骤末尾加"（04-can-mcal 的 Demo 值 0x023E0003 只差 SJW=3 Tq。）" | 同上 |
| G3 | `docs/debugging-autosar-diagnostics.md:308` | "例 0x023E0003 为 40 MHz/500k 候选值" -> "…Classical 接口、fCAN 40 MHz、500k、SJW=3 Tq 的候选值" | 同上 |
| G4 | `docs/04-can-mcal/15-can-driver-debugging.md:79` | "例 0x023E0003" -> 加 "Classical、fCAN 40 MHz、500k、SJW=3 Tq" | 同上 |
| G5 | — | 其余 0x023E0003 出现处（04-can-mcal/03,06,08,09,14）所在章节已给出 divider 4/TSEG1 15/TSEG2 4/SJW 3 推导；0x003E0003 在 04-can-mcal/03:213 已说明为 SJW=1 变体；未改 | — |
| H1 | `docs/05-can-stack/07-can-tx-path.md:436` | 实验 1 trace 后未提示 "response on the bus" 措辞 -> 追加一句：E_NOT_OK 时该固定文字不准确，以 result 为准，链接 demo README 已知偏差 | examples/uds_diag_demo/diag/Dcm_Dsl.c:493-495 |
| H2 | `docs/06-dcm/08-did.md:251` | F1A0 "读 SYNCH，写为 async 形态" 未标为偏差 -> 加 "（demo 简化：R20-11 一个 DcmDspData 只有一个 DcmDspDataUsePort…）" | Dcm_Cfg.c:44-48；SWS_Dcm_00794（R20-11）；与 07-rte-swc/02:197、06-dcm/05 一致 |
| H3 | `docs/06-dcm/07-security-access.md:422`（新增 1 行表格） | demo 未实现列表缺 00139 偏差 -> 新增行："00139 只在非默认→任意会话转换时复位；demo 对所有转换（含默认→默认）都上锁" | SWS_Dcm_00139（R20-11 p.74，txt:4547-4553）；Dcm_Dsl.c:114-115；与 06-dcm/06:395 一致 |
| H4 | `examples/uds_diag_demo/README.md 末尾追加 "## 已知与规范的偏差 (Known deviations)"（仅追加）` | 6 项偏差 + 其他 | 见该节 |
| H5 | — | 已核对一致、未改：06-dcm/06:142,395-398（raw 会话值/上锁）；06-dcm/07:405,421（无 DcmSecurityAccess 模式切换）；05-can-stack/03:375 与 07:397,410,430-436（FC 早于 FF 确认被忽略）；05-can-stack/03:492 与 debugging-autosar-diagnostics.md:280（trace 措辞）；07-rte-swc/02:197、08:245、09:295（F1A0） | — |
| I1 | — | 已核对一致、未改：Dcm_ExternalProcessingDone（06-dcm/04:14,146,544,582 —— R20-11 txt 中 0 命中）；Dcm_TxConfirmation=IF/周期传输（05-can-stack/05:371、06-dcm/01:154、02:137、03:274、10:196；SWS_Dcm_01092 p.247）；BswM_Dcm_RequestSessionMode 缺失（06-dcm/06:146、08-integration/03:94、dcm-upgrade-guide.md:78,396）。无矛盾陈述 | — |
| J1 | — | 已核对、未改：无章节断言 P1M-E 有 PROTCMD/软件 PLL/G4MH/CAN 80 MHz；出现处均为"典型 RH850/[Conceptual]/误区"语境（03-mcal/02:255-258、01-rh850/04:341-373、07:273-304、03-mcal/02:651 等） | — |
| B5 | `docs/01-rh850/01-rh850-overview.md:363` | '时钟/复位寄存器靠 Slave Guard' -> '时钟控制器寄存器靠 Slave Guard（p.471）、复位寄存器靠 P-Bus Guard（p.420，PBG 是 slave guard 的一种）' | HW-E p.420 / p.471 / §31.4.1.2 |

### 2.2 Mermaid 语法修正

| ID | 位置（file:line） | before → after | 证据 |
|---|---|---|---|
| M | `docs/debugging-autosar-diagnostics.md:70` | C{ECU: RX ISR<br/>(EI190) 进入了吗?} -> C{"ECU: RX ISR<br/>(EI190) 进入了吗?"} | flowchart 菱形节点文本含未加引号的括号 → 加双引号 |
| M | `docs/debugging-autosar-diagnostics.md:78` | G{Dcm_TpRxIndication(E_OK)<br/>之后 Dcm_MainFunction<br/>处理了请求?} -> G{"Dcm_TpRxIndication(E_OK)<br/>之后 Dcm_MainFunction<br/>处理了请求?"} | flowchart 菱形节点文本含未加引号的括号 → 加双引号 |
| M | `docs/01-rh850/03-memory-map.md:320` | S->>R: 清零 .bss (Dcm_RxCount = 0; 若硬件已清则冗余) -> S->>R: 清零 .bss (Dcm_RxCount = 0，若硬件已清则冗余) | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/01-rh850/05-linker-script.md:203` | LD->>ELF: .data VMA=LRAM, LMA=iROM; 生成 __data_* 符号 -> LD->>ELF: .data VMA=LRAM, LMA=iROM，生成 __data_* 符号 | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/01-rh850/05-linker-script.md:205` | SU->>SU: SP ← __stack_top; GP/EP ← __gp/__ep -> SU->>SU: SP ← __stack_top，GP/EP ← __gp/__ep | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/01-rh850/05-linker-script.md:206` | SU->>SU: EBASE ← __exvect_start; INTBP ← __intvect_start -> SU->>SU: EBASE ← __exvect_start，INTBP ← __intvect_start | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/01-rh850/08-peripheral-overview.md:164` | operating; 通道保持 STOPPED -> operating，通道保持 STOPPED | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/06-dcm/03-dsd.md:359` | reqDataLen+1 = 3 ≥ minReqLen 3 ✓ ; subFuncAvail = FALSE -> reqDataLen+1 = 3 ≥ minReqLen 3 ✓ ， subFuncAvail = FALSE | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/06-dcm/06-diagnostic-session.md:269` | DSP->>DSP: reqDataLen==1 ✓; FindSessionRow(0x03)=1 -> DSP->>DSP: reqDataLen==1 ✓，FindSessionRow(0x03)=1 | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/06-dcm/07-security-access.md:262` | DSP->>DSP: seedLevel = 0（seed 已用掉）; attemptCounter = 0 -> DSP->>DSP: seedLevel = 0（seed 已用掉），attemptCounter = 0 | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/07-rte-swc/05-rte-generation.md:461` | EcuTopComposition: 连线; RTE ECUC: event→task -> EcuTopComposition: 连线，RTE ECUC: event→task | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/08-integration/02-can-stack-integration.md:178` | t=31 FF 上线 (VirtualCanBus_Tick) ; Can_MainFunction_Write -> t=31 FF 上线 (VirtualCanBus_Tick) ， Can_MainFunction_Write | sequenceDiagram 消息含 ";"（Mermaid 语句分隔符）→ 改为 "，" |
| M | `docs/06-dcm/06-diagnostic-session.md:242-250` | stateDiagram 状态名 Default/Extended/Programming -> DefaultSession/ExtendedSession/ProgrammingSession | mermaid@11 parse: "Expecting ID ... got DEFAULT"（default 是保留字） |
| M | `docs/debugging-autosar-diagnostics.md:68` | NRC["不是"无响应"…"] -> NRC["不是“无响应”…"] | 带引号节点文本内嵌 ASCII 双引号；mermaid@11 parse error "got STR" |

### 2.3 标记补全（00-writing-conventions §3）

| ID | 位置（file:line） | before → after | 证据 |
|---|---|---|---|
| K | `docs/02-autosar-classic/05-generated-code.md:444` | /* diag/Dcm_Cfg.c:120 */ -> /* [Educational Implementation] diag/Dcm_Cfg.c:120 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:316` | /* P1M-E manual R01UH0585EJ0120, pp.1543,1551-1567. -> /* [Educational Implementation] [RH850 Hardware] P1M-E manual R01UH0585EJ0120, pp.1543,155 | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:334` | typedef enum { OSTM_OK, OSTM_INVALID, OSTM_BUSY, OSTM_TIMEOUT } Ostm_R -> typedef enum { OSTM_OK, OSTM_INVALID, OSTM_BUSY, OSTM_TIMEOUT } Ostm_Result;   /* [Educati | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:344` | /* Caller owns channel, masks its interrupt, keeps TSST low, and seria -> /* [Educational Implementation] [RH850 Hardware] Caller owns channel, masks its interrupt, | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:354` | /* examples/rh850_mcal_reference/mcal/gpt/Ostm.c */ -> /* [Educational Implementation] [RH850 Hardware] examples/rh850_mcal_reference/mcal/gpt/Os | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:366` | static bool valid(const Rh850_Mmio *io, uint8_t unit)                  -> static bool valid(const Rh850_Mmio *io, uint8_t unit)                     /* [Educational | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:377` | Ostm_Result Ostm_InitPclk(const Rh850_Mmio *io, uint8_t unit, -> Ostm_Result Ostm_InitPclk(const Rh850_Mmio *io, uint8_t unit,  /* [Educational Implementat | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:406` | Ostm_Result Ostm_Start(const Rh850_Mmio *io, uint8_t unit)             -> Ostm_Result Ostm_Start(const Rh850_Mmio *io, uint8_t unit)                /* [Educational | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:420` | Ostm_Result Ostm_Stop(const Rh850_Mmio *io, uint8_t unit, uint32_t pol -> Ostm_Result Ostm_Stop(const Rh850_Mmio *io, uint8_t unit, uint32_t poll_limit)   /* [Educa | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:437` | Ostm_Result Ostm_SetCompare(const Rh850_Mmio *io, uint8_t unit, uint32 -> Ostm_Result Ostm_SetCompare(const Rh850_Mmio *io, uint8_t unit, uint32_t compare)  /* [Edu | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:451` | Ostm_Result Ostm_ReadCounter(const Rh850_Mmio *io, uint8_t unit, uint3 -> Ostm_Result Ostm_ReadCounter(const Rh850_Mmio *io, uint8_t unit, uint32_t *value)  /* [Edu | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/03-mcal/05-gpt-driver.md:463` | bool Ostm_IntervalCompare(uint32_t counter_hz, uint32_t period_us, uin -> bool Ostm_IntervalCompare(uint32_t counter_hz, uint32_t period_us, uint32_t *compare)  /* | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/03-can-clock-bit-timing.md:407` | static bool valid(const Can_Timing *t, bool data_phase) -> static bool valid(const Can_Timing *t, bool data_phase)  /* [Educational Implementation] [ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/03-can-clock-bit-timing.md:441` | if (result == NULL \|\| (fcan_hz != 16000000U && fcan_hz != 40000000U) | -> if (result == NULL \|\| (fcan_hz != 16000000U && fcan_hz != 40000000U) \|\|  /* [Educational I \| 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/03-can-clock-bit-timing.md:458` | ntq = 1U + (uint32_t)nominal->tseg1 + nominal->tseg2; -> ntq = 1U + (uint32_t)nominal->tseg1 + nominal->tseg2;  /* [Educational Implementation] [RH | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/03-can-clock-bit-timing.md:489` | output.nominal_sample_permyriad = (uint16_t)((1U + nominal->tseg1) * 1 -> output.nominal_sample_permyriad = (uint16_t)((1U + nominal->tseg1) * 10000U / ntq);  /* [E | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/09-can-init-implementation.md:454` | static boolean Can_ConfigIsConsistent(const Can_ConfigType *cfg) -> static boolean Can_ConfigIsConsistent(const Can_ConfigType *cfg)  /* [Educational Implemen | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/09-can-init-implementation.md:486` | static void Can_WriteRxRules(const Can_ConfigType *cfg) -> static void Can_WriteRxRules(const Can_ConfigType *cfg)  /* [Educational Implementation] * | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/09-can-init-implementation.md:518` | void Can_Init(const Can_ConfigType *Config) -> void Can_Init(const Can_ConfigType *Config)  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/10-can-write-implementation.md:163` | typedef struct { -> typedef struct {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/12-can-interrupt-implementation.md:147` | static uint8 Can_IrqDisableCnt[CAN_MAX_HW_CHANNELS];   /* 嵌套计数，00202 * -> static uint8 Can_IrqDisableCnt[CAN_MAX_HW_CHANNELS];   /* [Educational Implementation] 嵌套计 | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/12-can-interrupt-implementation.md:195` | static void Can_ForChannel(uint8 m, void (*fn)(uint8)) -> static void Can_ForChannel(uint8 m, void (*fn)(uint8))  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/04-can-mcal/13-can-error-busoff.md:246` | Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId, Can_Err -> Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType *ErrorSt | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/02-dsl.md:370` | /* examples/uds_diag_demo/diag/Dcm_Dsl.c:313-348 (摘录) */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Dsl.c:313-348 (摘录) */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/02-dsl.md:436` | /* examples/uds_diag_demo/diag/Dcm_Dsl.c:451-473 (摘录) */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Dsl.c:451-473 (摘录) */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/02-dsl.md:516` | /* examples/uds_diag_demo/diag/Dcm_Dsl.c:408-417 */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Dsl.c:408-417 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/02-dsl.md:625` | /* examples/uds_diag_demo/diag/Dcm_Dsl.c:477-497 */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Dsl.c:477-497 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/03-dsd.md:450` | /* examples/uds_diag_demo/diag/Dcm_Dsd.c:80-105 (摘录) */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Dsd.c:80-105 (摘录) */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/03-dsd.md:468` | if (svc->subFuncAvail) { -> if (svc->subFuncAvail) {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/03-dsd.md:485` | ret = Dcm_DsdActiveService->fnc(OpStatus, pMsgContext, &nrc); -> ret = Dcm_DsdActiveService->fnc(OpStatus, pMsgContext, &nrc);  /* [Educational Implementat | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/03-dsd.md:507` | static boolean Dcm_DsdNrcSuppressedForFunctional(Dcm_NegativeResponseC -> static boolean Dcm_DsdNrcSuppressedForFunctional(Dcm_NegativeResponseCodeType nrc)  /* [Ed | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/04-dsp.md:288` | typedef struct { -> typedef struct {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/04-dsp.md:410` | /* examples/uds_diag_demo/diag/Dcm_Cfg.c:77-85 */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Cfg.c:77-85 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/04-dsp.md:482` | /* examples/uds_diag_demo/diag/Dcm_Dsp.c:264-367 (结构摘要) */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Dsp.c:264-367 (结构摘要) */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/05-dcm-configuration.md:291` | {   /* VIN: asynchronous C/S interface -> exercises DCM_E_PENDING / Op -> {   /* [Educational Implementation] VIN: asynchronous C/S interface -> exercises DCM_E_PEN | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/05-dcm-configuration.md:393` | /* examples/uds_diag_demo/diag/Dcm_Cfg.c:18-30 */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Cfg.c:18-30 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/05-dcm-configuration.md:411` | /* examples/uds_diag_demo/diag/Dcm_Cfg.c:44-48 */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Cfg.c:44-48 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/05-dcm-configuration.md:422` | /* examples/uds_diag_demo/diag/Dcm_Cfg.c:127-133 */ -> /* [Educational Implementation] examples/uds_diag_demo/diag/Dcm_Cfg.c:127-133 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/06-diagnostic-session.md:221` | Dcm_Dsl.sessionRow = Dcm_DslFindSessionRow(DCM_DEFAULT_SESSION);   /*  -> Dcm_Dsl.sessionRow = Dcm_DslFindSessionRow(DCM_DEFAULT_SESSION);   /* [Educational Impleme | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/06-diagnostic-session.md:407` | Std_ReturnType Dcm_DspDiagnosticSessionControl(Dcm_OpStatusType OpStat -> Std_ReturnType Dcm_DspDiagnosticSessionControl(Dcm_OpStatusType OpStatus, Dcm_MsgContextTy | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/06-diagnostic-session.md:444` | static void Dcm_DslSetSession(uint8 rowIdx, const char *reason) -> static void Dcm_DslSetSession(uint8 rowIdx, const char *reason)  /* [Educational Implement | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/07-security-access.md:213` | typedef struct { -> typedef struct {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/07-security-access.md:435` | uint8 sub = pMsgContext->reqData[0];                     /* SPRMIB 已被  -> uint8 sub = pMsgContext->reqData[0];                     /* [Educational Implementation] S | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/07-security-access.md:450` | if ((sub & 0x01u) != 0u) { -> if ((sub & 0x01u) != 0u) {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/07-security-access.md:474` | if (OpStatus == DCM_INITIAL) { -> if (OpStatus == DCM_INITIAL) {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/06-dcm/07-security-access.md:503` | void Dcm_DspMainFunction(void) -> void Dcm_DspMainFunction(void)  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/01-swc-concept.md:332` | /* swc/VehicleInfoSWC.c:22-24 */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:22-24 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/01-swc-concept.md:343` | /* swc/VehicleInfoSWC.c:34-47  —— ② 内部状态：全部 static */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:34-47  —— ② 内部状态：全部 static */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/01-swc-concept.md:353` | /* swc/VehicleInfoSWC.c:54-60  —— ③ Init runnable（InitEvent） */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:54-60  —— ③ Init runnable（InitEvent） | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/02-port-interface.md:190` | Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusTyp -> Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusType OpStatus, uint8 *D | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/04-rte-concept.md:334` | void Rte_Start(void) -> void Rte_Start(void)  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/04-rte-concept.md:352` | Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusTyp -> Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusType OpStatus, uint8 *D | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/04-rte-concept.md:367` | void SchM_Switch_Dcm_DcmDiagnosticSessionControl(Dcm_SesCtrlType nextM -> void SchM_Switch_Dcm_DcmDiagnosticSessionControl(Dcm_SesCtrlType nextMode)  /* [Educationa | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/05-rte-generation.md:556` | /* rte/Rte_VehicleInfoSWC.h:26-36 */ -> /* [Educational Implementation] rte/Rte_VehicleInfoSWC.h:26-36 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/05-rte-generation.md:572` | /* rte/Rte_VehicleInfoSWC.h:42-47 */ -> /* [Educational Implementation] rte/Rte_VehicleInfoSWC.h:42-47 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/06-client-server.md:280` | Std_ReturnType VehicleInfoSWC_ReadVin(Dcm_OpStatusType OpStatus, uint8 -> Std_ReturnType VehicleInfoSWC_ReadVin(Dcm_OpStatusType OpStatus, uint8 *Data)  /* [Educati | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/06-client-server.md:308` | if (OpStatus == DCM_INITIAL) { -> if (OpStatus == DCM_INITIAL) {  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/08-dcm-rte-integration.md:469` | * What the RTE does on Rte_Call for a synchronous server call point wh -> * [Educational Implementation] What the RTE does on Rte_Call for a synchronous server call | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/08-dcm-rte-integration.md:489` | Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusTyp -> Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusType OpStatus, uint8 *D | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/08-dcm-rte-integration.md:517` | void SchM_Switch_Dcm_DcmDiagnosticSessionControl(Dcm_SesCtrlType nextM -> void SchM_Switch_Dcm_DcmDiagnosticSessionControl(Dcm_SesCtrlType nextMode)  /* [Educationa | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/09-diagnostic-swc-example.md:192` | /* swc/VehicleInfoSWC.c:98-103 */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:98-103 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/09-diagnostic-swc-example.md:214` | /* swc/VehicleInfoSWC.c:77-96 */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:77-96 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/09-diagnostic-swc-example.md:252` | /* swc/VehicleInfoSWC.c:105-109 */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:105-109 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/09-diagnostic-swc-example.md:263` | /* swc/VehicleInfoSWC.c:114-142（节选） */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:114-142（节选） */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/09-diagnostic-swc-example.md:348` | /* swc/SecurityAccessSWC.c:34-54（节选） */ -> /* [Educational Implementation] swc/SecurityAccessSWC.c:34-54（节选） */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/07-rte-swc/09-diagnostic-swc-example.md:389` | /* swc/VehicleInfoSWC.c:54-60 */ -> /* [Educational Implementation] swc/VehicleInfoSWC.c:54-60 */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/autosar-swc-rte-tutorial.md:274` | {   0xF190u, 17u, DCM_USE_DATA_ASYNCH_CLIENT_SERVER, -> {   0xF190u, 17u, DCM_USE_DATA_ASYNCH_CLIENT_SERVER,  /* [Educational Implementation] */ | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |
| K | `docs/autosar-swc-rte-tutorial.md:283` | Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusTyp -> Std_ReturnType Rte_Call_DataServices_DID_F190_ReadData(Dcm_OpStatusType OpStatus, uint8 *D | 代码块缺少约定标记（00-writing-conventions §3）；所在小节无任何标记 |

### 2.4 Legacy 文档 banner

| ID | 位置（file:line） | before → after | 证据 |
|---|---|---|---|
| L | `docs/agent-guide.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/mcal-reference-guide.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/hardware-findings.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/rh850-hardware-handoff.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/internal-agent-task.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/counter-design.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/requirements-status.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |
| L | `docs/hardware-review.md:3-8` | （无）-> 插入 6 行 Legacy banner（教育项目声明、迁移去向、已知更正、行号 +6 说明） | 研究笔记 01 §1.1/§3；任务 4 |

每个 banner 包含：① 上一阶段产物、不再维护；② **本仓库是教育项目**，“内部 agent / 内部工程 / 交付”只是旧语境（`internal-agent-task.md` 另有“特别说明”，明确其“给公司内部 agent 的指令”框架不构成对任何真实项目的指令）；③ 迁移去向（章节链接，依据研究笔记 01 §3 的 split 建议）；④ 已知更正；⑤ 行号 +6 说明。

## 3. 追加到 demo README 的内容

`examples/uds_diag_demo/README.md` 第 325 行起（原 324 行之后，只追加，原行号不变）：“## 已知与规范的偏差 (Known deviations)”，列 D1–D6：

| # | 偏差 | 源码确认位置 |
|---|---|---|
| D1 | F1A0 读 SYNCH / 写 ASYNCH 签名（同一 `DcmDspData` 只能有一个 UsePort，`SWS_Dcm_00794`） | `diag/Dcm_Cfg.c:44-48` |
| D2 | 所有会话转换（含默认→默认）都锁安全级（`SWS_Dcm_00139` 不要求） | `diag/Dcm_Dsl.c:114-115` |
| D3 | 安全级变化不做 `DcmSecurityAccess` 模式切换（`SWS_Dcm_01329`） | `diag/Dcm_Dsl.c:102-106` |
| D4 | 原始 UDS 会话值当 RTE 模式值 | `diag/Dcm_Dsl.c:121`、`rte/Rte_Dcm.c:134-144` |
| D5 | FF TxConfirmation 前到达的 FC 被丢弃 | `com/CanTp.c:430-433` |
| D6 | `E_NOT_OK` 时 trace 仍打印 “response on the bus” | `diag/Dcm_Dsl.c:493` |

## 4. 未处理 / 留给其他 owner

- `docs/online-references.md:28` 仍有“本次内部 agent 解说已交付”的旧语境（不在本次范围）。
- `docs/rh850-autosar-tutorial-content.md:231/333`（OSTM0 归属、`return CAN_OK` 未标版本）属 index agent，未改。
- 研究笔记 01 / `analysis-and-learning-plan.md` 中引用 legacy 文档的行号（如 `agent-guide.md:71`、`hardware-findings.md:86`）是 banner 插入前的行号，当前需 +6；本次未改这些引用，以免与研究笔记不一致。
- 研究笔记 `04-rh850-hardware-notes.md:246` 写“时钟控制器和复位寄存器 … 靠 P-Bus Guard（PBG）”——时钟控制器一侧与 HW-E p.471 原文（“Slave Guards”）措辞不符，建议 owner 改为“时钟控制器 = Slave Guard（p.471）；复位寄存器 = P-Bus Guard（p.420）”（属 reference/，本次未改）。
- openAUTOSAR 摘录代码块使用 `[openAUTOSAR]` 而非约定的六种标记之一，保留原样；建议在 00-writing-conventions 中把它列为允许的来源标签。
