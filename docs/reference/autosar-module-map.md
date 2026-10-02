# AUTOSAR 模块地图：层、SWS 可用性、openAUTOSAR 路径、本项目路径、章节

> 对应规范: 仓库根目录的 5 份 AUTOSAR PDF（身份核验见研究笔记 [02 §0](research/02-autosar-sws-notes.md)）：MCU **CP R24-11**、CAN Driver **CP R22-11**、DCM **CP R20-11**、IoHwAb **CP R24-11**、Diagnostics **AP R22-11（Adaptive Platform，不能用于 Classic DCM）**。
> 对应源码: openAUTOSAR = `D:\side_project\openAUTOSAR\`（Arctic Core 2.18.0，版本宏见研究笔记 [03 §1.7](research/03-openautosar-trace.md)）；本项目 = `examples/uds_diag_demo/`、`examples/rh850_mcal_reference/`。所有路径已在本仓库与 openAUTOSAR 工作副本中 `ls`/grep 确认。

---

## 0. 图例

- **层**：MCAL（Microcontroller Abstraction）/ ECUAL（ECU Abstraction）/ Service（Services Layer）/ RTE / ASW（Application SW-C）/ Integration（启动、调度胶水）。层的归属以 AUTOSAR Layered Architecture 为准；本仓库没有该文档，CAN SWS p.14、p.22 脚注 3 给出了 Can 属于 MCAL 的直接证据（研究笔记 02 §2.2）。
- **SWS 本地**：✔ = 仓库中有该模块 SWS（注明 release）；✘ = 没有，该模块的 API/配置描述须标注“需以真实项目 release 的 SWS 确认”。
- **openAUTOSAR**：给目录（实现文件），以及该模块在 Arctic 中声明的 AR 版本。“缺失” = grep 不到实现。
- **本项目**：`demo:` = `examples/uds_diag_demo/`；`ref:` = `examples/rh850_mcal_reference/`；`cap:` = `docs/04-can-mcal/14-can-driver-from-scratch.md`（Markdown 中的完整教学驱动）。

---

## 1. 通信与诊断主链（本教程的核心）

| 模块 | 层 | SWS 本地 | openAUTOSAR 路径（AR 版本） | 本项目路径 | 主要章节 |
|---|---|---|---|---|---|
| **Can**（CAN Driver） | MCAL | ✔ CAN R22-11 | 只有头文件：`include/Can.h`（AR 3.1.5，`:24-26`）、`boards/linuxOs/MCAL/Can/include/Can_Cfg.h`；**`Can.c` 缺失** | `demo:mcal/Can.c`（mock，接口真实）、`demo:mcal/Can_Cfg.*`；`cap:`（RS-CANFD 教学驱动 + mock 寄存器测试）；`ref:mcal/can/Can_BitTiming.c`（位时间编码） | [04-can-mcal/](../04-can-mcal/01-can-hardware-basics.md) 全部 15 章 |
| **CanIf** | ECUAL | ✘ | `communication/CAN/CanIf/src/CanIf.c`、`CanIf_Cfg.c`（AR 3.1.5，`CanIf.h:31-33`） | `demo:ecual/CanIf.c`、`demo:ecual/CanIf_Cfg.*` | [05-can-stack/01](../05-can-stack/01-canif.md)、[02](../05-can-stack/02-canif-configuration.md) |
| **CanTp** | Service（Communication） | ✘ | `communication/CAN/CanTp/src/CanTp.c`、`CanTp_Cfg.c`（AR 3.1.5，`CanTp.h:36-38`） | `demo:com/CanTp.c`、`demo:com/CanTp_Cfg.*` | [05-can-stack/03](../05-can-stack/03-cantp.md)、[04](../05-can-stack/04-isotp.md) |
| **PduR** | Service（Communication） | ✘ | `communication/ComServices/PDURouter/src/*.c`（AR 3.1.5，`PduR.h:29-31`）；`PduR_Cfg.h:77-130` 宏短路 | `demo:com/PduR.c`、`demo:com/PduR_Cfg.*` | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| **Dcm** | Service（Communication Services） | ✔ DCM R20-11 | `diagnostic/Dcm/src/{Dcm.c,Dcm_Dsl.c,Dcm_Dsd.c,Dcm_Dsp.c}`（AR 3.1.5，`Dcm.h:34-36`）；`DCM_Config` 实例缺失 | `demo:diag/{Dcm.c,Dcm_Dsl.c,Dcm_Dsd.c,Dcm_Dsp.c,Dcm_Cfg.*}` | [06-dcm/](../06-dcm/01-dcm-overview.md) 全部 14 章 |
| **Dem** | Service（Diagnostic） | ✘（DCM SWS 只列出 Dcm 调用的 Dem API） | `diagnostic/Dem/src/Dem.c`（AR 3.1.5，`Dem.h:34-36`）；配置 `include/Dem_LCfg.c` 放错目录 | `demo:diag/Dem.c`（stub：Select/Clear/Filter） | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| **NvM** | Service（Memory） | ✘ | `memory/NvM/src/NvM.c`（AR 3.1.5，`include/NvM.h:37-39`）；`NvM_Config` 缺失 | `demo:mem/NvM.c`（stub） | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md) |
| **Rte** | RTE | ✘ | `rte/src/rte.c`（65 行占位）；`diagnostic/Dcm/include/Rte_Dcm.h` 空 | `demo:rte/{Rte_Dcm.c,Rte_Dcm.h,Rte_Dcm_Type.h,Rte_VehicleInfoSWC.h,Rte_SecurityAccessSWC.h,SchM_Dcm.h}`（手写 “as if generated”） | [07-rte-swc/](../07-rte-swc/04-rte-concept.md) |
| **SW-C**（诊断数据提供者） | ASW | ✘（DCM SWS 定义其端口接口，`SWS_Dcm_00686/00685/00690`） | `examples/rte_simple/*.c`（通用示例，未纳入构建） | `demo:swc/VehicleInfoSWC.c`、`demo:swc/SecurityAccessSWC.c` | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md)、[08-integration/04](../08-integration/04-f190-vin-demo.md) |
| **Det** | Service（System） | ✘ | `debug/Det/src/Det.c`（AR 3.1.5） | `demo:general/Det.c` | [06-dcm/13](../06-dcm/13-dcm-debugging.md) |

## 2. 系统服务与调度

| 模块 | 层 | SWS 本地 | openAUTOSAR 路径（AR 版本） | 本项目路径 | 主要章节 |
|---|---|---|---|---|---|
| **EcuM** | Service（System） | ✘ | `system/EcuM/src/EcuM.c`、`EcuM_Callout_Stubs.c`（AR 3.1.5）；`EcuMConfig` 缺失 | `demo:integration/EcuM.c`（启动顺序胶水） | [02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md) |
| **BswM** | Service（System） | ✘ | **缺失**（C 代码中 0 命中） | 无；demo 在 `rte/Rte_Dcm.c:146-155` 内联“BswM 角色” | [02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md)、[06-dcm/10](../06-dcm/10-uds-services.md) |
| **SchM**（BSW Scheduler） | RTE（BSW 部分） | ✘ | `system/SchM/src/SchM.c`、`system/SchM/include/SchM_cfg.h`、`SchM_<Mod>.h` | `demo:integration/BswScheduler.c`（1/5/10 ms）、`demo:rte/SchM_Dcm.h` | [02-autosar-classic/07](../02-autosar-classic/07-mainfunction-scheduling.md) |
| **Os** | Service（System） | ✘ | `system/kernel/src/*.c`（OSEK/AUTOSAR OS；无 arch port，`isr.c` 未编译） | 无 OS；`ref:integration/Tick_Accumulator.c`（tick 累计） | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| **ComM** | Service（Communication） | ✘（DCM SWS 列出 `ComM_DCM_ActiveDiagnostic` 等） | `system/ComM/src/ComM.c` | 无（demo 由 EcuM 直接 STARTED） | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| **CanSM** | Service（Communication） | ✘ | `communication/CAN/CanSM/src/CanSM.c`、`CanSM_Cfg.c` | 无 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| **Com** | Service（Communication） | ✘ | `communication/ComServices/Com/src/*.c`；`ComConfiguration` 缺失 | 无（本教程聚焦诊断 TP 路径） | [05-can-stack/05](../05-can-stack/05-pdur.md) |

## 3. MCAL（非 CAN）与 IoHwAb

| 模块 | 层 | SWS 本地 | openAUTOSAR 路径（AR 版本） | 本项目路径 | 主要章节 |
|---|---|---|---|---|---|
| **Mcu** | MCAL | ✔ MCU R24-11 | `boards/linuxOs/MCAL/Mcu/src/Mcu.c`（**AR 2.2.2**，PowerPC 遗留） | 无 C 实现；教程伪代码 `docs/03-mcal/02-mcu-driver.md` | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| **Port** | MCAL | ✘ | `boards/linuxOs/MCAL/Port/src/Port.c`（AR 3.1.0，**STM32 代码**） | 无 C 实现 | [03-mcal/03](../03-mcal/03-port-driver.md) |
| **Dio** | MCAL | ✘ | `boards/linuxOs/MCAL/Dio/src/Dio.c`（STM32） | 无；教程片段 `docs/03-mcal/04-dio-driver.md` | [03-mcal/04](../03-mcal/04-dio-driver.md) |
| **Gpt** | MCAL | ✘ | `boards/linuxOs/MCAL/Gpt/src/Gpt.c`（STM32 `TIM_TypeDef`） | `ref:mcal/gpt/Ostm.c`（OSTM 底层，非 Gpt API）；教程片段 `docs/03-mcal/05-gpt-driver.md` | [03-mcal/05](../03-mcal/05-gpt-driver.md) |
| **Icu** | MCAL | ✘ | 缺失 | 无 | [03-mcal/06](../03-mcal/06-icu-driver.md) |
| **Wdg / Fls** | MCAL | ✘ | `boards/linuxOs/MCAL/Wdg`、`Fls`（仅头文件或残片） | 无 | — |
| **IoHwAb** | ECUAL | ✔ IoHwAb R24-11 | `iohwabs/`（Fee/MemIf/Ea/WdgIf，**不是** IoHwAb 本身） | 无 | [02-autosar-classic/02](../02-autosar-classic/02-layered-architecture.md) |
| （平台）MMIO 抽象 | — | — | — | `ref:platform/Rh850_Mmio.c` | [03-mcal/01](../03-mcal/01-mcal-overview.md) |

## 4. 版本混杂一览（读代码前先确认版本）

| 来源 | 版本 | 影响 |
|---|---|---|
| 本仓库 SWS | MCU/IoHwAb R24-11，CAN R22-11，DCM R20-11 | 引用 API 时写明 release |
| openAUTOSAR | 主体 R3.1.5；Mcu 2.2.2；Ea/J1939Tp 4.0.2 | TP 接口是 `ProvideRxBuffer/ProvideTxBuffer`，不是 R4.x `StartOfReception/CopyRxData/CopyTxData` |
| 本项目 demo | 按 R4.x 公认形态（CAN 按 R22-11，DCM 按 R20-11） | CanIf/CanTp/PduR 签名在源码头注释中声明“需以项目 release 确认”（`demo:com/CanTp.h:16-17`、`demo:com/PduR.h:15-17`） |
| 截图中的真实环境 | RTA-CAR 12.9.0；Renesas P1M MCAL（AR 4.2.2 API） | 本仓库无法验证；`Can_Write` 等返回类型可能是旧的 `Can_ReturnType` |

---

## 5. 层次判断规则（备忘）

1. **直接访问片上外设寄存器** → MCAL（Mcu、Port、Dio、Gpt、Can）。CAN SWS p.22 脚注 3：片外 CAN 控制器的驱动反而属于 ECU Abstraction。
2. **访问 MCAL、抽象板级/ECU 布线** → ECUAL（CanIf、IoHwAb）。
3. **与网络/硬件无关的服务** → Service（CanTp、PduR、Dcm、Dem、NvM、ComM、EcuM、BswM）。
4. **SW-C 只通过 RTE 通信**，只 include 自己的 `Rte_<Swc>.h`（`demo:rte/Rte_VehicleInfoSWC.h:5-11` 注释）。

相关：[source-traceability.md](source-traceability.md)、[autosar-api-map.md](autosar-api-map.md)、[02-autosar-classic/02 分层架构](../02-autosar-classic/02-layered-architecture.md)。
