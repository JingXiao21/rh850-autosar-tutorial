# DCM 配置地图：ECUC 容器 → demo `Dcm_Cfg.*` → RTE 接口 → 章节

> 对应规范: SWS DCM **CP R20-11**（`AUTOSAR_SWS_DiagnosticCommunicationManager.pdf`）。下表每个 `ECUC_Dcm_xxxxx` 与页码均在 `artifacts/pdf-text/AUTOSAR_SWS_DiagnosticCommunicationManager.txt` 中按参数名定位（PDF 物理页）。`AUTOSAR_SWS_Diagnostics.pdf` 是 Adaptive Platform，**未使用**。
> 对应源码: `examples/uds_diag_demo/diag/Dcm_Cfg.h`、`diag/Dcm_Cfg.c`（手写 “as if generated”），以及使用这些配置的 `diag/Dcm_Dsl.c`、`Dcm_Dsd.c`、`Dcm_Dsp.c`；RTE 侧 `rte/Rte_Dcm.h`、`rte/Rte_Dcm.c`、`rte/SchM_Dcm.h`
> openAUTOSAR 对照: `diagnostic/Dcm/include/Dcm_Lcfg.h`（R3.1.5 配置类型，`DCM_Config` 实例缺失，`:641`）

`[Educational Implementation]` demo 为了可读性把若干容器“拍平”（`diag/Dcm_Cfg.h:9-11`）：DID + DidInfo + DidRead/Write + Data 合成一行；会话/安全授权用位掩码代替引用列表（`diag/Dcm_Cfg.h:39-50`）。读真实生成物时要“展开”回容器引用链。

---

## 1. 容器树（R20-11）与 demo 落点

```text
Dcm
├── DcmConfigSet ─────────────────────────────── demo: const Dcm_ConfigType Dcm_Config   (Dcm_Cfg.c:127-133)
│   ├── DcmDsd
│   │   └── DcmDsdServiceTable
│   │       └── DcmDsdService ─────────────────── demo: Dcm_Services[]                   (Dcm_Cfg.c:114-125)
│   │           └── DcmDsdSubService ──────────── demo: Dcm_Sub10/11/19/27/3E[]           (Dcm_Cfg.c:93-110)
│   ├── DcmDsl
│   │   ├── DcmDslBuffer ──────────────────────── demo: DCM_DSL_BUFFER_SIZE               (Dcm_Cfg.h:24)
│   │   ├── DcmDslDiagResp ────────────────────── demo: DCM_DSL_MAX_NUM_RESP_PEND         (Dcm_Cfg.h:25)
│   │   └── DcmDslProtocol / DcmDslProtocolRow
│   │       └── DcmDslConnection / DcmDslMainConnection
│   │           ├── DcmDslProtocolRx ──────────── demo: DcmConf_DcmDslProtocolRx_*        (Dcm_Cfg.h:31-32)
│   │           └── DcmDslProtocolTx ──────────── demo: DcmConf_DcmDslProtocolTx_DiagResp (Dcm_Cfg.h:33)
│   └── DcmDsp
│       ├── DcmDspSession/DcmDspSessionRow ────── demo: Dcm_SessionRows[]                (Dcm_Cfg.c:19-23)
│       ├── DcmDspSecurity/DcmDspSecurityRow ──── demo: Dcm_SecurityRows[]               (Dcm_Cfg.c:26-30)
│       ├── DcmDspDid / DcmDspDidInfo / DcmDspData demo: Dcm_Dids[]                       (Dcm_Cfg.c:33-49)
│       └── DcmDspRoutine ─────────────────────── demo: Dcm_Routines[]                   (Dcm_Cfg.c:87-90)
└── DcmGeneral ───────────────────────────────── demo: DCM_TASK_TIME_MS 等               (Dcm_Cfg.h:19-21)
```

---

## 2. 参数级映射

### 2.1 DcmGeneral / DcmDsl

