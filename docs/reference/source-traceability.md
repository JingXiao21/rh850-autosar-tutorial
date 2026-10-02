# 源码追踪表：AUTOSAR API → SWS → openAUTOSAR → 本项目 → RH850 → 章节

> 对应要求: `claude_plan.md` §18 “建立代码追踪表”（CAN 与 DCM 各一张，不猜路径）
> 对应规范: SWS CAN Driver **R22-11**、SWS DCM **R20-11**、SWS MCU **R24-11**（页码均为 PDF 物理页，可在 `artifacts/pdf-text/*.txt` 中按 `=== PDF PAGE n ===` 复核）。**本仓库没有** CanIf / CanTp / PduR / Dem / Rte / Port / Gpt / Os SWS——这些行的 SWS 列写“本仓库无该 SWS”，只列出 CAN/DCM SWS 中“作为对接方”出现的需求。
> 对应源码: openAUTOSAR = `D:\side_project\openAUTOSAR\`（Arctic Core 2.18.0，**R3.1.5 风格**）；本项目 = `examples/uds_diag_demo/`、`examples/rh850_mcal_reference/`、`docs/04-can-mcal/14-can-driver-from-scratch.md`（capstone 教学驱动，代码嵌在 Markdown 中，行号为 Markdown 文件行号）
> 硬件依据: RH850/P1M-E User's Manual: Hardware R01UH0585EJ0120（下文 “HW-E p.N”）

---

## 0. 如何使用与核验方式

- **每一个 `file:line` 都经过 grep 确认**（2026-10-02，对照当前仓库与 openAUTOSAR 工作副本）。研究笔记 `03-openautosar-trace.md` 中个别行号有偏差（例如 `PduR_CanIfRxIndication` 实际在 `PduR_CanIf.c:21` 而非 `:89`），本表以重新 grep 的结果为准。
- 路径简写：
  - `oA:` = openAUTOSAR 根目录；
  - `demo:` = `examples/uds_diag_demo/`；
  - `ref:` = `examples/rh850_mcal_reference/`；
  - `cap:` = `docs/04-can-mcal/14-can-driver-from-scratch.md`（行号指向 Markdown 中代码块里的定义行）。
- “不存在” = 在该代码库中 grep 不到对应实现。openAUTOSAR 列若给出的是 R3 等价函数，会注明“R3 等价”。
- 标签：本项目代码均为 `[Educational Implementation]`；openAUTOSAR 为参考实现（非 R4.x）；RH850 列为 `[RH850 Hardware]` 事实，页码来自研究笔记 04（已对照手册）。

---

## 1. CAN 追踪表（Can → CanIf → CanTp → PduR）

| AUTOSAR API | SWS（release, page） | openAUTOSAR file:line | 本项目 file:line | RH850 hardware | 教程章节 |
|---|---|---|---|---|---|
| `Can_Init` | `SWS_Can_00223`，CAN R22-11 p.62–63 | 声明 `oA:include/Can.h:313`；调用 `oA:system/EcuM/src/EcuM_Callout_Stubs.c:308`；**定义不存在**（`oA:boards/linuxOs/MCAL/Can/` 只有 `include/Can_Cfg.h`） | `demo:mcal/Can.c:51`；`cap:672`；调用 `demo:integration/EcuM.c:28` | RSCFD0 基址 `0xFFD2_0000`；GSTS.GRAMINIT→GCTR global reset→GRMCFG→GCFG→CmCFG→GAFL 规则→RFCCx→global operating（HW-E p.791, p.802, p.1090–1091） | [04-can-mcal/09](../04-can-mcal/09-can-init-implementation.md)、[06](../04-can-mcal/06-can-controller-init.md) |
| `Can_SetControllerMode` | `SWS_Can_00230`，p.66–67（Asynchronous） | 声明 `oA:include/Can.h:321`（R3：返回 `Can_ReturnType`，参数 `Can_StateTransitionType`）；定义不存在 | `demo:mcal/Can.c:106`；`cap:821`；上层 `demo:ecual/CanIf.c:38` | CmCTR.CHMDC（00 communication / 01 reset / 10 halt），CmSTS 确认（HW-E p.805–811, p.1065–1066） | [04-can-mcal/06](../04-can-mcal/06-can-controller-init.md)、[12](../04-can-mcal/12-can-interrupt-implementation.md) |
| `Can_MainFunction_Mode` | `SWS_Can_00368`，p.87 | 不存在（`oA:include/Can.h:330-334` 无此声明） | `demo:mcal/Can.c:147`；`cap:873` | 轮询 CmSTS（HW-E p.810） | [04-can-mcal/12](../04-can-mcal/12-can-interrupt-implementation.md) |
| `Can_Write` | `SWS_Can_00233`，p.80–81；`SWS_Can_00213`（BUSY）p.81；`SWS_Can_00276` p.45 | 声明 `oA:include/Can.h:327`（R3：HTH 类型 `Can_Arc_HTHType`）；调用 `oA:communication/CAN/CanIf/src/CanIf.c:470`；定义不存在 | `demo:mcal/Can.c:171`（写 TX buffer `:210`，保存 `swPduHandle` `:212`）；`cap:952`；调用 `demo:ecual/CanIf.c:89` | TX buffer p：TMIDp/TMPTRp/TMDF0_p/TMDF1_p，TMCp.TMTR=1（**TMCp 仅 8 位访问**）（HW-E p.878–887, p.1107） | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| `Can_MainFunction_Write` | `SWS_Can_00225`，p.84–85；`SWS_Can_00016` p.45 | 声明 `oA:include/Can.h:330`；调度 `oA:system/SchM/include/SchM_Can.h:19`、`oA:system/SchM/src/SchM.c:396`；定义不存在 | `demo:mcal/Can.c:220`（调确认 `:235`）；`cap:1100` | TMSTSp.TMTRF=10B 判定完成，写 00B 清除（HW-E p.880–881, p.1109–1110） | [04-can-mcal/12](../04-can-mcal/12-can-interrupt-implementation.md) |
| `Can_MainFunction_Read` | `SWS_Can_00226`，p.85；`SWS_Can_00279` p.48 | 声明 `oA:include/Can.h:331`；调度 `oA:system/SchM/include/SchM_Can.h:20`、`SchM.c:397`；定义不存在 | `demo:mcal/Can.c:242`（INTERRUPT 模式，空函数）；`cap:1109` | RFSTSx.RFEMP / RFPCTRx=0xFF（HW-E p.846–848, p.1102） | [04-can-mcal/11](../04-can-mcal/11-can-rx-implementation.md)、[12](../04-can-mcal/12-can-interrupt-implementation.md) |
| Can RX ISR（非标准名） | `SWS_Can_00033` p.33；`SWS_Can_00420` p.33 | 不存在 | `demo:mcal/Can.c:255`（`Can_Isr_GlobalRxFifo`）；`cap:1143`（`Can_Internal_IsrRxFifo`）、`cap:1174`（`ISR(Can_Isr_RxFifo)`） | **EI190 INTRCANGRECC**（RX FIFO0–7 共用），不是 EI184（HW-E p.286, p.792） | [04-can-mcal/05](../04-can-mcal/05-can-interrupt.md)、[12](../04-can-mcal/12-can-interrupt-implementation.md) |
| `Can_MainFunction_BusOff` | `SWS_Can_00227`，p.86；`SWS_Can_00020` p.42；`SWS_Can_00274` p.43 | 声明 `oA:include/Can.h:332`；调度 `oA:system/SchM/include/SchM_Can.h:21`、`SchM.c:398`；定义不存在 | **demo 中不存在**；`cap:1118`（处理 `cap:1083`）；错误 ISR `cap:1170` | CmERFL.BOEF、CmSTS.BOSTS、CmCTR.BOM（01=进入 bus-off 转 halt）；INTRCANmERR = EI183/186/191（HW-E p.807, p.810–815, p.285–286） | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| `CanIf_RxIndication` | 本仓库无 CanIf SWS；被调用要求 `SWS_Can_00279` p.48、必需回调 `SWS_Can_00234` p.88 | `oA:communication/CAN/CanIf/src/CanIf.c:764`（**R3 四参数签名**；`CanIf_Cbk.h:27`）；CanTp 分支 `CanIf.c:868` 需 `USE_CANTP` | `demo:ecual/CanIf.c:160`（匹配 `:173`，上调 `:182`）；调用 `demo:mcal/Can.c:275`；`cap` 中为 stub `cap:1474` | 上下文：RX ISR（EI190）或 `Can_MainFunction_Read` | [05-can-stack/01](../05-can-stack/01-canif.md)、[06](../05-can-stack/06-can-rx-path.md) |
| `CanIf_Transmit` | 本仓库无 CanIf SWS；CAN SWS p.51：`CAN_BUSY` 时 CanIf 排队 | `oA:communication/CAN/CanIf/src/CanIf.c:424`（无 Tx 缓冲） | `demo:ecual/CanIf.c:93`（Tx 缓冲 `:116-131`）；调用 `demo:com/CanTp.c:108` | 间接：经 `Can_Write` 到 TX buffer | [05-can-stack/01](../05-can-stack/01-canif.md)、[07](../05-can-stack/07-can-tx-path.md) |
| `CanIf_TxConfirmation` | 本仓库无 CanIf SWS；`SWS_Can_00016` p.45 | `oA:communication/CAN/CanIf/src/CanIf.c:743`（经函数指针上报 `:758`） | `demo:ecual/CanIf.c:136`（上报 `:155`）；调用 `demo:mcal/Can.c:235` | TMTRF=10B / INTRCANmTRX（EI185/188/193）（HW-E p.1109, p.792） | [05-can-stack/07](../05-can-stack/07-can-tx-path.md) |
| `CanIf_ControllerBusOff` | 本仓库无 CanIf SWS；`SWS_Can_00020` p.42 | `oA:communication/CAN/CanIf/src/CanIf.c:919` | `demo:ecual/CanIf.c:69`（demo 不产生 bus-off）；`cap:1094` 调用 | 见 `Can_MainFunction_BusOff` 行 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| `CanIf_ControllerModeIndication` | 本仓库无 CanIf SWS；`SWS_Can_00234` p.88 | 不存在（R3 CanIf） | `demo:ecual/CanIf.c:56`；调用 `demo:mcal/Can.c:164` | CmSTS 模式位 | [04-can-mcal/06](../04-can-mcal/06-can-controller-init.md) |
| `CanTp_RxIndication` | 本仓库无 CanTp SWS | `oA:communication/CAN/CanTp/src/CanTp.c:1001`（依赖 `CanTpRxIdList`，示例配置未初始化） | `demo:com/CanTp.c:466` | 无（纯软件；仍在 RX ISR 上下文） | [05-can-stack/03](../05-can-stack/03-cantp.md)、[04](../05-can-stack/04-isotp.md) |
| `CanTp_Transmit` | 本仓库无 CanTp SWS | `oA:communication/CAN/CanTp/src/CanTp.c:892` | `demo:com/CanTp.c:392`；调用 `demo:com/PduR.c:75`（经路由表函数指针） | 无 | [05-can-stack/03](../05-can-stack/03-cantp.md) |
| `CanTp_TxConfirmation` | 本仓库无 CanTp SWS | `oA:communication/CAN/CanTp/src/CanTp.c:1111`（R3：无 result 参数） | `demo:com/CanTp.c:501`；调用 `demo:ecual/CanIf.c:155` | 无 | [05-can-stack/03](../05-can-stack/03-cantp.md) |
| `CanTp_MainFunction` | 本仓库无 CanTp SWS | `oA:communication/CAN/CanTp/src/CanTp.c:1172`；周期宏 `oA:communication/CAN/CanTp/include/CanTp_Cfg.h:24`（1000 ms） | `demo:com/CanTp.c:590`；调度 `demo:integration/BswScheduler.c:41`（1 ms 任务） | OS 任务（时基来自 OSTM，见 MCAL 表） | [05-can-stack/03](../05-can-stack/03-cantp.md)、[02-autosar-classic/07](../02-autosar-classic/07-mainfunction-scheduling.md) |
| `PduR_CanTpStartOfReception` | 本仓库无 PduR SWS；对端 `Dcm_StartOfReception` `SWS_Dcm_00094` p.243 | **不存在**；R3 等价 `PduR_CanTpProvideRxBuffer` `oA:communication/ComServices/PDURouter/src/PduR_CanTp.c:25`（被 `PduR_Cfg.h:86` 宏改名为 `Dcm_ProvideRxBuffer`） | `demo:com/PduR.c:80`；调用 `demo:com/CanTp.c:204`（SF）、`:246`（FF） | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| `PduR_CanTpCopyRxData` | 本仓库无 PduR SWS；对端 `SWS_Dcm_00556` p.244 | 不存在（R3 用“借整块 buffer”） | `demo:com/PduR.c:92`；调用 `demo:com/CanTp.c:213, :257, :296` | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| `PduR_CanTpRxIndication` | 本仓库无 PduR SWS；对端 `SWS_Dcm_00093` p.245 | `oA:communication/ComServices/PDURouter/src/PduR_CanTp.c:30`（R3，`NotifResultType`；宏改名 `PduR_Cfg.h:87`） | `demo:com/PduR.c:101`；调用 `demo:com/CanTp.c:214, :309` | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| `PduR_CanTpCopyTxData` | 本仓库无 PduR SWS；对端 `SWS_Dcm_00092` p.245 | 不存在；R3 等价 `PduR_CanTpProvideTxBuffer` `oA:.../PDURouter/src/PduR_CanTp.c:34`（宏改名 `PduR_Cfg.h:88`） | `demo:com/PduR.c:111`；调用 `demo:com/CanTp.c:365` | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| `PduR_CanTpTxConfirmation` | 本仓库无 PduR SWS；对端 `SWS_Dcm_00351` p.247 | `oA:.../PDURouter/src/PduR_CanTp.c:39`（宏改名 `PduR_Cfg.h:89`） | `demo:com/PduR.c:121`；调用 `demo:com/CanTp.c:543` | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| `PduR_CanIfRxIndication`（IF 路由，非诊断） | 本仓库无 PduR SWS | `oA:.../PDURouter/src/PduR_CanIf.c:21`（被 `PduR_Cfg.h:78` 宏改名为 `Com_RxIndication`）；`PduR_CanIfTxConfirmation` `:25` | demo 中不存在（诊断走 TP 路由） | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md) |

### 1.1 CAN 配置根

| 配置根 | openAUTOSAR | 本项目 |
|---|---|---|
| Can | 类型 `oA:boards/linuxOs/MCAL/Can/include/Can_Cfg.h:206-214`，`extern` 于 `:217-219`，**实例不存在** | `demo:mcal/Can_Cfg.c:23`（`Can_Config`）；`cap:1216`（`Can_Config0`） |
| CanIf | `oA:communication/CAN/CanIf/src/CanIf_Cfg.c:168`（无 CanTp PDU） | `demo:ecual/CanIf_Cfg.c:25` |
| CanTp | `oA:communication/CAN/CanTp/src/CanTp_Cfg.c:136`（缺 `CanTpRxIdList`） | `demo:com/CanTp_Cfg.c:36` |
| PduR | `oA:communication/ComServices/PDURouter/include/PduR_PbCfg.h:34` 仅 `extern`，实例不存在 | `demo:com/PduR_Cfg.c:27` |

---

## 2. DCM 追踪表（PduR → Dcm DSL/DSD/DSP → Rte → SWC / Dem）

| AUTOSAR API | SWS（release, page） | openAUTOSAR file:line | 本项目 file:line | RH850 hardware | 教程章节 |
|---|---|---|---|---|---|
| `PduR_DcmTransmit` | `SWS_Dcm_00115`，DCM R20-11 p.59（DCM 经 PduR 发送）；PduR SWS 本仓库无 | `oA:communication/ComServices/PDURouter/src/PduR_Dcm.c:21`；调用 `oA:diagnostic/Dcm/src/Dcm_Dsl.c:410, :596, :607`；**被 `PduR_Cfg.h:127` 宏改名为 `CanTp_Transmit`** | `demo:com/PduR.c:67`；调用 `demo:diag/Dcm_Dsl.c:199`（最终响应）、`:221`（NRC 0x78） | 无 | [05-can-stack/05](../05-can-stack/05-pdur.md)、[06-dcm/02](../06-dcm/02-dsl.md) |
| `Dcm_Init` | `SWS_Dcm_00037`，p.236 | `oA:diagnostic/Dcm/src/Dcm.c:79`（**R3：`Dcm_Init(void)`**）；调用 `oA:system/EcuM/src/EcuM_Callout_Stubs.c:360` | `demo:diag/Dcm.c:16`；调用 `demo:integration/EcuM.c:36` | 无 | [06-dcm/01](../06-dcm/01-dcm-overview.md)、[02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md) |
| `Dcm_MainFunction` | `SWS_Dcm_00053`，p.260（`SchM_Dcm.h`） | `oA:diagnostic/Dcm/src/Dcm.c:96`（DsdMain/DspMain/DslMain `:100-102`）；调度宏 `oA:system/SchM/include/SchM_Dcm.h:20`、`oA:system/SchM/src/SchM.c:408` | `demo:diag/Dcm.c:33`；调度 `demo:integration/BswScheduler.c:50`（10 ms） | CPU 任务上下文（OS task，时基通常由 OSTM0/1 提供，EI74/75，HW-E p.1543–1544） | [06-dcm/12](../06-dcm/12-dcm-mainfunction.md) |
| `Dcm_StartOfReception` | `SWS_Dcm_00094`，p.243–244 | **不存在**；R3 等价 `Dcm_ProvideRxBuffer` `oA:diagnostic/Dcm/src/Dcm.c:109` → `DslProvideRxBufferToPdur` `Dcm_Dsl.c:682` | `demo:diag/Dcm_Dsl.c:309` | 无（可能在 RX ISR 上下文被调用） | [06-dcm/02](../06-dcm/02-dsl.md) |
| `Dcm_CopyRxData` | `SWS_Dcm_00556`，p.244–245 | 不存在（R3 由 CanTp 直接写入借来的 buffer） | `demo:diag/Dcm_Dsl.c:352` | 无 | [06-dcm/02](../06-dcm/02-dsl.md) |
| `Dcm_TpRxIndication` | `SWS_Dcm_00093`，p.245 | 不存在；R3 等价 `Dcm_RxIndication` `oA:diagnostic/Dcm/src/Dcm.c:124` → `DslRxIndicationFromPduR` `Dcm_Dsl.c:743` | `demo:diag/Dcm_Dsl.c:384`（启动 P2 `:433`） | 无 | [06-dcm/02](../06-dcm/02-dsl.md)、[11](../06-dcm/11-dcm-runtime-flow.md) |
| `Dcm_CopyTxData` | `SWS_Dcm_00092`，p.245–246 | 不存在；R3 等价 `Dcm_ProvideTxBuffer` `oA:diagnostic/Dcm/src/Dcm.c:174` → `DslProvideTxBuffer` `Dcm_Dsl.c:899` | `demo:diag/Dcm_Dsl.c:441` | 无 | [06-dcm/02](../06-dcm/02-dsl.md) |
| `Dcm_TpTxConfirmation` | `SWS_Dcm_00351`，p.247 | 不存在；R3 等价 `Dcm_TxConfirmation` `oA:diagnostic/Dcm/src/Dcm.c:188` → `DslTxConfirmation` `Dcm_Dsl.c:950` | `demo:diag/Dcm_Dsl.c:477` | 无 | [06-dcm/02](../06-dcm/02-dsl.md) |
| DSD 分发（内部，非标准 API） | `SWS_Dcm_00221` p.100；校验顺序 `SWS_Dcm_01535` p.94；未知 SID `SWS_Dcm_00197` p.93 | `oA:diagnostic/Dcm/src/Dcm_Dsd.c:278`（`DsdHandleRequest`）→ `selectServiceFunction` `:86`（case 需 `DCM_USE_SERVICE_*`，全仓未定义） | `demo:diag/Dcm_Dsd.c:144`（检查 `:77`，查表 `:67`，调用 handler `:163`）；由 `demo:diag/Dcm_Dsl.c:256`（INITIAL）/`:260`（PENDING）调用 | 无 | [06-dcm/03](../06-dcm/03-dsd.md) |
| DSP 0x10 DiagnosticSessionControl | `SWS_Dcm_00250` p.114；`SWS_Dcm_00311` p.114 | `oA:diagnostic/Dcm/src/Dcm_Dsp.c:469`（case `Dcm_Dsd.c:92`） | `demo:diag/Dcm_Dsp.c:110`；会话切换在 DSL `demo:diag/Dcm_Dsl.c:121`（`SchM_Switch`） | 无 | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| DSP 0x11 ECUReset | `SWS_Dcm_00260` p.114；`SWS_Dcm_00373/00594` p.115 | `oA:.../Dcm_Dsp.c:530`；Tx 确认后复位 `Dcm_Dsp.c:1968`（`Mcu_PerformReset` `:1977`） | `demo:diag/Dcm_Dsp.c:147`（`SchM_Switch` HARD/SOFT `:160`）；EXECUTE `demo:diag/Dcm_Dsl.c:178` → `demo:rte/Rte_Dcm.c:146` | 最终复位：SWSRESA0 / SWARESA0，复位原因 RESF `0xFFF8_1000`（HW-E p.418–425） | [06-dcm/10](../06-dcm/10-uds-services.md) |
| DSP 0x14 ClearDiagnosticInformation | `SWS_Dcm_00005` p.116（`Dem_ClearDTC`） | `oA:.../Dcm_Dsp.c:581`（调用 `Dem_ClearDTC` `:590`，R3 三参数） | `demo:diag/Dcm_Dsp.c:170`（`Dem_SelectDTC` `:187`，`Dem_ClearDTC` `:192`） | 无（Dem→NvM→Fls/Fee 时才落到 Data Flash） | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| DSP 0x19 ReadDTCInformation | p.112–135；0x02 `SWS_Dcm_00377` p.119；`SWS_Dcm_00835` p.113 | `oA:.../Dcm_Dsp.c:1091`（`Dem_SetDTCFilter` `:651`，`Dem_GetNextFilteredDTC` `:777`） | `demo:diag/Dcm_Dsp.c:216`（`Dem_SetDTCFilter` `:238`，`Dem_GetNextFilteredDTC` `:243`） | 无 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| DSP 0x22 ReadDataByIdentifier | p.135–141；`SWS_Dcm_00437` p.138 | `oA:.../Dcm_Dsp.c:1386`（`readDidData` `:1223`，回调 `:1254`，**C 函数指针，无 RTE**） | `demo:diag/Dcm_Dsp.c:264`（DID 查表 `:57`，同步 `:345`，异步 `:349`） | 无 | [06-dcm/08](../06-dcm/08-did.md) |
| DSP 0x27 SecurityAccess | p.142–144；`SWS_Dcm_00321/00324` p.142；`SWS_Dcm_00863` p.143 | `oA:.../Dcm_Dsp.c:1614`（`GetSeed` `:1648`，`CompareKey` `:1694`；无 0x36/0x37） | `demo:diag/Dcm_Dsp.c:383`（GetSeed `:424`，CompareKey `:454`，尝试计数 `:468`） | 无 | [06-dcm/07](../06-dcm/07-security-access.md) |
| DSP 0x2E WriteDataByIdentifier | p.174–177；`SWS_Dcm_00395` p.175 | `oA:.../Dcm_Dsp.c:1578` | `demo:diag/Dcm_Dsp.c:487`（`writeAsync` `:521`） | NvM 写最终到 Data Flash `0xFF20_0000`（HW-E p.257），本项目 NvM 为 stub | [06-dcm/08](../06-dcm/08-did.md)、[10](../06-dcm/10-uds-services.md) |
| DSP 0x31 RoutineControl | `SWS_Dcm_00257` p.188；`SWS_Dcm_00400` p.193 | `oA:.../Dcm_Dsp.c:1832`（`DspStartRoutineFnc` `:1762`） | `demo:diag/Dcm_Dsp.c:540`（选择 start/stop/results `:573`） | 无 | [06-dcm/10](../06-dcm/10-uds-services.md) |
| DSP 0x3E TesterPresent | `SWS_Dcm_00251` p.196；功能寻址 `3E 80` 旁路 `SWS_Dcm_00112/00113` p.58 | `oA:.../Dcm_Dsp.c:1897` | `demo:diag/Dcm_Dsp.c:623`；功能寻址旁路 `demo:diag/Dcm_Dsl.c:408-416` | 无 | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| `Rte_Call_DataServices_<Data>_ReadData` | 接口 `SWS_Dcm_00686` p.341；C 原型 sync `SWS_Dcm_00793` / async `SWS_Dcm_91006` p.269 | **不存在**：`oA:diagnostic/Dcm/include/Rte_Dcm.h` 只有 include guard（`:23-28`）；DID 用 `Dcm_Lcfg.h:181` 的函数指针 | F190 async `demo:rte/Rte_Dcm.c:54`；F187 sync `:64`；F1A0 `:70`；原型 `demo:rte/Rte_Dcm.h:30-32`；配置引用 `demo:diag/Dcm_Cfg.c:36, :41, :46` | 无 | [07-rte-swc/08](../07-rte-swc/08-dcm-rte-integration.md)、[06](../07-rte-swc/06-client-server.md) |
| `Rte_Call_DataServices_<Data>_WriteData` | `SWS_Dcm_91008` p.271（async 定长） | 不存在 | `demo:rte/Rte_Dcm.c:76`；原型 `demo:rte/Rte_Dcm.h:33` | 无 | [07-rte-swc/08](../07-rte-swc/08-dcm-rte-integration.md) |
| `Rte_Call_SecurityAccess_<Level>_GetSeed/CompareKey` | `SWS_Dcm_00685` p.338；C 原型 `91003/91004` p.266 | 不存在；R3 函数指针 `oA:diagnostic/Dcm/include/Dcm_Lcfg.h:121-122` | `demo:rte/Rte_Dcm.c:89, :97` → `demo:swc/SecurityAccessSWC.c:34, :56` | 无 | [06-dcm/07](../06-dcm/07-security-access.md) |
| `Rte_Call_RoutineServices_<Routine>_Start/Stop/RequestResults` | `SWS_Dcm_00690` p.362 | 不存在 | `demo:rte/Rte_Dcm.c:107, :115, :123`；适配 `demo:diag/Dcm_Cfg.c:55-85` | 无 | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md) |
| SWC server runnable（F190） | — | 不存在（`oA:examples/rte_simple/` 只演示通用 `Rte_Call`，`Tester.c:16`） | `demo:swc/VehicleInfoSWC.c:77`（`VehicleInfoSWC_ReadVin`） | 无 | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md)、[08-integration/04](../08-integration/04-f190-vin-demo.md) |
| SWC 周期 runnable | — | `oA:rte/src/rte.c:27`（空函数 `Rte_Runnable_10ms`） | `demo:swc/VehicleInfoSWC.c:62`（`VehicleInfoSWC_Run10ms`）← `demo:rte/Rte_Dcm.c:46`（`Rte_Task_10ms`） | OS 任务 | [07-rte-swc/03](../07-rte-swc/03-runnable-event.md) |
| `SchM_Switch_<bsnp>_DcmDiagnosticSessionControl` | `SWS_Dcm_00311` p.114 | 不存在（R3 用 callout `Dcm_DiagnosticSessionControl`，只声明无实现） | `demo:rte/Rte_Dcm.c:134`；声明 `demo:rte/SchM_Dcm.h:18`；调用 `demo:diag/Dcm_Dsl.c:121` | 无 | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| `Dem_ClearDTC` | 本仓库无 Dem SWS；DCM 侧 `SWS_Dcm_00005` p.116 | `oA:diagnostic/Dem/src/Dem.c:2998`（R3 三参数） | `demo:diag/Dem.c:56`（stub，ClientId 形态） | 无 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| `Dem_SetDTCFilter` / `Dem_GetNextFilteredDTC` | 本仓库无 Dem SWS；DCM 侧 `SWS_Dcm_00835` p.113 | `oA:diagnostic/Dem/src/Dem.c:2834` / `:2951`（R3 签名，无 ClientId） | `demo:diag/Dem.c:92` / `:110` | 无 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| `Dem_SelectDTC` | 本仓库无 Dem SWS；DCM 侧 `SWS_Dcm_01263` p.116 | 不存在（R3 Dem 无 select 接口） | `demo:diag/Dem.c:45` | 无 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |

### 2.1 DCM 配置根

| 配置根 | openAUTOSAR | 本项目 |
|---|---|---|
| Dcm | 类型 `oA:diagnostic/Dcm/include/Dcm_Lcfg.h:634`；`extern const Dcm_ConfigType DCM_Config;` `:641`；**实例不存在** | `demo:diag/Dcm_Cfg.c:127` |
| 服务开关 | `DCM_USE_SERVICE_*` 全仓未定义（`oA:diagnostic/Dcm/src/Dcm_Dsd.c:91` 等 `#ifdef`） | 服务表 `demo:diag/Dcm_Cfg.c:114-125` |

