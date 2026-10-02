# RH850 + AUTOSAR Classic 学习指南（Master Index）

> 本文件是整套中文教程的**总目录 / 学习地图 / 进度表**，不承载技术正文。
> 规划依据: [`claude_plan.md`](../claude_plan.md)「rh850-autosar-tutorial-content.md 的新职责」「每完成一个 Markdown 都必须更新导航」「文档必须服务于最终 End-to-End Mental Model」§23；[学习路线图](00-learning-roadmap.md)；[分析与学习计划](analysis-and-learning-plan.md)；[写作约定](reference/research/00-writing-conventions.md)
> 参考器件: **R7F701381 = RH850/P1M-E**（core RH850G3M，lock-step；CAN = RS-CANFD）。
> 本文件重构前的单页教程原文已逐字归档到 [legacy/rh850-autosar-tutorial-content-v1.md](legacy/rh850-autosar-tutorial-content-v1.md)。
> 最后更新: 2026-10-02（状态由文件实际存在与行数核对得出）

[Educational Implementation] 这是一个**教学 / 准备项目**，不是量产 ECU 工程。所有章节都区分 `[AUTOSAR Standard]` / `[RH850 Hardware]` / `[Educational Implementation]` / `[Real Project Consideration]`；限制见文末 [已知限制](#已知限制与需在真实项目确认的事项)。

## 1. 整个系统一张图

```text
RH850
 │
 ├─ CPU          → Part I-02
 ├─ Memory       → Part I-03 / I-05
 ├─ Clock        → Part I-07
 ├─ Port         → Part III-03
 └─ Interrupt    → Part I-06
       │
       ▼
      MCAL         → Part III
       │
       ▼
   CAN Driver      → Part IV
       │
       ▼
     CanIf         → Part V-01/02
       │
       ▼
     CanTp         → Part V-03/04
       │
       ▼
      PduR         → Part V-05
       │
       ▼
      DCM          → Part VI
       │
       ▼
      RTE          → Part VII
       │
       ▼
      SWC          → Part VII-09
```

## 2. End-to-End 链路：所有章节最终汇聚到这里

请求方向（下行）——每一跳标出主讲章节：

```text
CANoe                       → VIII-06 CANoe 测试
  ↓
CAN_H / CAN_L               → IV-01 CAN 硬件基础
  ↓
CAN Transceiver             → IV-04 引脚与收发器
  ↓
RH850 CAN Peripheral        → IV-02 RS-CANFD、IV-03 位定时、IV-11 RX
  ↓
CAN RX Interrupt (EI190)    → I-06 中断、IV-05 CAN 中断、IV-12 ISR 实现
  ↓
CAN MCAL                    → IV-09…IV-14
  ↓
CanIf                       → V-01、V-02
  ↓
CanTp                       → V-03、V-04
  ↓
PduR                        → V-05、V-06 RX 路径
  ↓
DCM (DSL → DSD → DSP)       → VI-02…VI-04、VI-11
  ↓
RTE                         → VII-04、VII-06、VII-08
  ↓
SWC (VehicleInfoSWC)        → VII-09
```

响应方向（上行）：

```text
SWC → RTE → DCM             → VII-08、VI-04、VI-12
 ↓
PduR (PduR_DcmTransmit)     → V-05、V-07 TX 路径
 ↓
CanTp (FF / FC / CF)        → V-03、V-04
 ↓
CanIf → Can_Write()         → V-01、IV-10
 ↓
RH850 CAN Controller (TX buffer) → IV-10、IV-12
 ↓
CAN Bus → CANoe             → VIII-05、VIII-06
```

把两条链重新串起来的是汇聚章 **[VIII-05 UDS 端到端](08-integration/05-uds-end-to-end.md)**；可在 PC 上逐行观察的是 **[VIII-04 F190 VIN Demo](08-integration/04-f190-vin-demo.md)**（`python tools/run_uds_demo.py`）。

## 3. 我现在学到哪里？——使用说明

- **从头开始**：先读 [学习路线图](00-learning-roadmap.md)（依赖图、时间估计、里程碑、每 Part 自测题），然后按 Part I → IX 顺序阅读。
- **定位当前位置**：在下面的 Part 表格中找到你正在读的章节编号（如 `IV-07`）。`Prerequisite` 列是读它之前必须懂的章节，`Next` 列是读完后的下一站——两列都取自每章文件头部的 `> Prerequisite` / `> Next`，与章节本身一致。
- **对照规范 / 源码**：`对应 AUTOSAR specification` 列给出本仓库实际拥有的 SWS release 与页码范围（“本仓库无”表示只能按 R4.x 公认形态讲解）；`对应 source implementation` 列中 `demo` = `examples/uds_diag_demo/`、`ref` = `examples/rh850_mcal_reference/`、`oA` = `D:\side_project\openAUTOSAR`（Arctic Core 2.18.0，R3.1.5 风格，只读参考）。精确 `file:line` 见各章与 [源码追踪表](reference/source-traceability.md)。
- **里程碑 1**：读完 Part IV 并能独立写出 [IV-14](04-can-mcal/14-can-driver-from-scratch.md) 的驱动、通过主机测试 = 理解并能自己实现 RH850 CAN MCAL Driver。
- **复习路径（UDS-down）**：从 [VI-11 `22 F1 90` Runtime Flow](06-dcm/11-dcm-runtime-flow.md) 倒推回硬件，说不清楚的一跳回到对应章节。
- **状态图例**：✅ Complete（文件存在且内容实质完整）；✅ 入口 = 按去重规则刻意保持简短、正文在独立指南中的入口章；🚧 In Progress；⬜ Planned（目标目录中尚未写出）。

## Part I — RH850 Fundamentals

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| I-01 | [RH850 概览：芯片家族到 R7F701381](01-rh850/01-rh850-overview.md) | ✅ | [学习路线图](00-learning-roadmap.md) | [I-02](01-rh850/02-cpu-architecture.md) | HW-E §1/§3.1/§4；DS-E p.1–3；SWS-MCU R24-11 p.9–14 | ref `examples/rh850_mcal_reference/`；oA 无 RH850 支持 |
| I-02 | [G3M CPU：寄存器、PSW、特权与异常级别](01-rh850/02-cpu-architecture.md) | ✅ | [I-01](01-rh850/01-rh850-overview.md) | [I-03](01-rh850/03-memory-map.md) | HW-E §3.2.1 p.189–226、§6.4；G3M Software Manual（本仓库无） | 无（全部 `[Conceptual]`） |
| I-03 | [存储器映射与程序段布局](01-rh850/03-memory-map.md) | ✅ | [I-01](01-rh850/01-rh850-overview.md)、[I-02](01-rh850/02-cpu-architecture.md) | [I-04](01-rh850/04-startup-process.md) | HW-E §4 p.257–260、§35–36；SWS-MCU R24-11 p.26 | oA `system/kernel/src/init.c`（链接自检） |
| I-04 | [启动过程：Reset → SWC Runnable](01-rh850/04-startup-process.md) | ✅ | [I-02](01-rh850/02-cpu-architecture.md)、[I-03](01-rh850/03-memory-map.md) | [I-05](01-rh850/05-linker-script.md)、[II-03](02-autosar-classic/03-ecu-startup.md) | HW-E §8 p.418–434、§4.2.1 p.258；SWS-MCU R24-11 p.13–14 | oA `system/kernel/src/init.c`、`system/EcuM/src/EcuM.c` |
| I-05 | [链接脚本与 map 文件](01-rh850/05-linker-script.md) | ✅ | [I-03](01-rh850/03-memory-map.md)、[I-04](01-rh850/04-startup-process.md) | [I-06](01-rh850/06-interrupt-exception.md) | HW-E §4 p.257–259、p.205–206、p.281–282；SWS-MCU R24-11 p.13 | 无 RH850 链接脚本（`[Conceptual]`）；oA `init.c` 自检 |
| I-06 | [中断与异常：INTC1/INTC2、EIC、向量](01-rh850/06-interrupt-exception.md) | ✅ | [I-02](01-rh850/02-cpu-architecture.md)、[I-05](01-rh850/05-linker-script.md) | [I-07](01-rh850/07-clock-system.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[II-06](02-autosar-classic/06-os-task-isr.md) | HW-E §6 p.264–297、Table 6.11 p.282–290 | oA `system/kernel/src/isr.c`（平台无关） |
| I-07 | [时钟系统：固定时钟树与 CLMA](01-rh850/07-clock-system.md) | ✅ | [I-04](01-rh850/04-startup-process.md)、[I-06](01-rh850/06-interrupt-exception.md) | [I-08](01-rh850/08-peripheral-overview.md)、[IV-03](04-can-mcal/03-can-clock-bit-timing.md)、[III-02](03-mcal/02-mcu-driver.md) | HW-E §12 p.468–483、§17.1.3 p.791、§31.5 | ref `mcal/gpt/Ostm.c`、`mcal/can/Can_BitTiming.c` |
| I-08 | [外设总览：寄存器 → MCAL → AUTOSAR API](01-rh850/08-peripheral-overview.md) | ✅ | [I-01](01-rh850/01-rh850-overview.md)、[I-06](01-rh850/06-interrupt-exception.md)、[I-07](01-rh850/07-clock-system.md) | [II-01](02-autosar-classic/01-classic-platform-overview.md)、[III-01](03-mcal/01-mcal-overview.md)、[IV-02](04-can-mcal/02-rh850-can-peripheral.md) | HW-E §2/§8/§17/§21–24/§30/§35 | ref `platform/Rh850_Mmio.*`、`mcal/*` |

## Part II — AUTOSAR Classic

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| II-01 | [Classic Platform 总览](02-autosar-classic/01-classic-platform-overview.md) | ✅ | [I-08](01-rh850/08-peripheral-overview.md)、[I-06](01-rh850/06-interrupt-exception.md) | [II-02](02-autosar-classic/02-layered-architecture.md) | SWS-MCU R24-11；SWS-CAN R22-11；SWS-DCM R20-11；SWS-IoHwAb R24-11 | oA `include/Std_Types.h`、`system/SchM/`、`debug/Det/` |
| II-02 | [分层架构与四条诊断路径](02-autosar-classic/02-layered-architecture.md) | ✅ | [II-01](02-autosar-classic/01-classic-platform-overview.md) | [II-03](02-autosar-classic/03-ecu-startup.md) | SWS-CAN R22-11 p.14/22–23；SWS-DCM R20-11 p.22–31；IoHwAb R24-11 | oA `communication/CAN/{CanIf,CanTp}`、`diagnostic/Dcm` |
| II-03 | [ECU 启动：复位向量 → Rte_Start](02-autosar-classic/03-ecu-startup.md) | ✅ | [I-04](01-rh850/04-startup-process.md)、[II-02](02-autosar-classic/02-layered-architecture.md) | [II-04](02-autosar-classic/04-configuration-arxml.md) | SWS-MCU R24-11 p.13–14/24–36；SWS-CAN R22-11 p.22/36–43；SWS-DCM R20-11 p.85–88（EcuM SWS 本仓库无） | oA `system/EcuM/src/EcuM.c`、`EcuM_Callout_Stubs.c` |
| II-04 | [配置与 ARXML：ECUC → `*_Init(ConfigPtr)`](02-autosar-classic/04-configuration-arxml.md) | ✅ | [II-03](02-autosar-classic/03-ecu-startup.md) | [II-05](02-autosar-classic/05-generated-code.md)、[II-06](02-autosar-classic/06-os-task-isr.md) | SWS-MCU R24-11 §10；SWS-CAN R22-11 §10 p.98–131 | oA `CanIf_Cfg.c`、`Dcm_Lcfg.h`；demo `*_Cfg.*` |
| II-05 | [生成代码：`*_Cfg.h`/`*_PBcfg.c`/`Rte_*.h`/MemMap](02-autosar-classic/05-generated-code.md) | ✅ | [II-04](02-autosar-classic/04-configuration-arxml.md) | [II-06](02-autosar-classic/06-os-task-isr.md) | SWS-CAN R22-11 p.44/125–130；SWS-MCU R24-11 p.38 | demo `mcal/Can_Cfg.*`…`rte/Rte_VehicleInfoSWC.h` |
| II-06 | [OS：Task、Cat1/Cat2 ISR、Counter/Alarm](02-autosar-classic/06-os-task-isr.md) | ✅ | [I-06](01-rh850/06-interrupt-exception.md)、[II-03](02-autosar-classic/03-ecu-startup.md) | [II-07](02-autosar-classic/07-mainfunction-scheduling.md) | Os SWS 本仓库无（`[Conceptual]`）；SWS-CAN R22-11 p.33 | oA `system/kernel/src/*`；ref `Ostm.c`、`Tick_Accumulator.c` |
| II-07 | [Interrupt / Task / MainFunction / Runnable 与 P2](02-autosar-classic/07-mainfunction-scheduling.md) | ✅ | [II-06](02-autosar-classic/06-os-task-isr.md)、[II-02](02-autosar-classic/02-layered-architecture.md) | [III-01](03-mcal/01-mcal-overview.md) | SWS-CAN R22-11 p.50–51/84–87；SWS-DCM R20-11 `DcmTaskTime` p.678 | oA `system/SchM/`、`diagnostic/Dcm/src/Dcm.c`；demo `integration/BswScheduler.c` |

## Part III — MCAL

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| III-01 | [MCAL 总览：驱动骨架与资源归属](03-mcal/01-mcal-overview.md) | ✅ | [II-01](02-autosar-classic/01-classic-platform-overview.md)、[II-03](02-autosar-classic/03-ecu-startup.md)、[I-08](01-rh850/08-peripheral-overview.md) | [III-02](03-mcal/02-mcu-driver.md) | SWS-MCU R24-11 p.25；SWS-CAN R22-11 p.43（Port/Dio/Gpt/Icu SWS 本仓库无） | oA `boards/linuxOs/MCAL/*`（非 RH850）；ref `platform`、`mcal` |
| III-02 | [MCU：时钟、PLL、复位、RAM、模式](03-mcal/02-mcu-driver.md) | ✅ | [III-01](03-mcal/01-mcal-overview.md)、[II-03](02-autosar-classic/03-ecu-startup.md)、[I-07](01-rh850/07-clock-system.md) | [III-03](03-mcal/03-port-driver.md) | SWS-MCU R24-11（全册 p.9–51）；HW-E §8/§12 | oA `boards/linuxOs/MCAL/Mcu/`、`EcuM_Callout_Stubs.c` |
| III-03 | [Port：引脚复用与 RS-CANFD 引脚](03-mcal/03-port-driver.md) | ✅ | [III-01](03-mcal/01-mcal-overview.md)、[III-02](03-mcal/02-mcu-driver.md) | [III-04](03-mcal/04-dio-driver.md) | Port SWS 本仓库无；SWS-MCU R24-11 p.25；HW-E §2 | oA `boards/linuxOs/MCAL/Port/`（STM32） |
| III-04 | [Dio：GPIO 读写与原子性](03-mcal/04-dio-driver.md) | ✅ | [III-03](03-mcal/03-port-driver.md) | [III-05](03-mcal/05-gpt-driver.md) | Dio SWS 本仓库无；SWS-IoHwAb R24-11 p.15 | oA `boards/linuxOs/MCAL/Dio/`（STM32） |
| III-05 | [Gpt：OSTM / TAUJ 与 `Ostm.c` 逐行](03-mcal/05-gpt-driver.md) | ✅ | [III-01](03-mcal/01-mcal-overview.md)、[II-06](02-autosar-classic/06-os-task-isr.md) | [III-06](03-mcal/06-icu-driver.md) | Gpt SWS 本仓库无；HW-E §22 OSTM | ref `mcal/gpt/Ostm.{h,c}`、`tests/test_reference.c` |
| III-06 | [Icu：边沿、时间戳、TAUD 捕获](03-mcal/06-icu-driver.md) | ✅ | [III-05](03-mcal/05-gpt-driver.md)、[III-03](03-mcal/03-port-driver.md) | [III-07](03-mcal/07-rh850-hardware-mapping.md) | ICU SWS 本仓库无；HW-E §23 TAUD | 无（oA 无 Icu） |
| III-07 | [RH850 硬件 ↔ MCAL 映射总表](03-mcal/07-rh850-hardware-mapping.md) | ✅ | [III-01](03-mcal/01-mcal-overview.md)、[III-02](03-mcal/02-mcu-driver.md)、[III-03](03-mcal/03-port-driver.md)、[III-04](03-mcal/04-dio-driver.md)、[III-05](03-mcal/05-gpt-driver.md)、[III-06](03-mcal/06-icu-driver.md) | [IV-02](04-can-mcal/02-rh850-can-peripheral.md) | SWS-MCU R24-11；SWS-CAN R22-11 | ref；oA `boards/linuxOs/MCAL/` |

## Part IV — CAN MCAL Driver（里程碑 1）

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| IV-01 | [CAN 硬件基础：bit → 帧 → bus-off](04-can-mcal/01-can-hardware-basics.md) | ✅ | [I-08](01-rh850/08-peripheral-overview.md)、[III-01](03-mcal/01-mcal-overview.md) | [IV-02](04-can-mcal/02-rh850-can-peripheral.md) | SWS-CAN R22-11 p.43/58–61；HW-E §17 | 无（oA 无 Can driver） |
| IV-02 | [RS-CANFD 全景](04-can-mcal/02-rh850-can-peripheral.md) | ✅ | [IV-01](04-can-mcal/01-can-hardware-basics.md)、[I-03](01-rh850/03-memory-map.md)、[I-08](01-rh850/08-peripheral-overview.md) | [IV-03](04-can-mcal/03-can-clock-bit-timing.md) | HW-E §17 p.788–1123 | `docs/hardware-registers.json`、`tools/build_hardware_index.py` |
| IV-03 | [CAN 时钟与位定时（CmCFG/NCFG/DCFG）](04-can-mcal/03-can-clock-bit-timing.md) | ✅ | [IV-01](04-can-mcal/01-can-hardware-basics.md)、[IV-02](04-can-mcal/02-rh850-can-peripheral.md)、[I-07](01-rh850/07-clock-system.md)、[III-02](03-mcal/02-mcu-driver.md) | [IV-06](04-can-mcal/06-can-controller-init.md)、[IV-04](04-can-mcal/04-can-pin-transceiver.md) | HW-E p.791/803–804/921–936；SWS-CAN R22-11 p.114–117 | ref `mcal/can/Can_BitTiming.{h,c}` |
| IV-04 | [CAN 引脚与收发器](04-can-mcal/04-can-pin-transceiver.md) | ✅ | [IV-01](04-can-mcal/01-can-hardware-basics.md)、[IV-02](04-can-mcal/02-rh850-can-peripheral.md)、[III-03](03-mcal/03-port-driver.md)、[III-04](03-mcal/04-dio-driver.md) | [IV-05](04-can-mcal/05-can-interrupt.md) | HW-E p.70–74/91–131/793；DS-E p.23 | 无（`rh850-hardware-handoff.md` §3 为 legacy 线索） |
| IV-05 | [CAN 中断：EI183–193 与中断/轮询](04-can-mcal/05-can-interrupt.md) | ✅ | [IV-02](04-can-mcal/02-rh850-can-peripheral.md)、[I-06](01-rh850/06-interrupt-exception.md)、[II-06](02-autosar-classic/06-os-task-isr.md)、[II-07](02-autosar-classic/07-mainfunction-scheduling.md) | [IV-06](04-can-mcal/06-can-controller-init.md) | HW-E p.264–286/792/1057–1058；SWS-CAN R22-11 p.33/50–51 | oA `CanIf.c` 回调 |
| IV-06 | [控制器初始化与模式切换](04-can-mcal/06-can-controller-init.md) | ✅ | [IV-02](04-can-mcal/02-rh850-can-peripheral.md)、[IV-03](04-can-mcal/03-can-clock-bit-timing.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[II-03](02-autosar-classic/03-ecu-startup.md) | [IV-07](04-can-mcal/07-hoh-hrh-hth.md) | SWS-CAN R22-11 p.34–43/62–67 | oA `EcuM_Callout_Stubs.c`、`CanIf.c` |
| IV-07 | [HOH / HRH / HTH ↔ 邮箱](04-can-mcal/07-hoh-hrh-hth.md) | ✅ | [IV-01](04-can-mcal/01-can-hardware-basics.md)、[IV-02](04-can-mcal/02-rh850-can-peripheral.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[IV-06](04-can-mcal/06-can-controller-init.md) | [IV-08](04-can-mcal/08-can-configuration.md) | SWS-CAN R22-11 p.15–17/45–47/122–129 | oA `Can_Cfg.h`、`CanIf_Cfg.c` |
| IV-08 | [Can 配置：ECUC → 寄存器值](04-can-mcal/08-can-configuration.md) | ✅ | [IV-03](04-can-mcal/03-can-clock-bit-timing.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[IV-06](04-can-mcal/06-can-controller-init.md)、[IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[II-04](02-autosar-classic/04-configuration-arxml.md)、[II-05](02-autosar-classic/05-generated-code.md) | [IV-09](04-can-mcal/09-can-init-implementation.md) | SWS-CAN R22-11 §10 p.92–131 | oA `Can_Cfg.h`（无实例）；demo `mcal/Can_Cfg.*` |
| IV-09 | [`Can_Init` 实现](04-can-mcal/09-can-init-implementation.md) | ✅ | [IV-06](04-can-mcal/06-can-controller-init.md)、[IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[IV-08](04-can-mcal/08-can-configuration.md) | [IV-10](04-can-mcal/10-can-write-implementation.md) | SWS-CAN R22-11 p.35–44/62–63；HW-E §17 | ref `platform/Rh850_Mmio.h`；教学代码见 IV-14 |
| IV-10 | [`Can_Write` 实现与 TxConfirmation](04-can-mcal/10-can-write-implementation.md) | ✅ | [IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[IV-09](04-can-mcal/09-can-init-implementation.md) | [IV-11](04-can-mcal/11-can-rx-implementation.md) | SWS-CAN R22-11 p.44–47/80–83；HW-E p.878–889 | oA `CanIf.c`（`Can_ReturnType` 旧形态）；IV-14 |
| IV-11 | [RX 实现：接收规则、RX FIFO](04-can-mcal/11-can-rx-implementation.md) | ✅ | [IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[IV-09](04-can-mcal/09-can-init-implementation.md)、[IV-10](04-can-mcal/10-can-write-implementation.md) | [IV-12](04-can-mcal/12-can-interrupt-implementation.md) | SWS-CAN R22-11 p.48–50/85–86；HW-E p.830–852 | oA `CanIf_RxIndication`；IV-14 |
| IV-12 | [ISR 包装、清标志、MainFunction](04-can-mcal/12-can-interrupt-implementation.md) | ✅ | [IV-05](04-can-mcal/05-can-interrupt.md)、[IV-10](04-can-mcal/10-can-write-implementation.md)、[IV-11](04-can-mcal/11-can-rx-implementation.md)、[II-06](02-autosar-classic/06-os-task-isr.md) | [IV-13](04-can-mcal/13-can-error-busoff.md) | SWS-CAN R22-11 p.33/50–51/84–87 | oA `include/Can.h`、SchM 宏；IV-14 `Can_Irq.c` |
| IV-13 | [错误处理与 Bus-off（CanSM 恢复）](04-can-mcal/13-can-error-busoff.md) | ✅ | [IV-01](04-can-mcal/01-can-hardware-basics.md)、[IV-09](04-can-mcal/09-can-init-implementation.md)、[IV-12](04-can-mcal/12-can-interrupt-implementation.md) | [IV-14](04-can-mcal/14-can-driver-from-scratch.md) | SWS-CAN R22-11 p.36/42–43（`SWS_Can_00274`）/71–74/86 | oA `CanIf.c`、`CanSM/src/CanSM.c` |
| IV-14 | [从零写 RS-CANFD Can Driver（主机可测）](04-can-mcal/14-can-driver-from-scratch.md) | ✅ | [IV-09](04-can-mcal/09-can-init-implementation.md)、[IV-10](04-can-mcal/10-can-write-implementation.md)、[IV-11](04-can-mcal/11-can-rx-implementation.md)、[IV-12](04-can-mcal/12-can-interrupt-implementation.md)、[IV-13](04-can-mcal/13-can-error-busoff.md) | [IV-15](04-can-mcal/15-can-driver-debugging.md) | SWS-CAN R22-11 p.52–89；HW-E §17 | ref `Rh850_Mmio`；`tools/run_host_tests.py` |
| IV-15 | [Can Driver 分层调试](04-can-mcal/15-can-driver-debugging.md) | ✅ | [IV-04](04-can-mcal/04-can-pin-transceiver.md)、[IV-09](04-can-mcal/09-can-init-implementation.md)、[IV-14](04-can-mcal/14-can-driver-from-scratch.md) | [V-01](05-can-stack/01-canif.md)、[VIII-07](08-integration/07-integration-debugging.md) | SWS-CAN R22-11 p.52–53/88；HW-E §17 | IV-14 教学驱动 |

## Part V — CAN Communication Stack

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| V-01 | [CanIf：HRH/HTH → L-PDU](05-can-stack/01-canif.md) | ✅ | [IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[IV-10](04-can-mcal/10-can-write-implementation.md)、[IV-11](04-can-mcal/11-can-rx-implementation.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[IV-13](04-can-mcal/13-can-error-busoff.md) | [V-02](05-can-stack/02-canif-configuration.md) | SWS-CAN R22-11 p.14/22–23/45（CanIf SWS 本仓库无） | demo `ecual/CanIf.*`；oA `CanIf/src/CanIf.c` |
| V-02 | [CanIf 配置与 0x7E0/0x7DF/0x7E8](05-can-stack/02-canif-configuration.md) | ✅ | [V-01](05-can-stack/01-canif.md)、[IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[IV-08](04-can-mcal/08-can-configuration.md)、[II-04](02-autosar-classic/04-configuration-arxml.md) | [V-03](05-can-stack/03-cantp.md) | SWS-CAN R22-11 p.122–130（CanIf SWS 本仓库无） | demo `mcal/Can_Cfg.*`、`ecual/CanIf_Cfg.*` |
| V-03 | [CanTp：分段/重组、N_xx 计时](05-can-stack/03-cantp.md) | ✅ | [V-01](05-can-stack/01-canif.md)、[V-02](05-can-stack/02-canif-configuration.md)、[II-07](02-autosar-classic/07-mainfunction-scheduling.md)、[II-06](02-autosar-classic/06-os-task-isr.md) | [V-04](05-can-stack/04-isotp.md)、[V-05](05-can-stack/05-pdur.md) | SWS-DCM R20-11 p.243–247（CanTp SWS 本仓库无） | demo `com/CanTp.*`；oA `CanTp/src/CanTp.c` |
| V-04 | [ISO 15765-2：SF/FF/CF/FC](05-can-stack/04-isotp.md) | ✅ | [V-03](05-can-stack/03-cantp.md)、[IV-01](04-can-mcal/01-can-hardware-basics.md) | [V-05](05-can-stack/05-pdur.md) | ISO 15765-2（本仓库无原文） | demo `com/CanTp.c`、`sim/UdsTester.c` |
| V-05 | [PduR：TP/IF 路由与 `PduR_DcmTransmit`](05-can-stack/05-pdur.md) | ✅ | [V-03](05-can-stack/03-cantp.md)、[V-04](05-can-stack/04-isotp.md)、[V-01](05-can-stack/01-canif.md) | [V-06](05-can-stack/06-can-rx-path.md) | SWS-DCM R20-11 p.23/243–247（PduR SWS 本仓库无） | demo `com/PduR.*`；oA `PDURouter/` |
| V-06 | [完整 RX 路径：RX FIFO → Dcm](05-can-stack/06-can-rx-path.md) | ✅ | [V-01](05-can-stack/01-canif.md)、[V-03](05-can-stack/03-cantp.md)、[V-04](05-can-stack/04-isotp.md)、[V-05](05-can-stack/05-pdur.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[IV-11](04-can-mcal/11-can-rx-implementation.md)、[IV-12](04-can-mcal/12-can-interrupt-implementation.md) | [V-07](05-can-stack/07-can-tx-path.md) | SWS-CAN R22-11 p.33/48；SWS-DCM R20-11 p.243–247 | demo `mcal/Can.c`…`diag/Dcm_Dsl.c`；`artifacts/uds-demo/trace.txt` |
| V-07 | [完整 TX 路径：Dcm → `Can_Write`](05-can-stack/07-can-tx-path.md) | ✅ | [V-06](05-can-stack/06-can-rx-path.md)、[V-03](05-can-stack/03-cantp.md)、[V-05](05-can-stack/05-pdur.md)、[V-01](05-can-stack/01-canif.md)、[IV-10](04-can-mcal/10-can-write-implementation.md)、[IV-12](04-can-mcal/12-can-interrupt-implementation.md) | [VI-01](06-dcm/01-dcm-overview.md)、[VIII-02](08-integration/02-can-stack-integration.md) | SWS-CAN R22-11 p.45–47；SWS-DCM R20-11 p.59 | demo `diag/Dcm_Dsl.c`…`mcal/Can.c` |

## Part VI — DCM

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| VI-01 | [DCM 总览](06-dcm/01-dcm-overview.md) | ✅ | [II-07](02-autosar-classic/07-mainfunction-scheduling.md)、[V-03](05-can-stack/03-cantp.md)、[V-05](05-can-stack/05-pdur.md)、[V-06](05-can-stack/06-can-rx-path.md) | [VI-02](06-dcm/02-dsl.md) | SWS-DCM R20-11 p.22–31/49–50 | oA `diagnostic/Dcm/src/*`；demo `diag/` |
| VI-02 | [DSL：TP 接口、缓冲、P2/P2*/S3、NRC 0x78](06-dcm/02-dsl.md) | ✅ | [VI-01](06-dcm/01-dcm-overview.md)、[V-03](05-can-stack/03-cantp.md)、[V-04](05-can-stack/04-isotp.md)、[V-05](05-can-stack/05-pdur.md) | [VI-03](06-dcm/03-dsd.md) | SWS-DCM R20-11 §7.4 p.53–88、§8.4 | demo `diag/Dcm_Dsl.c`；oA `Dcm_Dsl.c` |
| VI-03 | [DSD：服务表、校验链、SPRMIB](06-dcm/03-dsd.md) | ✅ | [VI-01](06-dcm/01-dcm-overview.md)、[VI-02](06-dcm/02-dsl.md) | [VI-04](06-dcm/04-dsp.md) | SWS-DCM R20-11 §7.5 p.88–102 | demo `diag/Dcm_Dsd.c`；oA `Dcm_Dsd.c` |
| VI-04 | [DSP：handler、OpStatus 异步模型](06-dcm/04-dsp.md) | ✅ | [VI-02](06-dcm/02-dsl.md)、[VI-03](06-dcm/03-dsd.md)、[VII-06](07-rte-swc/06-client-server.md) | [VI-05](06-dcm/05-dcm-configuration.md) | SWS-DCM R20-11 §7.6 p.102–111、§8.7–8.8 | demo `diag/Dcm_Dsp.c`；oA `Dcm_Dsp.c` |
| VI-05 | [DCM 配置：ECUC 树 → 生成表](06-dcm/05-dcm-configuration.md) | ✅ | [VI-02](06-dcm/02-dsl.md)、[VI-03](06-dcm/03-dsd.md)、[VI-04](06-dcm/04-dsp.md)、[II-04](02-autosar-classic/04-configuration-arxml.md) | [VI-06](06-dcm/06-diagnostic-session.md) | SWS-DCM R20-11 §10 p.441–678 | demo `diag/Dcm_Cfg.*`；oA `Dcm_Lcfg.h` |
| VI-06 | [0x10 会话控制与 S3](06-dcm/06-diagnostic-session.md) | ✅ | [VI-02](06-dcm/02-dsl.md)、[VI-03](06-dcm/03-dsd.md)、[VI-05](06-dcm/05-dcm-configuration.md) | [VI-07](06-dcm/07-security-access.md) | SWS-DCM R20-11 p.74–84/114 | demo `diag/Dcm_Dsp.c`、`Dcm_Dsl.c` |
| VI-07 | [0x27 SecurityAccess](06-dcm/07-security-access.md) | ✅ | [VI-03](06-dcm/03-dsd.md)、[VI-04](06-dcm/04-dsp.md)、[VI-06](06-dcm/06-diagnostic-session.md) | [VI-08](06-dcm/08-did.md) | SWS-DCM R20-11 p.73–75/142–144 | demo `diag/Dcm_Dsp.c`、`swc/SecurityAccessSWC.c` |
| VI-08 | [DID：0x22 / 0x2E](06-dcm/08-did.md) | ✅ | [VI-03](06-dcm/03-dsd.md)、[VI-04](06-dcm/04-dsp.md)、[VI-05](06-dcm/05-dcm-configuration.md)、[VI-07](06-dcm/07-security-access.md) | [VI-09](06-dcm/09-dtc-dem.md) | SWS-DCM R20-11 p.135–141/174–177/269–276 | demo `diag/Dcm_Dsp.c`、`rte/Rte_Dcm.c`、`swc/VehicleInfoSWC.c` |
| VI-09 | [DTC 与 Dcm↔Dem：0x14 / 0x19](06-dcm/09-dtc-dem.md) | ✅ | [VI-04](06-dcm/04-dsp.md)、[VI-08](06-dcm/08-did.md) | [VI-10](06-dcm/10-uds-services.md) | SWS-DCM R20-11 p.112–135（Dem SWS 本仓库无） | demo `diag/Dem.*`；oA `diagnostic/Dem/` |
| VI-10 | [UDS 服务目录](06-dcm/10-uds-services.md) | ✅ | [VI-03](06-dcm/03-dsd.md)、[VI-06](06-dcm/06-diagnostic-session.md)、[VI-07](06-dcm/07-security-access.md)、[VI-08](06-dcm/08-did.md)、[VI-09](06-dcm/09-dtc-dem.md) | [VI-11](06-dcm/11-dcm-runtime-flow.md) | SWS-DCM R20-11 p.92–196 | demo `diag/Dcm_Dsd.c`、`Dcm_Dsp.c`、`tests/test_uds_demo.c` |
| VI-11 | [`22 F1 90` 经过哪些函数](06-dcm/11-dcm-runtime-flow.md) | ✅ | [VI-02](06-dcm/02-dsl.md)、[VI-03](06-dcm/03-dsd.md)、[VI-04](06-dcm/04-dsp.md)、[VI-08](06-dcm/08-did.md)、[V-03](05-can-stack/03-cantp.md)、[V-05](05-can-stack/05-pdur.md)、[IV-12](04-can-mcal/12-can-interrupt-implementation.md) | [VI-12](06-dcm/12-dcm-mainfunction.md) | SWS-DCM R20-11 p.59–79/243–247 | demo 全链路；`artifacts/uds-demo/trace.txt` |
| VI-12 | [`Dcm_MainFunction` 内部](06-dcm/12-dcm-mainfunction.md) | ✅ | [II-06](02-autosar-classic/06-os-task-isr.md)、[II-07](02-autosar-classic/07-mainfunction-scheduling.md)、[VI-02](06-dcm/02-dsl.md)、[VI-11](06-dcm/11-dcm-runtime-flow.md) | [VI-13](06-dcm/13-dcm-debugging.md) | SWS-DCM R20-11 p.79–83/260–261/678 | demo `diag/Dcm.c`、`integration/BswScheduler.c` |
| VI-13 | [DCM Debugging](06-dcm/13-dcm-debugging.md) | ✅ | [VI-11](06-dcm/11-dcm-runtime-flow.md)、[VI-12](06-dcm/12-dcm-mainfunction.md)、[VI-10](06-dcm/10-uds-services.md) | [VI-14](06-dcm/14-dcm-upgrade-guide.md)、[DCM 升级指南](dcm-upgrade-guide.md) | SWS-DCM R20-11 p.47–48/56–60/243–247 | demo `diag/*`、`general/Det.c`、`general/UdsTrace.c` |
| VI-14 | [DCM 升级（章节入口 → 独立指南）](06-dcm/14-dcm-upgrade-guide.md) | ✅ 入口 | [VI-05](06-dcm/05-dcm-configuration.md)、[VI-11](06-dcm/11-dcm-runtime-flow.md)、[VI-13](06-dcm/13-dcm-debugging.md) | [VII-01](07-rte-swc/01-swc-concept.md) | SWS-DCM R20-11（只有此版本） | 见 [DCM 升级指南](dcm-upgrade-guide.md) |

## Part VII — RTE / SWC

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| VII-01 | [SWC 概念](07-rte-swc/01-swc-concept.md) | ✅ | [II-02](02-autosar-classic/02-layered-architecture.md)、[II-04](02-autosar-classic/04-configuration-arxml.md)、[II-07](02-autosar-classic/07-mainfunction-scheduling.md) | [VII-02](07-rte-swc/02-port-interface.md) | RTE SWS / SWC Template 本仓库无；SWS-DCM R20-11 §8.8 | oA `examples/rte_simple/`；demo `swc/` |
| VII-02 | [Port 与 Interface：S/R、C/S、Mode](07-rte-swc/02-port-interface.md) | ✅ | [VII-01](07-rte-swc/01-swc-concept.md) | [VII-03](07-rte-swc/03-runnable-event.md) | RTE SWS 本仓库无；SWS-DCM R20-11 §8.8 | oA `rte_simple_lib.arxml`；demo `rte/Rte_Dcm.h` |
| VII-03 | [Runnable 与 RTE Event](07-rte-swc/03-runnable-event.md) | ✅ | [VII-02](07-rte-swc/02-port-interface.md)、[II-06](02-autosar-classic/06-os-task-isr.md)、[II-07](02-autosar-classic/07-mainfunction-scheduling.md) | [VII-04](07-rte-swc/04-rte-concept.md) | RTE/Os SWS 本仓库无（`[Conceptual]`） | oA `rte/src/rte.c`；demo `rte/Rte_Dcm.c`、`BswScheduler.c` |
| VII-04 | [RTE 概念](07-rte-swc/04-rte-concept.md) | ✅ | [VII-01](07-rte-swc/01-swc-concept.md)、[VII-02](07-rte-swc/02-port-interface.md)、[VII-03](07-rte-swc/03-runnable-event.md) | [VII-05](07-rte-swc/05-rte-generation.md) | RTE SWS 本仓库无 | oA `rte/src/rte.c`；demo `rte/*` |
| VII-05 | [RTE 生成：ARXML → `Rte_VehicleInfoSWC.h`](07-rte-swc/05-rte-generation.md) | ✅ | [VII-04](07-rte-swc/04-rte-concept.md)、[II-04](02-autosar-classic/04-configuration-arxml.md)、[VII-02](07-rte-swc/02-port-interface.md) | [VII-06](07-rte-swc/06-client-server.md) | RTE SWS / XSD 本仓库无；SWS-DCM R20-11 端口命名 | oA `rte_simple_*.arxml`；demo `rte/Rte_VehicleInfoSWC.h` |
| VII-06 | [Client/Server：同步/异步与 OpStatus](07-rte-swc/06-client-server.md) | ✅ | [VII-02](07-rte-swc/02-port-interface.md)、[VII-03](07-rte-swc/03-runnable-event.md)、[VII-05](07-rte-swc/05-rte-generation.md) | [VII-07](07-rte-swc/07-sender-receiver.md) | RTE SWS 本仓库无；SWS-DCM R20-11 OpStatus | demo `rte/Rte_Dcm.c`、`swc/VehicleInfoSWC.c` |
| VII-07 | [Sender/Receiver：Read/Write、IRead/IWrite](07-rte-swc/07-sender-receiver.md) | ✅ | [VII-02](07-rte-swc/02-port-interface.md)、[VII-06](07-rte-swc/06-client-server.md) | [VII-08](07-rte-swc/08-dcm-rte-integration.md) | RTE / Com SWS 本仓库无 | oA `examples/rte_simple/`（demo 无 S/R） |
| VII-08 | [DCM 如何最终调用应用（DCM ↔ RTE）](07-rte-swc/08-dcm-rte-integration.md) | ✅ | [VII-06](07-rte-swc/06-client-server.md)、[VII-07](07-rte-swc/07-sender-receiver.md)、[VII-05](07-rte-swc/05-rte-generation.md)、[VI-04](06-dcm/04-dsp.md)、[VI-08](06-dcm/08-did.md) | [VII-09](07-rte-swc/09-diagnostic-swc-example.md) | SWS-DCM R20-11 §8.8 p.335–415 | demo `diag/Dcm_Dsp.c`、`rte/Rte_Dcm.c`；oA `Dcm_Dsp.c` |
| VII-09 | [诊断 SWC 实例：VehicleInfoSWC](07-rte-swc/09-diagnostic-swc-example.md) | ✅ | [VII-08](07-rte-swc/08-dcm-rte-integration.md)、[VII-06](07-rte-swc/06-client-server.md)、[VII-05](07-rte-swc/05-rte-generation.md) | [VIII-04](08-integration/04-f190-vin-demo.md)、[SWC+RTE 教程](autosar-swc-rte-tutorial.md) | SWS-DCM R20-11 `SWS_Dcm_00685/00686/00690` | demo `swc/VehicleInfoSWC.c`、`swc/SecurityAccessSWC.c` |

## Part VIII — Integration

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| VIII-01 | [ECU 配置一致性检查清单](08-integration/01-ecu-configuration-checklist.md) | ✅ | [II-04](02-autosar-classic/04-configuration-arxml.md)、[II-05](02-autosar-classic/05-generated-code.md)、[IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[IV-08](04-can-mcal/08-can-configuration.md)、[V-02](05-can-stack/02-canif-configuration.md)、[V-05](05-can-stack/05-pdur.md)、[VI-05](06-dcm/05-dcm-configuration.md)、[VII-08](07-rte-swc/08-dcm-rte-integration.md) | [VIII-02](08-integration/02-can-stack-integration.md) | SWS-CAN R22-11 p.22/109–129；SWS-DCM R20-11 | demo 全部 `*_Cfg.[ch]` |
| VIII-02 | [CAN 栈集成](08-integration/02-can-stack-integration.md) | ✅ | [VIII-01](08-integration/01-ecu-configuration-checklist.md)、[IV-14](04-can-mcal/14-can-driver-from-scratch.md)、[V-01](05-can-stack/01-canif.md)、[V-03](05-can-stack/03-cantp.md)、[V-04](05-can-stack/04-isotp.md)、[V-05](05-can-stack/05-pdur.md)、[II-03](02-autosar-classic/03-ecu-startup.md) | [VIII-03](08-integration/03-dcm-integration.md) | SWS-CAN R22-11 p.36–43/66–67/80–88；SWS-DCM R20-11 p.243–247 | demo `mcal/Can.c`…`integration/EcuM.c` |
| VIII-03 | [DCM 集成：收到却不回的根因](08-integration/03-dcm-integration.md) | ✅ | [VIII-02](08-integration/02-can-stack-integration.md)、[VI-01](06-dcm/01-dcm-overview.md)、[VI-02](06-dcm/02-dsl.md)、[VI-03](06-dcm/03-dsd.md)、[VI-04](06-dcm/04-dsp.md)、[VI-05](06-dcm/05-dcm-configuration.md)、[VI-12](06-dcm/12-dcm-mainfunction.md)、[VII-08](07-rte-swc/08-dcm-rte-integration.md) | [VIII-04](08-integration/04-f190-vin-demo.md) | SWS-DCM R20-11 p.30–31/236/260–261 | demo `diag/`、`rte/`、`swc/`、`integration/` |
| VIII-04 | [F190 VIN Demo：PC 上跑通 `22 F1 90`](08-integration/04-f190-vin-demo.md) | ✅ | [VIII-03](08-integration/03-dcm-integration.md)、[demo README](../examples/uds_diag_demo/README.md)、[VI-08](06-dcm/08-did.md)、[SWC+RTE 教程](autosar-swc-rte-tutorial.md) | [VIII-05](08-integration/05-uds-end-to-end.md) | SWS-DCM R20-11 p.135–141/269–270；SWS-CAN R22-11 | demo 全部；`tools/run_uds_demo.py` |
| VIII-05 | [UDS 端到端：CANoe → SWC → CANoe（汇聚章）](08-integration/05-uds-end-to-end.md) | ✅ | [I-04](01-rh850/04-startup-process.md)、[I-06](01-rh850/06-interrupt-exception.md)、[II-03](02-autosar-classic/03-ecu-startup.md)、[II-07](02-autosar-classic/07-mainfunction-scheduling.md)、[IV-05](04-can-mcal/05-can-interrupt.md)、[IV-11](04-can-mcal/11-can-rx-implementation.md)、[V-06](05-can-stack/06-can-rx-path.md)、[V-07](05-can-stack/07-can-tx-path.md)、[VI-11](06-dcm/11-dcm-runtime-flow.md)、[VII-08](07-rte-swc/08-dcm-rte-integration.md)、[VIII-04](08-integration/04-f190-vin-demo.md) | [VIII-06](08-integration/06-canoe-test.md) | SWS-CAN R22-11；SWS-DCM R20-11 | demo 全部层；`artifacts/uds-demo/trace.txt` |
| VIII-06 | [CANoe / python-udsoncan 测试](08-integration/06-canoe-test.md) | ✅ | [VIII-05](08-integration/05-uds-end-to-end.md)、[VIII-04](08-integration/04-f190-vin-demo.md)、[V-04](05-can-stack/04-isotp.md)、[VI-10](06-dcm/10-uds-services.md)、[VI-06](06-dcm/06-diagnostic-session.md)、[VI-07](06-dcm/07-security-access.md) | [VIII-07](08-integration/07-integration-debugging.md) | SWS-DCM R20-11 p.58/79/94/101 | demo `tests/test_uds_demo.c`、`sim/UdsTester.c` |
| VIII-07 | [集成调试（入口 → 调试手册）](08-integration/07-integration-debugging.md) | ✅ 入口 | [VIII-01](08-integration/01-ecu-configuration-checklist.md)、[VIII-02](08-integration/02-can-stack-integration.md)、[VIII-03](08-integration/03-dcm-integration.md)、[VIII-06](08-integration/06-canoe-test.md) | [IX-01](09-real-project-preparation/01-how-to-read-real-autosar-project.md) | 同 [调试手册](debugging-autosar-diagnostics.md) | demo；见调试手册 |

## Part IX — Real Project Preparation

| # | Chapter | Status | Prerequisite | Next | 对应 AUTOSAR specification (release) | 对应 source implementation |
|---|---|---|---|---|---|---|
| IX-01 | [如何阅读真实 AUTOSAR 工程](09-real-project-preparation/01-how-to-read-real-autosar-project.md) | ✅ | [VIII-05](08-integration/05-uds-end-to-end.md)、[II-02](02-autosar-classic/02-layered-architecture.md)、[II-04](02-autosar-classic/04-configuration-arxml.md)、[II-03](02-autosar-classic/03-ecu-startup.md) | [IX-02](09-real-project-preparation/02-how-to-read-mcal.md) | SWS-MCU R24-11 p.13–14；SWS-CAN R22-11 p.33；SWS-DCM R20-11 p.50 | demo（缩小版工程） |
| IX-02 | [如何阅读真实 MCAL](09-real-project-preparation/02-how-to-read-mcal.md) | ✅ | [IX-01](09-real-project-preparation/01-how-to-read-real-autosar-project.md)、[III-01](03-mcal/01-mcal-overview.md)、[III-02](03-mcal/02-mcu-driver.md)、[III-03](03-mcal/03-port-driver.md)、[IV-08](04-can-mcal/08-can-configuration.md)、[IV-14](04-can-mcal/14-can-driver-from-scratch.md) | [IX-03](09-real-project-preparation/03-how-to-read-dcm.md) | SWS-CAN R22-11 p.57–131；SWS-MCU R24-11 p.25/41；HW-E §17 | demo `mcal/Can.c`；ref |
| IX-03 | [如何阅读陌生 DCM](09-real-project-preparation/03-how-to-read-dcm.md) | ✅ | [VI-01](06-dcm/01-dcm-overview.md)、[VI-02](06-dcm/02-dsl.md)、[VI-03](06-dcm/03-dsd.md)、[VI-04](06-dcm/04-dsp.md)、[VI-05](06-dcm/05-dcm-configuration.md)、[VIII-03](08-integration/03-dcm-integration.md)、[IX-01](09-real-project-preparation/01-how-to-read-real-autosar-project.md) | [IX-04](09-real-project-preparation/04-how-to-read-generated-code.md) | SWS-DCM R20-11 p.49–50/236–678 | demo `diag/`；oA `diagnostic/Dcm/` |
| IX-04 | [如何阅读生成代码](09-real-project-preparation/04-how-to-read-generated-code.md) | ✅ | [II-04](02-autosar-classic/04-configuration-arxml.md)、[II-05](02-autosar-classic/05-generated-code.md)、[VII-05](07-rte-swc/05-rte-generation.md)、[IX-01](09-real-project-preparation/01-how-to-read-real-autosar-project.md)、[IX-03](09-real-project-preparation/03-how-to-read-dcm.md) | [IX-05](09-real-project-preparation/05-how-to-trace-can-signal.md) | SWS-CAN R22-11 p.57/98–131；SWS-MCU R24-11 p.38 | demo 全部 `*_Cfg.*`、`rte/*` |
| IX-05 | [如何追踪 CAN 帧 / 信号](09-real-project-preparation/05-how-to-trace-can-signal.md) | ✅ | [IX-04](09-real-project-preparation/04-how-to-read-generated-code.md)、[IV-07](04-can-mcal/07-hoh-hrh-hth.md)、[V-01](05-can-stack/01-canif.md)、[V-06](05-can-stack/06-can-rx-path.md)、[V-07](05-can-stack/07-can-tx-path.md)、[VII-07](07-rte-swc/07-sender-receiver.md) | [IX-06](09-real-project-preparation/06-how-to-trace-uds-request.md) | SWS-CAN R22-11 p.48/58/122–129（Com/CanIf SWS 本仓库无） | oA `Com/src/Com_Com.c`、`CanIf_Cfg.c` |
| IX-06 | [如何在真实 ECU 上追踪 UDS 请求](09-real-project-preparation/06-how-to-trace-uds-request.md) | ✅ | [IX-05](09-real-project-preparation/05-how-to-trace-can-signal.md)、[IX-03](09-real-project-preparation/03-how-to-read-dcm.md)、[VIII-04](08-integration/04-f190-vin-demo.md)、[VIII-05](08-integration/05-uds-end-to-end.md) | [IX-07](09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md) | SWS-DCM R20-11 p.61/81/135–141/243–276 | `artifacts/uds-demo/trace.txt`；demo |
| IX-07 | [RTA-CAR DCM 升级准备清单](09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md) | ✅ | [IX-01](09-real-project-preparation/01-how-to-read-real-autosar-project.md)、[IX-03](09-real-project-preparation/03-how-to-read-dcm.md)、[IX-06](09-real-project-preparation/06-how-to-trace-uds-request.md)、[VIII-01](08-integration/01-ecu-configuration-checklist.md) | [DCM 升级指南](dcm-upgrade-guide.md) | SWS-DCM R20-11（R21-11+ 本仓库无） | demo `tests/test_uds_demo.c`、`tools/run_uds_demo.py` |

## Standalone guides（独立指南）

| Guide | Status | 定位 | 对应章节 |
|---|---|---|---|
| [调试手册：CANoe 发 22 F1 90，ECU 没有 response](debugging-autosar-diagnostics.md) | ✅ | 逐层排查（`claude_plan.md` §19），逐层调试方法只写在这里 | IV-15、VI-13、VIII-07 |
| [DCM 升级指南](dcm-upgrade-guide.md) | ✅ | 版本 / API / 配置 / callout / 生成代码 / 依赖 / 回归测试（§9） | VI-14、IX-07 |
| [AUTOSAR SWC + RTE 教程](autosar-swc-rte-tutorial.md) | ✅ | 从最简单 SWC 到 `22 F1 90` 读出 VIN 的导读版 | VII-01…VII-09 |

## Reference（随时查阅）

| Reference | 内容 |
|---|---|
| [autosar-api-map.md](reference/autosar-api-map.md) | API：谁调用、调用谁、上下文、同步/异步 |
| [autosar-module-map.md](reference/autosar-module-map.md) | 模块：层、SWS 可用性、openAUTOSAR / 本项目路径 |
| [rh850-autosar-mapping.md](reference/rh850-autosar-mapping.md) | RH850 寄存器块 → MCAL → API → 配置容器 |
| [can-configuration-map.md](reference/can-configuration-map.md) | 0x7E0/0x7E8 的 Can → CanIf → CanTp → PduR 参数链 |
| [dcm-configuration-map.md](reference/dcm-configuration-map.md) | DCM ECUC 容器 → demo `Dcm_Cfg.*` → RTE 接口 |
| [glossary.md](reference/glossary.md) | 中英术语表 |
| [source-traceability.md](reference/source-traceability.md) | API → SWS → openAUTOSAR → 本项目 → RH850 追踪表（行号已重新 grep，优先于研究笔记 03） |

## Demo code（可运行的教学实现）

| 代码 | 内容 | 运行 | 主要章节 |
|---|---|---|---|
| [examples/uds_diag_demo/](../examples/uds_diag_demo/README.md) | host 可运行教学诊断栈：Virtual CAN → Can → CanIf → CanTp → PduR → Dcm(DSL/DSD/DSP) → Rte → VehicleInfoSWC / SecurityAccessSWC；已知偏差 D1–D6 见其 README | `python tools/run_uds_demo.py`（输出 `artifacts/uds-demo/trace.txt`、`results.txt`） | V、VI、VII、VIII-04/05 |
| [examples/rh850_mcal_reference/](../examples/rh850_mcal_reference/README.md) | OSTM 驱动、CAN FD 位时间计算、tick 累加、MMIO shim | `python tools/run_host_tests.py`（输出 `artifacts/host-build/results.txt`） | I-07、III-05、IV-03、IV-14 |

## Research notes（写作底稿，事实来源）

| Note | 内容 |
|---|---|
| [00-writing-conventions.md](reference/research/00-writing-conventions.md) | 写作约定、关键已确认事实、标记规则 |
| [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) | 既有项目与文档审查、错误清单、拆分去向 |
| [02-autosar-sws-notes.md](reference/research/02-autosar-sws-notes.md) | MCU / CAN / DCM / IoHwAb SWS 笔记（PDF 页码） |
| [03-openautosar-trace.md](reference/research/03-openautosar-trace.md) | openAUTOSAR path:line 追踪（部分行号已过时，以 source-traceability 为准） |
| [04-rh850-hardware-notes.md](reference/research/04-rh850-hardware-notes.md) | RH850/P1M-E 硬件事实（手册页码） |
| [05-consistency-review-log.md](reference/research/05-consistency-review-log.md) | 跨章节一致性复审日志 |
| [analysis-and-learning-plan.md](analysis-and-learning-plan.md) | Phase 1 分析与学习计划 |

## Legacy（上一阶段产物，只作线索，不再维护）

每个 legacy 文件顶部都有 banner，写明迁移去向与已知更正。

| File | 迁移去向 |
|---|---|
| [legacy/rh850-autosar-tutorial-content-v1.md](legacy/rh850-autosar-tutorial-content-v1.md) | 本文件重构前的单页教程原文；逐节去向见其 banner |
| [rh850-hardware-handoff.md](rh850-hardware-handoff.md) | Part I、IV（RH850 硬件最详细的种子资料） |
| [agent-guide.md](agent-guide.md) | Part I、III、IV |
| [hardware-findings.md](hardware-findings.md) / [hardware-review.md](hardware-review.md) | Part I、IV |
| [mcal-reference-guide.md](mcal-reference-guide.md) | Part III、IV |
| [counter-design.md](counter-design.md) | II-06、III-05 |
| [internal-agent-task.md](internal-agent-task.md) / [requirements-status.md](requirements-status.md) | Part IX、学习路线图 |
| [source-index.md](source-index.md) / [online-references.md](online-references.md) | 资料来源与哈希；网上参考评估 |

## §23「最重要的问题」→ 回答它的章节

`claude_plan.md` §23：如果教程不能让你回答这 10 个问题，就说明还不够深入。每题的简答见 [VIII-05 §10](08-integration/05-uds-end-to-end.md#10-回答-23-最重要的问题)。

| # | 主题 | 问题 | 主要章节 | 辅助章节 |
|---|---|---|---|---|
| 1 | **Hardware** | RH850 reset 后到底发生了什么？ | [I-04](01-rh850/04-startup-process.md) | [I-02](01-rh850/02-cpu-architecture.md)、[I-03](01-rh850/03-memory-map.md)、[I-05](01-rh850/05-linker-script.md)、[I-06](01-rh850/06-interrupt-exception.md)、[I-07](01-rh850/07-clock-system.md)、[II-03](02-autosar-classic/03-ecu-startup.md) |
| 2 | **MCAL** | AUTOSAR Mcu/Port/Can Driver 到底如何操作 RH850 hardware？ | [III-02](03-mcal/02-mcu-driver.md)、[III-03](03-mcal/03-port-driver.md)、[IV-06](04-can-mcal/06-can-controller-init.md)、[IV-14](04-can-mcal/14-can-driver-from-scratch.md) | [III-01](03-mcal/01-mcal-overview.md)、[III-07](03-mcal/07-rh850-hardware-mapping.md)、[IV-09](04-can-mcal/09-can-init-implementation.md)…[IV-12](04-can-mcal/12-can-interrupt-implementation.md) |
| 3 | **CAN** | 一个 CAN frame 如何从 RH850 CAN peripheral 一路进入 DCM？ | [V-06](05-can-stack/06-can-rx-path.md) | [IV-05](04-can-mcal/05-can-interrupt.md)、[IV-11](04-can-mcal/11-can-rx-implementation.md)、[V-01](05-can-stack/01-canif.md)、[V-03](05-can-stack/03-cantp.md)、[V-05](05-can-stack/05-pdur.md) |
| 4 | **DCM** | `22 F1 90` 到达 ECU 后到底经过哪些函数？ | [VI-11](06-dcm/11-dcm-runtime-flow.md)、[VIII-05](08-integration/05-uds-end-to-end.md) | [VI-02](06-dcm/02-dsl.md)…[VI-04](06-dcm/04-dsp.md)、[VI-08](06-dcm/08-did.md)、[VIII-04](08-integration/04-f190-vin-demo.md) |
| 5 | **RTE** | DCM 如何最终调用 application software？ | [VII-08](07-rte-swc/08-dcm-rte-integration.md) | [SWC+RTE 教程](autosar-swc-rte-tutorial.md)、[VI-08](06-dcm/08-did.md)、[VII-06](07-rte-swc/06-client-server.md) |
| 6 | **SWC** | 一个真实 Classic AUTOSAR SWC 是如何定义、生成并运行的？ | [SWC+RTE 教程](autosar-swc-rte-tutorial.md)、[VII-09](07-rte-swc/09-diagnostic-swc-example.md) | [VII-01](07-rte-swc/01-swc-concept.md)…[VII-07](07-rte-swc/07-sender-receiver.md) |
| 7 | **Configuration** | ARXML / generated configuration 在整个过程中起什么作用？ | [II-04](02-autosar-classic/04-configuration-arxml.md)、[II-05](02-autosar-classic/05-generated-code.md) | [IV-08](04-can-mcal/08-can-configuration.md)、[VI-05](06-dcm/05-dcm-configuration.md)、[VII-05](07-rte-swc/05-rte-generation.md)、[VIII-01](08-integration/01-ecu-configuration-checklist.md)、[IX-04](09-real-project-preparation/04-how-to-read-generated-code.md)、[CAN 配置地图](reference/can-configuration-map.md)、[DCM 配置地图](reference/dcm-configuration-map.md) |
| 8 | **Scheduling** | Interrupt、OS Task、MainFunction、Runnable 之间是什么关系？ | [II-07](02-autosar-classic/07-mainfunction-scheduling.md) | [II-06](02-autosar-classic/06-os-task-isr.md)、[I-06](01-rh850/06-interrupt-exception.md)、[VI-12](06-dcm/12-dcm-mainfunction.md)、[VII-03](07-rte-swc/03-runnable-event.md) |
| 9 | **Debug** | CANoe 发出 UDS request 后 ECU 不响应，我应该从哪里开始查？ | [调试手册](debugging-autosar-diagnostics.md) | [IV-15](04-can-mcal/15-can-driver-debugging.md)、[VI-13](06-dcm/13-dcm-debugging.md)、[VIII-06](08-integration/06-canoe-test.md)、[VIII-07](08-integration/07-integration-debugging.md) |
| 10 | **Upgrade** | 升级 RTACAR DCM 时，我究竟应该检查哪些 dependency、configuration、API 和 behavior？ | [DCM 升级指南](dcm-upgrade-guide.md) | [VI-14](06-dcm/14-dcm-upgrade-guide.md)、[IX-03](09-real-project-preparation/03-how-to-read-dcm.md)、[IX-07](09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md)、[VI-05](06-dcm/05-dcm-configuration.md)、[DCM 配置地图](reference/dcm-configuration-map.md) |

## 已知限制与需在真实项目确认的事项

[Real Project Consideration] 下列限制贯穿全教程。读到相关章节时，这里的每一条都意味着“结论需在真实项目环境中确认”。

| # | 限制 | 影响 | 需要在真实项目中确认什么 / 去哪里找 |
|---|---|---|---|
| 1 | 本仓库**没有** CanIf / CanTp / PduR / Dem / NvM / Rte / Os（以及 EcuM / BswM / ComM / Port / Dio / Gpt / Icu）的 SWS | Part II、III、V、VII 中这些模块的 API 与配置名只按 AUTOSAR R4.x 公认形态讲解，不给 SWS ID | 用项目所用 release 的 SWS 与工具文档（RTA-CAR / Renesas MCAL 手册）核对 API 签名、配置容器与行为 |
| 2 | 本地 SWS release 混用：MCU = **R24-11**、CAN Driver = **R22-11**、DCM = **R20-11**、IoHwAb = **R24-11** | 跨模块接口（如 `Can_Write` 返回类型、DCM 端口签名）可能与项目 release 不同 | 先确定项目的 AUTOSAR release，再对照 [研究笔记 02](reference/research/02-autosar-sws-notes.md) 与 [DCM 升级指南](dcm-upgrade-guide.md) 做差异分析 |
| 3 | `AUTOSAR_SWS_Diagnostics.pdf` 是 **Adaptive Platform** 规范 | 不能作为 Classic DCM 依据；Part VI 只引用 DCM SWS CP R20-11 | Classic DCM 一律以 CP DCM SWS 为准 |
| 4 | 没有 **RH850G3M Software User's Manual**（指令集、系统寄存器完整定义） | [I-02](01-rh850/02-cpu-architecture.md)、[I-06](01-rh850/06-interrupt-exception.md) 中的指令/ABI 细节只到硬件手册可证的程度 | 向 Renesas 获取 G3M 软件手册；对照编译器（GHS）ABI 文档 |
| 5 | CAN 引脚 ALT 功能表来自 PDF 文本抽取，表格列位可能错位 | [IV-04](04-can-mcal/04-can-pin-transceiver.md)、[III-03](03-mcal/03-port-driver.md) 的 ALT 编号 | 打开 HW-E PDF 原页（p.70–74、p.151–154、p.793）逐格核对，并与原理图、封装（100 pin）一致 |
| 6 | **没有任何内容在真实 RH850 硬件上运行过** | 寄存器序列、中断、时序、位定时只经过手册核对与主机测试 | 上板后按 [IV-15](04-can-mcal/15-can-driver-debugging.md) 与 [调试手册](debugging-autosar-diagnostics.md) 的读回点逐项验证 |
| 7 | 教学 demo 与 SWS 的已知偏差 **D1–D6**（F1A0 读写签名混用；所有会话转换都锁安全级；安全级变化不做 `DcmSecurityAccess` 模式切换；UDS 会话值直接当 RTE 模式值；FF TxConfirmation 前到达的 FC 被丢弃；`E_NOT_OK` 时 trace 仍打印 response） | demo 行为在这些点上不等于标准 DCM / CanTp | 详见 [demo README “已知与规范的偏差”](../examples/uds_diag_demo/README.md) 与 [复审日志 §3](reference/research/05-consistency-review-log.md#3-追加到-demo-readme-的内容) |
| 8 | openAUTOSAR（Arctic Core 2.18.0）是 R3.1.5 风格，无 Can driver、无 RH850 port、配置大量缺失、无法链接运行 | 只能作为“读架构与状态机”的参照，不能当 R4.x 实现 | 以真实项目 BSW 源码为准；行号以 [source-traceability](reference/source-traceability.md) 为准 |
| 9 | 目标工程背景（RTA-CAR 12.9.0、RTA-OS RH850GHS port、GHS、Renesas P1M MCAL AR 4.2.2）只来自截图 | 只能作为“可能的真实环境”提及 | 进入项目后用 [IX-01](09-real-project-preparation/01-how-to-read-real-autosar-project.md) 的清单确认版本与路径 |
| 10 | OSTM0 / OSTM1 归 Gpt 还是 OS 在不同资料中不一致 | 这是**配置选择**，不是硬件事实 | 查项目 OS 与 Gpt 配置中的计数器/通道分配 |

## 进度汇总

| Part | 章节数 | 已完成 | 总行数 |
|---|---|---|---|
| I — RH850 Fundamentals | 8 | 8 | 3985 |
| II — AUTOSAR Classic | 7 | 7 | 3451 |
| III — MCAL | 7 | 7 | 3384 |
| IV — CAN MCAL Driver（里程碑 1） | 15 | 15 | 8898 |
| V — CAN Communication Stack | 7 | 7 | 3535 |
| VI — DCM | 14 | 14 | 6920 |
| VII — RTE / SWC | 9 | 9 | 4386 |
| VIII — Integration | 7 | 7 | 2185 |
| IX — Real Project Preparation | 7 | 7 | 1295 |
| **合计** | **81** | **81** | **38039** |

目标目录（`claude_plan.md`「推荐目录结构」）中的全部章节文件均已存在；当前没有 ⬜ 章节。后续工作集中在“已知限制”中需要真实项目 / 真实硬件确认的事项。