| ECUC 参数 | ECUC ID, 页 | demo 值 | demo file:line | 使用位置 | 运行时意义 | 章节 |
|---|---|---|---|---|---|---|
| `DcmDevErrorDetect` | 00823, p.676 | STD_ON | `diag/Dcm_Cfg.h:19` | — | DET 检查 | [06-dcm/13](../06-dcm/13-dcm-debugging.md) |
| `DcmTaskTime` | 00820, p.678 | 10 ms | `diag/Dcm_Cfg.h:20` | P2/S3 递减 `diag/Dcm_Dsl.c:268, :291`；安全延时 `diag/Dcm_Dsp.c:91` | 必须等于调度周期（`integration/BswScheduler.c:50` 的 10 ms 分支） | [06-dcm/12](../06-dcm/12-dcm-mainfunction.md) |
| `DcmRespondAllRequest` | 00600, p.677 | STD_ON | `diag/Dcm_Cfg.h:21` | — | FALSE 时 0x40–0x7F/0xC0–0xFF 不响应（`SWS_Dcm_00084` p.92） | [06-dcm/03](../06-dcm/03-dsd.md) |
| `DcmDslBufferSize` | 00738, p.459 | 128 字节（Rx/Tx 各一） | `diag/Dcm_Cfg.h:24` | 缓冲 `diag/Dcm_Dsl.c:72-73`；超长 → `BUFREQ_E_OVFL` `:334` | 决定最长请求/响应 | [06-dcm/02](../06-dcm/02-dsl.md) |
| `DcmDslDiagRespMaxNumRespPend` | 00693, p.460 | 20 | `diag/Dcm_Cfg.h:25` | `diag/Dcm_Dsl.c:270`；用尽 → 取消 + 运行时错误 `:280-281` | NRC 0x78 次数上限（未配置 = 无限，`SWS_Dcm_01567` p.62） | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| `DcmTimStrP2ServerAdjust` | 00729, p.466 | 10 ms | `diag/Dcm_Cfg.h:26` | `diag/Dcm_Dsl.c:433` | 提前于 P2ServerMax 发 0x78，补偿下层延迟（`SWS_Dcm_00024` p.61） | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| `DcmTimStrP2StarServerAdjust` | 00728, p.467 | 100 ms | `diag/Dcm_Cfg.h:27` | `diag/Dcm_Dsl.c:276` | 同上，对 P2\* | 同上 |
| S3Server（规范固定值） | `SWS_Dcm_00143` p.79 | 5000 ms | `diag/Dcm_Cfg.h:28` | `diag/Dcm_Dsl.c:183, :291-298` | 非默认会话无请求 5 s 回默认会话 | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| `DcmDslProtocolRxPduId`（`DcmDslProtocolRx`） | 00770 p.477（RxPduRef） | 物理 0 / 功能 1 | `diag/Dcm_Cfg.h:31-32` | `diag/Dcm_Dsl.c:317, :425`；PduR 引用 `com/PduR_Cfg.c:18-19` | 与 PduR dest id 必须一致 | [06-dcm/05](../06-dcm/05-dcm-configuration.md) |
| `DcmDslProtocolRxAddrType` | 00710, p.476 | 由 RxPduId 隐含 | `diag/Dcm_Dsl.c:425` | 功能寻址 NRC 抑制（`SWS_Dcm_00001` p.101） | — | [06-dcm/03](../06-dcm/03-dsd.md) |
| `DcmDslProtocolTxPduRef` | 00772, p.478 | 0 | `diag/Dcm_Cfg.h:33` | `diag/Dcm_Dsl.c:448, :479` | 与 PduR Tx 路径 upper id 一致（`com/PduR_Cfg.c:24`） | [06-dcm/05](../06-dcm/05-dcm-configuration.md) |
| `DcmDemClientRef` | 01083, p.467 | 0 | `diag/Dcm_Cfg.h:37` | `diag/Dcm_Dsp.c:187, :192, :233, :238, :243` | 所有带 ClientId 的 Dem API（`SWS_Dcm_01369` p.82） | [06-dcm/09](../06-dcm/09-dtc-dem.md) |