---

## 3. MCAL / 启动追踪表（Mcu、Port、Gpt/OSTM、Dio）

| AUTOSAR API | SWS（release, page） | openAUTOSAR file:line | 本项目 file:line | RH850 hardware | 教程章节 |
|---|---|---|---|---|---|
| `Mcu_Init` | `SWS_Mcu_00153`，MCU R24-11 p.24 | `oA:boards/linuxOs/MCAL/Mcu/src/Mcu.c:345`（Mcu AR 2.2.2，PowerPC 遗留）；调用 `oA:system/EcuM/src/EcuM_Callout_Stubs.c:193` | 无 C 实现（教程表格 `docs/03-mcal/02-mcu-driver.md:86`） | P1M-E 无软件 PLL 寄存器；时钟固定 CPU 160 / HSB 80 / LSB 40 MHz（HW-E p.469–471） | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| `Mcu_InitClock` | `SWS_Mcu_00155`，p.26–27 | `oA:.../Mcu/src/Mcu.c:382`；调用 `EcuM_Callout_Stubs.c:197` | 概念伪代码 `docs/03-mcal/02-mcu-driver.md:261`、`:321`（**[Conceptual]，不适用于 P1M-E**） | 无 PROTCMD；0xA5 序列只用于 CLMA/ECM/FLMDCNT（HW-E p.2764, p.2795, p.2870–2871） | [03-mcal/02](../03-mcal/02-mcu-driver.md)、[01-rh850/07](../01-rh850/07-clock-system.md) |
| `Mcu_PerformReset` | `SWS_Mcu_00160`，p.31 | `oA:.../Mcu/src/Mcu.c:485`；DCM 0x11 调用 `oA:diagnostic/Dcm/src/Dcm_Dsp.c:1977` | 教学伪代码 `docs/03-mcal/02-mcu-driver.md:544`；demo 模拟 `demo:integration/EcuM.c:54` | SWSRESA0（System Reset 2）/ SWARESA0（Application Reset 1）（HW-E p.418, p.424–425） | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| `Mcu_GetResetReason` | `SWS_Mcu_00158`，p.29 | `oA:.../Mcu/src/Mcu.c:435`；调用 `oA:system/EcuM/src/EcuM.c:137` | 无 | RESF `0xFFF8_1000`（HW-E p.420–422） | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| `Port_Init` | 本仓库无 Port SWS；CAN 引脚由 Port 配置 `SWS_Can_00239` p.22 | `oA:boards/linuxOs/MCAL/Port/src/Port.c:98`（**STM32 代码**，`#include "stm32f10x.h"` `:18`）；调用 `EcuM_Callout_Stubs.c:214` | 无 C 实现 | PORT 基址 `0xFFC1_0000`；PMC/PFC/PFCE/PFCAE/PM/PIPC；CAN 引脚 PIPC=0（HW-E p.91, p.94, p.127–131, p.793） | [03-mcal/03](../03-mcal/03-port-driver.md)、[04-can-mcal/04](../04-can-mcal/04-can-pin-transceiver.md) |
| `Dio_WriteChannel` | 本仓库无 Dio SWS | `oA:boards/linuxOs/MCAL/Dio/src/Dio.c:150`（STM32） | 教学片段 `docs/03-mcal/04-dio-driver.md:176` | Pn / PSRn（原子置位）（HW-E p.96, p.99） | [03-mcal/04](../03-mcal/04-dio-driver.md) |
| `Gpt_Init` | 本仓库无 Gpt SWS | `oA:boards/linuxOs/MCAL/Gpt/src/Gpt.c:211`（STM32 `TIM_TypeDef`）；调用 `EcuM_Callout_Stubs.c:219` | 底层 `ref:mcal/gpt/Ostm.c:16`（`Ostm_InitPclk`） | OSTM0 `0xFFDD_8000` / OSTM1 `0xFFDD_9000`，PCLK = CLK_HSB 80 MHz（HW-E p.1543） | [03-mcal/05](../03-mcal/05-gpt-driver.md) |
| `Gpt_StartTimer` | 本仓库无 Gpt SWS | `oA:.../Gpt/src/Gpt.c:289` | 教学片段 `docs/03-mcal/05-gpt-driver.md:520`；底层 `ref:mcal/gpt/Ostm.c:32`（Start）、`:53`（SetCompare）、`:69`（`Ostm_IntervalCompare`，CMP = N−1） | CMP+00、CTL+20（MD1 interval/free-run），TS 启动（HW-E p.1551–1556） | [03-mcal/05](../03-mcal/05-gpt-driver.md) |
| Gpt 通知 ISR | 本仓库无 Gpt SWS | `oA:.../Gpt/src/Gpt.c:194`（`Gpt_Isr`） | 教学片段 `docs/03-mcal/05-gpt-driver.md:541`（`Gpt_Isr_Ostm0`） | INTOSTM0 = EI74、INTOSTM1 = EI75；OSTM3–7 只走 FEINT（HW-E p.283, p.1544） | [03-mcal/05](../03-mcal/05-gpt-driver.md)、[02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| OS tick 累计（非 AUTOSAR API） | 本仓库无 Os SWS | `oA:system/kernel/src/counter.c:193`（`OsTick`） | `ref:integration/Tick_Accumulator.c:14`（`Tick_Update`） | OSTM 32 位计数回绕 53.687 s @80 MHz（研究笔记 01 F-OSTM-3） | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| CAN 位时间计算（生成器步骤） | `CanControllerBaudrateConfig` `ECUC_Can_00387` p.114 | 不存在 | `ref:mcal/can/Can_BitTiming.c:19`（`Can_ComputeFdTiming`） | `bitrate = fCAN / [divider × (1+TSEG1+TSEG2)]`；fCAN = 40 MHz clkc 或 16 MHz（HW-E p.791, p.1092） | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md) |

