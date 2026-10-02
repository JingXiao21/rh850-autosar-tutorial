# RH850/P1M-E ↔ AUTOSAR 映射：寄存器块 → MCAL 驱动 → AUTOSAR API → 配置容器

> 对应硬件: **R7F701381 = RH850/P1M-E**（core RH850**G3M**，lock-step；不是 G4MH）。页码 “HW-E p.N” = RH850/P1M-E User's Manual: Hardware R01UH0585EJ0120 Rev.1.20 的 PDF 页（与印刷页一致）；“DS-E” = P1M-E Datasheet Rev.1.00。事实来源与复核状态见研究笔记 [04](research/04-rh850-hardware-notes.md) 与 [01 §4](research/01-project-and-docs-review.md)。
> 对应规范: SWS MCU R24-11、SWS CAN R22-11（ECUC 容器 ID 与页码）。Port/Dio/Gpt/Icu/Wdg/Os **本仓库无 SWS**，容器名为 R4.x 公认形态，需以真实项目确认。
> 对应源码: `examples/rh850_mcal_reference/`（OSTM、CAN 位时间、MMIO）、`examples/uds_diag_demo/mcal/Can.c`（标注 `[RH850 Hardware]` 的位置）、`docs/04-can-mcal/14-can-driver-from-scratch.md`（RS-CANFD 教学驱动）

`[RH850 Hardware]` 本页所有寄存器/地址/通道号仅适用于 P1M-E。P1x（非 -E，RS-CAN、有 PROT1PHCMD）与 P1x-C（M_CAN）**不能套用**（HW-X p.772、DS-C p.4）。

---

## 1. 总表