### 2.2 DcmDsd（服务表）

| ECUC 参数 | ECUC ID, 页 | demo 字段 | demo file:line | DSD 使用位置 | 失败 NRC |
|---|---|---|---|---|---|
| `DcmDsdSidTabServiceId` | 00735, p.449 | `sid` | `diag/Dcm_Cfg.h:127`；表 `diag/Dcm_Cfg.c:116-124` | 查表 `diag/Dcm_Dsd.c:67-69` | 0x11（`SWS_Dcm_00197` p.93） |
| `DcmDsdSidTabSubfuncAvail` | 00737, p.449 | `subFuncAvail` | `diag/Dcm_Cfg.h:128` | SPRMIB 处理 | — |
| `DcmDsdSidTabSessionLevelRef` | 00734, p.451 | `sessionMask` | `diag/Dcm_Cfg.h:130` | `diag/Dcm_Dsd.c:91` | 0x7F（`SWS_Dcm_00211` p.97） |
| `DcmDsdSidTabSecurityLevelRef` | 00733, p.450 | `securityMask` | `diag/Dcm_Cfg.h:131` | `diag/Dcm_Dsd.c:96` | 0x33（`SWS_Dcm_00217` p.97） |
| （最小长度，规范为通用检查） | `SWS_Dcm_00696` p.98 | `minReqLen` | `diag/Dcm_Cfg.h:129` | `diag/Dcm_Dsd.c:101` | 0x13 |
| `DcmDsdSubServiceId` | 00803, p.454 | `Dcm_DsdSubServiceType.subFunctionId` | `diag/Dcm_Cfg.h:121`；表 `diag/Dcm_Cfg.c:93-110` | `diag/Dcm_Dsd.c:113-118` | 0x12（`SWS_Dcm_00273` p.98） |
| `DcmDsdSubServiceSessionLevelRef` | 00804, p.456 | `sessionMask` | `diag/Dcm_Cfg.h:122` | `diag/Dcm_Dsd.c:127` | 0x7E（`SWS_Dcm_00616`） |
| `DcmDsdSubServiceSecurityLevelRef` | 00812, p.456 | `securityMask` | `diag/Dcm_Cfg.h:123` | `diag/Dcm_Dsd.c:132` | 0x33 |
| `DcmDsdSidTabFnc`（或内部 DSP） | 00777, p.448 | `fnc` | `diag/Dcm_Cfg.h:134` | 分发 `diag/Dcm_Dsd.c:163` | — |

demo 配置的服务（`diag/Dcm_Cfg.c:116-124`）：

| SID | 会话 | 安全 | 子功能表 | DSP 处理函数（file:line） |
|---|---|---|---|---|
| 0x10 | ALL | ANY | 01, 03（`:93-96`） | `Dcm_DspDiagnosticSessionControl` `diag/Dcm_Dsp.c:110` |
| 0x11 | ALL | ANY | 01, 03（`:97-100`） | `Dcm_DspEcuReset` `diag/Dcm_Dsp.c:147` |
| 0x14 | DEFAULT + EXTENDED | ANY | — | `Dcm_DspClearDiagnosticInformation` `diag/Dcm_Dsp.c:170` |
| 0x19 | DEFAULT + EXTENDED | ANY | 02（`:101-103`） | `Dcm_DspReadDTCInformation` `diag/Dcm_Dsp.c:216` |
| 0x22 | ALL | ANY | — | `Dcm_DspReadDataByIdentifier` `diag/Dcm_Dsp.c:264` |
| 0x27 | EXTENDED | ANY | 01, 02（`:104-107`） | `Dcm_DspSecurityAccess` `diag/Dcm_Dsp.c:383` |
| 0x2E | EXTENDED | ANY | — | `Dcm_DspWriteDataByIdentifier` `diag/Dcm_Dsp.c:487` |
| 0x31 | EXTENDED | ANY | （子功能由 DSP 检查） | `Dcm_DspRoutineControl` `diag/Dcm_Dsp.c:540` |
| 0x3E | ALL | ANY | 00（`:108-110`） | `Dcm_DspTesterPresent` `diag/Dcm_Dsp.c:623` |