---

## 4. 已知缺口与未复核项

1. **研究笔记 03 的若干行号已过时**：`PduR_CanIfRxIndication/TxConfirmation` 实际在 `oA:communication/ComServices/PDURouter/src/PduR_CanIf.c:21/:25`，`PduR_DcmTransmit` 在 `PduR_Dcm.c:21`，SchM 配置文件是 `oA:system/SchM/include/SchM_cfg.h`。本表均为重新 grep 后的值；openAUTOSAR 若更新，请按同样方法复核。
2. **demo 没有 `Can_MainFunction_BusOff`**：教学 demo 的 VirtualCanBus 不模拟错误帧；bus-off 的完整实现与测试只在 capstone 章节（`cap:1083`、`cap:1118`、`cap:1463`）。
3. **本项目没有 Mcu/Port 的 C 实现**；表中给的是教程章节中的伪代码行号，均标为 `[Conceptual]` 或 `[Educational Implementation]`。
4. **CanIf/CanTp/PduR/Dem/Rte/Os SWS 不在仓库中**，表中这些模块的签名为 R4.x 公认形态（本项目 `demo:com/CanTp.h:16-17`、`demo:com/PduR.h:15-17` 已注明），需以真实项目所用 release 的 SWS 确认。
5. 截图中的真实环境（RTA-CAR 12.9.0、Renesas P1M MCAL AR 4.2.2 API）**不在本仓库**，表中没有、也不应有它们的 file:line。到真实项目后，建议在本表右侧追加一列“真实项目 file:line”，按同样方法 grep 填写。

---

## 5. 相关参考

- [autosar-api-map.md](autosar-api-map.md)：每个 API 的调用者、被调者、上下文、同步/异步。
- [autosar-module-map.md](autosar-module-map.md)：模块 → 层 → SWS 可用性 → 源码路径。
- [rh850-autosar-mapping.md](rh850-autosar-mapping.md)：寄存器块 → MCAL → API → 配置容器。
- [can-configuration-map.md](can-configuration-map.md)、[dcm-configuration-map.md](dcm-configuration-map.md)：0x7E0/0x7E8 与 DCM 配置链。
- [02-autosar-classic/05 生成代码](../02-autosar-classic/05-generated-code.md)：handle 如何在模块间串起来。