| RH850 外设 / 寄存器块（手册页） | MCAL 驱动 | AUTOSAR API | 配置容器（ECUC） | 章节 |
|---|---|---|---|---|
| 时钟：MainOSC 16 MHz 固定 → PLL → CLK_CPU 160 / CLK_HSB 80 / CLK_LSB 40 MHz；寄存器仅 CLKD2/3DIV、CKSC2/3/8（HW-E p.469–471） | Mcu | `Mcu_Init`、`Mcu_InitClock`、`Mcu_GetPllStatus`、`Mcu_DistributePllClock` | `McuClockSettingConfig`（ECUC_Mcu_00124）→ `McuClockReferencePoint`（00174，p.49–50）；`McuNoPll`（00180，p.41） | [03-mcal/02](../03-mcal/02-mcu-driver.md)、[01-rh850/07](../01-rh850/07-clock-system.md) |
| 写保护：**无 PROTCMDn/PROTSn**；P-Bus Guard / Slave Guard；0xA5 序列仅 CLMAnPCMD、ECMPCMD、FLMDPCMD（HW-E p.471, p.2764, p.2795, p.2870–2871） | Mcu（CLMA）/ 启动代码 | `Mcu_InitClock`（P1M-E 上可能为平凡实现） | `McuGeneralConfiguration` | [01-rh850/07](../01-rh850/07-clock-system.md) |
| 复位：RESF `0xFFF8_1000`（HW-E p.420–422）；SWSRESA0 / SWARESA0（HW-E p.418, p.424–425） | Mcu | `Mcu_GetResetReason`、`Mcu_GetResetRawValue`、`Mcu_PerformReset` | `McuResetSetting`（00173）、`McuResetReasonConf`（00185，p.51） | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| RAM：LRAM self `0xFEDE_0000`–`0xFEDF_FFFF`、GRAM A/B `0xFEEF_8000`/`0xFEF0_0000`；复位时硬件清零 + ECC；STAC_* 控制（HW-E p.257, p.427–430, p.2890） | Mcu / 启动代码 / MemMap | `Mcu_InitRamSection`、`Mcu_GetRamState` | `McuRamSectorSettingConf`（00120，p.47–49） | [01-rh850/03](../01-rh850/03-memory-map.md)、[02-autosar-classic/05](../02-autosar-classic/05-generated-code.md) |
| PORT：基址 `0xFFC1_0000`，组步长 0x40；PMC/PFC/PFCE/PFCAE（ALT1–6 编码）、PM、PIPC（CAN 引脚 PIPC=0）（HW-E p.91, p.94, p.99–101, p.131） | Port | `Port_Init`、`Port_SetPinMode` | `PortContainer` / `PortPin`（PortPinMode 等）——本仓库无 Port SWS | [03-mcal/03](../03-mcal/03-port-driver.md) |
| CAN 引脚：CAN0 RX/TX = P2_0/P2_1（ALT1）、P3_7/P3_8（ALT3）、P4_5/P4_6（ALT3）；CAN2 TX P5_5（ALT6）（HW-E p.793；ALT 号需对照 PDF 原表 p.151–154） | Port | `Port_Init` | `PortPin` | [04-can-mcal/04](../04-can-mcal/04-can-pin-transceiver.md) |
| Pn / PSRn（高 16 位写使能，原子置位）/ PPRn（HW-E p.96, p.99） | Dio | `Dio_WriteChannel`、`Dio_ReadChannel` | `DioChannel` ——本仓库无 Dio SWS | [03-mcal/04](../03-mcal/04-dio-driver.md) |
| OSTM0 `0xFFDD_8000` / OSTM1 `0xFFDD_9000`（**无 OSTM2**）；CMP+00、CNT+04、TE+10、TS+14、TT+18、CTL+20；PCLK = 80 MHz（HW-E p.1542–1556） | Gpt（或 OS Counter 硬件） | `Gpt_Init`、`Gpt_StartTimer`、`Gpt_EnableNotification` | `GptChannelConfiguration`（GptChannelTickFrequency 等）——本仓库无 Gpt SWS | [03-mcal/05](../03-mcal/05-gpt-driver.md)；代码 `examples/rh850_mcal_reference/mcal/gpt/Ostm.c:16-77` |
| INTOSTM0/1 = EI74/75；OSTM3–7 只走 **FEINT**（HW-E p.283, p.1544） | Gpt / Os | Gpt 通知、OS 计数器 ISR | OS ISR 配置 | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| TAUD0–2（16 ch, 16 位）`0xFFE2_0000`…；TAUJ0–2（4 ch, 32 位）`0xFFE5_0000`…；INTP0–12（HW-E p.1571–1572, p.1878–1879, p.280） | Icu（/ Pwm） | `Icu_EnableEdgeDetection`、`Icu_GetTimeElapsed` 等 | `IcuChannel` ——本仓库无 Icu SWS | [03-mcal/06](../03-mcal/06-icu-driver.md) |
| WDTA0 `0xFFD7_4000`；WDTE 写 0xAC；OPBT0 决定是否自动启动（HW-E p.1522–1531, p.2884） | Wdg | `Wdg_Init`、`Wdg_SetTriggerCondition` | `WdgSettingsConfig` ——本仓库无 Wdg SWS | — |
| **RS-CANFD** 单元 RSCFD0，基址 `0xFFD2_0000`，CAN0–2；Classical / FD 两套寄存器映射（GRMCFG.RCMC，HW-E p.788–802, p.916–919） | Can | `Can_Init` | `CanConfigSet`（ECUC_Can_00343，p.131）；`CanControllerBaseAddress`（00382，p.108） | [04-can-mcal/02](../04-can-mcal/02-rh850-can-peripheral.md) |
| 通道 CANm：CmCFG / CmCTR / CmSTS / CmERFL（`+0x10×m`，Classical）（HW-E p.798, p.803–815） | Can | `Can_SetControllerMode`、`Can_GetControllerMode`、`Can_GetControllerErrorState` | `CanController`（00354，p.107）、`CanControllerId`（00316，p.109） | [04-can-mcal/06](../04-can-mcal/06-can-controller-init.md) |
| 位时间：CmCFG（SJW/TSEG2/TSEG1/BRP）或 CmNCFG/CmDCFG；fCAN = clkc 40 MHz 或 clk_xincan 16 MHz（GCFG.DCS），**不是 80 MHz**（HW-E p.791, p.803–804, p.817, p.921, p.935, p.1092） | Can | `Can_Init`、`Can_SetBaudrate` | `CanControllerBaudrateConfig`（00387，p.114）：BaudRate 00005、PropSeg 00073、Seg1 00074、Seg2 00075、SJW 00383（p.114–117）；`CanCpuClockRef`（00313，p.112）→ `McuClockReferencePoint` | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md)；代码 `examples/rh850_mcal_reference/mcal/can/Can_BitTiming.c:19` |
| 接收规则 GAFLIDj/GAFLMj/GAFLP0_j/GAFLP1_j（**GAFLM 位 = 1 表示比较**）；GAFLCFG0、GAFLECTR（HW-E p.830–837, p.1072–1074） | Can | （Init 时写入） | `CanHardwareObject`（00324，p.122）RECEIVE + `CanHwFilter`（00468）：Code 00469、Mask 00470（p.129–130） | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md)、[08](../04-can-mcal/08-can-configuration.md) |
| RX FIFO：RFCCx / RFSTSx / RFPCTRx / RFIDx…（8 个共享 FIFO）；RX buffer RMNB/RMNDy（HW-E p.838–848, p.1100–1102） | Can | `Can_MainFunction_Read`、RX ISR → `CanIf_RxIndication` | HRH 的 `CanHwObjectCount`（00467，p.124）；`CanRxProcessing`（00317，p.109） | [04-can-mcal/11](../04-can-mcal/11-can-rx-implementation.md) |
| TX buffer：TMCp（**8 位**）/ TMSTSp（**8 位**，TMTRF）/ TMIDp / TMPTRp / TMDF0/1_p；每通道 16 个（HW-E p.878–887, p.1107–1111） | Can | `Can_Write`、`Can_MainFunction_Write` → `CanIf_TxConfirmation` | `CanHardwareObject` TRANSMIT（HTH）；`CanTxProcessing`（00318，p.110） | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| Bus-off：CmERFL.BOEF、CmSTS.BOSTS/TEC/REC、CmCTR.BOM（00=ISO 自动恢复，01=进入 bus-off 转 halt …）（HW-E p.807–815, p.1069） | Can | `Can_MainFunction_BusOff` → `CanIf_ControllerBusOff`；驱动不得自动恢复（`SWS_Can_00274` p.43） | `CanBusoffProcessing`（00314，p.107）；`CanMainFunctionBusoffPeriod`（00355，p.101） | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| CAN 中断：EI183/184/185（CAN0 ERR/REC(common FIFO)/TRX）、186–188（CAN1）、**189 INTRCANGERR**、**190 INTRCANGRECC（RX FIFO0–7）**、191–193（CAN2）；EIC 地址 `0xFFFF_B000 + 2n`；电平型（HW-E p.267, p.285–286, p.792） | Can（ISR 体）+ Os（向量、EIC） | Can ISR → CanIf 回调 | OS 的 ISR 配置（Cat 2、优先级、通道）；`CanRxProcessing/CanTxProcessing = INTERRUPT` | [04-can-mcal/05](../04-can-mcal/05-can-interrupt.md)、[02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| INTC1（EIC0–31，`0xFFFE_EA00+2n`）/ INTC2（EIC32–383）；EICn：EIMK、EITB、EIP（0 最高）；INTBP 表 `INTBP + 4×ch`；直接向量 +100H…+1F0H（HW-E p.265–268, p.281–282） | Os 端口 | `ISR()`、`SuspendAllInterrupts` 等（OS API） | OS 配置（ISR 类别、优先级） | [01-rh850/06](../01-rh850/06-interrupt-exception.md)、[02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| PSW.ID / PMR / ISPR（HW-E p.198, p.210–211） | Os 端口 / SchM | `SchM_Enter_<Mod>_<EA>` / `SchM_Exit_*` | RTE/SchM 的 ExclusiveArea 配置 | [02-autosar-classic/07](../02-autosar-classic/07-mainfunction-scheduling.md) |
| Data Flash `0xFF20_0000`–`0xFF20_7FFF`（32 KB，64 B 块）；FACI（HW-E p.257, p.2859；Flash 自编程手册不在仓库） | Fls（→ Fee → NvM） | `Fls_Write/Erase` → `NvM_WriteBlock` | `FlsSector`、`NvMBlockDescriptor` ——本仓库无对应 SWS | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md)（NvM 部分） |

---

## 2. CAN 资源 → AUTOSAR 对象：一对一还是多对一

| RS-CANFD 资源 | AUTOSAR 对象 | 本项目对应 | 注意 |
|---|---|---|---|
| 单元 RSCFD0（3 通道共享 global 配置） | 一个 Can Driver 实例 | — | 规则表、RX FIFO、GCFG 是全局的：只能在 global reset 中写（HW-E p.817, p.831） |
| 通道 CANm | `CanController` | `examples/uds_diag_demo/mcal/Can_Cfg.h:18`（`CanConf_CanController_CAN0`） | 控制器 ID 与硬件通道号不一定相同 |
| 一条/多条接收规则 + RX FIFO x | HRH（`CanObjectType = RECEIVE`） | `mcal/Can_Cfg.c:18-19`；capstone 规则表 `docs/04-can-mcal/14-can-driver-from-scratch.md:1199-1202` | 规则的 label（GAFLP0_j.PTR）可用来携带 HRH 号（demo `mcal/Can.c:77-79`） |
| TX buffer p（`16m..16m+15`） | HTH（`CanObjectType = TRANSMIT`） | `mcal/Can_Cfg.c:20`（HTH2 → TX buffer 0） | HTH 也可用 TX queue / TX-RX FIFO 实现，取决于 MCAL |
| channel communication / reset / halt / stop | `CAN_CS_STARTED` / `STOPPED` / （逻辑）`SLEEP` | `mcal/Can.c:106-136` 的转换合法性检查 | 一种可能的映射；RS-CANFD 无 CAN 唤醒描述，SLEEP 语义按 MCAL 确认 |
| TMSTSp.TMTRF = 10B | `CanIf_TxConfirmation` | `mcal/Can.c:229-235` | 必须写 00B 清除才能再次发送 |
| RFSTSx.RFIF / EI190 | `CanIf_RxIndication` | `mcal/Can.c:255-277` | 先清模块内标志（电平型中断），再 dummy read + SYNCP（HW-E p.254） |

---

## 3. P1M-E 特有陷阱（与“典型 RH850”说法冲突的地方）

| 常见说法 | P1M-E 实际 | 依据 |
|---|---|---|
| “`Mcu_InitClock` 要走 PROTCMD 0xA5 解锁序列配置 PLL” | P1M-E 没有 PROTCMDn，也没有软件可配 PLL 寄存器 | HW-E p.471；全文 grep 0 命中 |
| “fCAN = 80 MHz” | 80 MHz 是 pclk（接口时钟）；位时间时钟是 40 MHz clkc 或 16 MHz clk_xincan | HW-E p.791, p.817 |
| “EI184 是 CAN RX FIFO 中断” | EI184 是 CAN0 TX/RX FIFO（common FIFO）接收中断；RX FIFO0–7 用 **EI190** | HW-E p.285–286, p.792 |
| “GAFLM 写 0 表示精确匹配” | GAFLM 位 = 1 表示比较，= 0 表示不比较 | HW-E p.834 |
| “OSTM0 一定给 OS，OSTM1 给 Gpt” | 这是配置选择，不是硬件事实；各文档说法不一 | 研究笔记 01 §3 |
| “DMA 可以访问栈/全局变量” | DMA 不能访问 LRAM self 地址，需用 PE1 别名 `0xFEBE_xxxx` | HW-E p.259 |

相关：[03-mcal/07 RH850 硬件映射](../03-mcal/07-rh850-hardware-mapping.md)、[can-configuration-map.md](can-configuration-map.md)、[source-traceability.md](source-traceability.md)。