### 2.3 DcmDsp：会话、安全

| ECUC 参数 | ECUC ID, 页 | demo 值 | demo file:line | 使用位置 | RTE / 模式接口 | 章节 |
|---|---|---|---|---|---|---|
| `DcmDspSessionLevel` | 00765, p.655 | 0x01 DEFAULT、0x03 EXTENDED | `diag/Dcm_Cfg.c:21-22`；常量 `rte/Rte_Dcm_Type.h:58-60` | `diag/Dcm_Dsl.c:87-94` | `SchM_Switch_Dcm_DcmDiagnosticSessionControl`（`rte/SchM_Dcm.h:18`，调用 `diag/Dcm_Dsl.c:121`）；SW-C 侧 `Rte_Mode_DcmDiagnosticSessionControl_*`（`rte/Rte_VehicleInfoSWC.h:51`） | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| `DcmDspSessionP2ServerMax` | 00766, p.656 | 50 ms | `diag/Dcm_Cfg.c:21-22` | P2 `diag/Dcm_Dsl.c:433`；0x10 响应 `diag/Dcm_Dsp.c:132-133` | — | 同上 |
| `DcmDspSessionP2StarServerMax` | 00768, p.656 | 5000 ms | `diag/Dcm_Cfg.c:21-22` | `diag/Dcm_Dsl.c:276`；0x10 响应（10 ms 单位）`diag/Dcm_Dsp.c:130` | — | 同上 |
| `DcmDspSecurityLevel` | 00754, p.650 | 1（requestSeed 0x01 / sendKey 0x02） | `diag/Dcm_Cfg.c:28` | `diag/Dcm_Dsp.c:372-381` | `SecurityAccess_Level_01`（C/S，`SWS_Dcm_00685` p.338） | [06-dcm/07](../06-dcm/07-security-access.md) |
| `DcmDspSecuritySeedSize` / `KeySize` | 00755 p.651 / 00760 p.650 | 4 / 4 | `diag/Dcm_Cfg.c:28` | `diag/Dcm_Dsp.c:417, :433, :439` | — | 同上 |
| `DcmDspSecurityNumAttDelay` | 00762, p.651 | 3 | `diag/Dcm_Cfg.c:28` | `diag/Dcm_Dsp.c:468` | 达到后 NRC 0x36，延时期间 0x37 | 同上 |
| `DcmDspSecurityDelayTime` | 00757, p.648 | 3000 ms | `diag/Dcm_Cfg.c:28` | 递减 `diag/Dcm_Dsp.c:89-96` | — | 同上 |
| `DcmDspSecurityUsePort` | 00967, p.652 | `USE_ASYNCH_CLIENT_SERVER` | `diag/Dcm_Cfg.h:60` 注释 | GetSeed `diag/Dcm_Dsp.c:424`、CompareKey `:454` | `Rte_Call_SecurityAccess_Level_01_GetSeed/CompareKey`（`rte/Rte_Dcm.h:36-39`，实现 `rte/Rte_Dcm.c:89, :97` → `swc/SecurityAccessSWC.c:34, :56`） | 同上 |

### 2.4 DcmDsp：DID / Data

| ECUC 参数 | ECUC ID, 页 | demo 字段 | demo file:line | 使用位置 |
|---|---|---|---|---|
| `DcmDspDidIdentifier` | 00602, p.509 | `identifier` | `diag/Dcm_Cfg.h:87` | 查表 `diag/Dcm_Dsp.c:57-60` |
| `DcmDspDataByteSize` | 01106, p.530 | `size` | `diag/Dcm_Cfg.h:88` | 响应拼装 |
| `DcmDspDataUsePort` | 00713, p.537 | `usePort`（SYNCH / ASYNCH C/S） | `diag/Dcm_Cfg.h:77-80, :89` | 同步 `diag/Dcm_Dsp.c:345`、异步 `:349` |
| `DcmDspDidReadSessionRef` / `SecurityLevelRef` | 00615 / 00614, p.517 | `readSessionMask` / `readSecurityMask` | `diag/Dcm_Cfg.h:93-94` | `diag/Dcm_Dsp.c:304`（会话不满足 → 0x31，`SWS_Dcm_00434` p.137） |
| `DcmDspDidWriteSessionRef` / `SecurityLevelRef` | 00618 / 00617, p.527 | `writeSessionMask` / `writeSecurityMask` | `diag/Dcm_Cfg.h:95-96` | `diag/Dcm_Dsp.c:502` |
| （`DcmDspDidWrite` 存在与否） | — | `writeAsync == NULL_PTR` 表示无写权限 | `diag/Dcm_Cfg.h:92` | `diag/Dcm_Dsp.c:502` |

demo 配置的 DID（`diag/Dcm_Cfg.c:33-49`）与 RTE 接口：

| DID | 大小 | UsePort | 读/写授权 | RTE 客户端调用（`rte/Rte_Dcm.h`） | 实现 → SW-C runnable | 章节 |
|---|---|---|---|---|---|---|
| 0xF190 VIN | 17 | ASYNCH C/S（可 PENDING） | 读：ALL/ANY；不可写 | `Rte_Call_DataServices_DID_F190_ReadData(OpStatus, Data)`（`:30`，`SWS_Dcm_91006` p.269） | `rte/Rte_Dcm.c:54` → `VehicleInfoSWC_ReadVin`（`swc/VehicleInfoSWC.c:77`） | [08-integration/04](../08-integration/04-f190-vin-demo.md)、[06-dcm/08](../06-dcm/08-did.md) |
| 0xF187 | 8 | SYNCH C/S | 读：ALL/ANY；不可写 | `Rte_Call_DataServices_DID_F187_ReadData(Data)`（`:31`，`SWS_Dcm_00793` p.269） | `rte/Rte_Dcm.c:64` → `VehicleInfoSWC_ReadSwVersion`（`swc/VehicleInfoSWC.c:98`） | [06-dcm/08](../06-dcm/08-did.md) |
| 0xF1A0 | 10 | SYNCH 读 / ASYNCH 写 | 读：ALL/ANY；写：EXTENDED + Level 1 | `…_F1A0_ReadData`（`:32`）/ `…_F1A0_WriteData(Data, OpStatus, ErrorCode)`（`:33`，`SWS_Dcm_91008` p.271） | `rte/Rte_Dcm.c:70, :76` → `VehicleInfoSWC_ReadDiagConfig / WriteDiagConfig`（`swc/VehicleInfoSWC.c:105, :114`）→ NvM | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md) |

`DcmDspMaxDidToRead`（00638, p.483）：demo = 4（`diag/Dcm_Cfg.h:36`），超过 → 0x13（`diag/Dcm_Dsp.c:291`，`SWS_Dcm_01335`）。

### 2.5 DcmDsp：Routine

| ECUC 参数 | ECUC ID, 页 | demo 值 | demo file:line | 使用位置 / RTE |
|---|---|---|---|---|
| `DcmDspRoutineIdentifier` | 00641, p.607 | 0xFF00 | `diag/Dcm_Cfg.c:88` | `diag/Dcm_Dsp.c:540` 起 |
| `DcmDspRoutineUsePort` | 00724, p.608 | TRUE（经 RTE） | `diag/Dcm_Cfg.h:100` 注释 | `Rte_Call_RoutineServices_Routine_FF00_Start/Stop/RequestResults`（`rte/Rte_Dcm.h:41-47`，`SWS_Dcm_00690` p.362）；适配 `diag/Dcm_Cfg.c:55-85` |
| `DcmDspStartRoutineCommonAuthorizationRef` → `DcmDspCommonAuthorization`（Session 01027 / Security 01026） | 01052 p.621；01027/01026 p.508 | EXTENDED / ANY | `diag/Dcm_Cfg.c:89` | 会话不满足 → 0x31（`SWS_Dcm_00570`；`diag/Dcm_Dsp.c:566`） |

---

## 3. 与 openAUTOSAR（R3.1.5）配置类型的对照

| R20-11 容器 | openAUTOSAR 类型（`diagnostic/Dcm/include/Dcm_Lcfg.h`） | 差异 |
|---|---|---|
| `DcmDspSessionRow` | `Dcm_DspSessionRowType`（结束于 `:109`） | 有 P2/P2\*；会话在 0x10 处理中**立即**切换（研究笔记 03 §4.2），R20-11 要求发送确认后切换（`SWS_Dcm_00311` p.114） |
| `DcmDspSecurityRow` | `Dcm_DspSecurityRowType`（`GetSeed`/`CompareKey` 函数指针 `:121-122`，结束于 `:124`） | 回调无 `OpStatus`；`CompareKey(uint8 *key)`（`:50`）；尝试次数/延时字段存在但未使用 |
| `DcmDspDid` + `DcmDspData` | `Dcm_DspDidType`（`DspDidUsePort` `:174`、`DspDidReadDataFnc` `:181`，结束于 `:192`） | `UsePort` 字段从未被源码读取；`ReadData(uint8 *data)`（`:56`）无 `OpStatus` → 无 `Rte_Call_DataServices_*` |
| `DcmDsdService` | `Dcm_DsdServiceType`（结束于 `:368`） | 服务实现另需 `DCM_USE_SERVICE_*` 编译开关（全仓未定义） |
| `DcmConfigSet` | `Dcm_ConfigType`（结束于 `:634`），`extern DCM_Config`（`:641`） | 实例缺失 |
| `DcmGeneral.DcmTaskTime` | `diagnostic/Dcm/include/Dcm_Cfg.h:34` `DCM_TASK_TIME TBD`；`:46` `DCM_MAIN_FUNCTION_PERIOD_TIME_MS 10` | 与 SchM 实际周期（≈25 ms）不一致 |

---

## 4. DCM 升级时要 diff 的配置项（清单）

[Real Project Consideration] 本仓库只有 R20-11；以下是“即使 release 不同也一定要逐项对比”的配置位置，具体变化需在真实项目 SWS / Change Documentation 中确认：

1. `DcmDspDataUsePort` / `DcmDspDidUsePort` 的取值集合与默认值 → 影响 `Rte_Call_DataServices_*` 签名（R20-11 取值见 `ECUC_Dcm_00713` p.537）。
2. `DcmDspSecurityUsePort`、尝试计数器相关参数 → 影响 `SecurityAccess_<Level>` 接口（R4.3.0 起重做，研究笔记 02 §6.1）。
3. `DcmTimStrP2ServerAdjust` / `DcmTimStrP2StarServerAdjust` / `DcmDslDiagRespMaxNumRespPend` → 0x78 行为。
4. 服务表授权（会话、安全、Mode Rule）与 NRC 顺序（`SWS_Dcm_01535` p.94）。
5. `DcmDemClientRef` 与 Dem 接口形态（ClientId + `Dem_SelectDTC`）。
6. 会话 / 复位 / 通信控制的 Mode Declaration Group 与 BswM 规则（`SchM_Switch_*`）。

相关：[06-dcm/05 DCM 配置](../06-dcm/05-dcm-configuration.md)、[06-dcm/14 DCM 升级指南](../06-dcm/14-dcm-upgrade-guide.md)、[07-rte-swc/08 DCM-RTE 集成](../07-rte-swc/08-dcm-rte-integration.md)、[can-configuration-map.md](can-configuration-map.md)、[02-autosar-classic/05 生成代码](../02-autosar-classic/05-generated-code.md)。
