# 术语表（中 / 英）

> 用途: 本教程统一用语。每个术语给出英文全称、中文说明、以及首选阅读章节。规范出处只在需要时注明（release + 页码）；**本仓库没有 CanIf / CanTp / PduR / Dem / Rte / Os SWS**，相关术语的定义按 R4.x 公认含义，需以真实项目所用 release 确认。
> 硬件术语以 RH850/P1M-E（R7F701381，RH850G3M）为准，页码 “HW-E p.N” 见研究笔记 [04](research/04-rh850-hardware-notes.md)。

---

## 1. RH850 硬件

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| RH850/P1M-E | — | 本教程参考器件系列；R7F701381 为 100-pin、1 MB Code Flash 型号（DS-E p.2） | [01-rh850/01](../01-rh850/01-rh850-overview.md) |
| G3M | RH850G3M core | P1M-E 的 CPU 核，主核 + lock-step 检查核；**不是 G4MH** | [01-rh850/02](../01-rh850/02-cpu-architecture.md) |
| 锁步 | Lock-step | 检查核与主核同步执行比较，检查核不是第二个可调度核 | [01-rh850/02](../01-rh850/02-cpu-architecture.md) |
| PSW | Program Status Word | 含 ID（中断屏蔽）、NP、EBV、UM 等位（HW-E p.197–198） | [01-rh850/02](../01-rh850/02-cpu-architecture.md) |
| EIPC / EIPSW / EIIC | EI-level PC / PSW / Interrupt Cause | EI 级异常时硬件保存的现场；EIIC = `0x1000 + 通道号` | [01-rh850/06](../01-rh850/06-interrupt-exception.md) |
| FE / EI 级异常 | FE-level / EI-level exception | FE 级（FENMI、FEINT、SYSERR）优先于 EI 级（EIINT 等） | [01-rh850/06](../01-rh850/06-interrupt-exception.md) |
| INTC1 / INTC2 | Interrupt Controller 1/2 | INTC1 管 EIC0–31（CPU 私有），INTC2 管 EIC32–383 | [01-rh850/06](../01-rh850/06-interrupt-exception.md) |
| EIC | EI Interrupt Control register | 每个中断通道一个 16 位寄存器：EIMK（屏蔽）、EITB（向量方式）、EIP（优先级，0 最高） | [01-rh850/06](../01-rh850/06-interrupt-exception.md) |
| INTBP | Interrupt Base Pointer | 表引用方式的中断地址表基址，处理函数地址 = `INTBP + 4×ch` | [01-rh850/06](../01-rh850/06-interrupt-exception.md) |
| RBASE / EBASE | Reset / Exception vector base | 异常向量基址；PSW.EBV 选择 | [01-rh850/04](../01-rh850/04-startup-process.md) |
| ISPR / PMR | In-Service Priority / Priority Mask Register | 正在服务的优先级 / 软件优先级屏蔽，OS 临界区可能用到 | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| SYNCP | Synchronize Pipeline | 清外设中断源后、开中断前需要的同步指令（HW-E p.254） | [04-can-mcal/12](../04-can-mcal/12-can-interrupt-implementation.md) |
| LRAM / GRAM | Local RAM / Global RAM | LRAM self `0xFEDE_0000`；GRAM A/B；复位时硬件清零并写 ECC | [01-rh850/03](../01-rh850/03-memory-map.md) |
| Code Flash / Data Flash | — | 代码区 `0x0000_0000`；Data Flash `0xFF20_0000`（EEPROM 仿真 / NvM 后端） | [01-rh850/03](../01-rh850/03-memory-map.md) |
| ECC | Error Correction Code | 除 I-Cache 外所有 RAM 带 ECC | [01-rh850/03](../01-rh850/03-memory-map.md) |
| CLK_CPU / CLK_HSB / CLK_LSB | CPU / High-Speed Bus / Low-Speed Bus clock | 160 / 80 / 40 MHz，P1M-E 上固定 | [01-rh850/07](../01-rh850/07-clock-system.md) |
| MainOSC | Main Oscillator | P1M-E 只支持 16 MHz | [01-rh850/07](../01-rh850/07-clock-system.md) |
| PLL | Phase-Locked Loop | P1M-E **没有**软件可配的 PLL 寄存器 | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| PROTCMD | Protection Command register | 其他 RH850 系列的写保护命令寄存器；**P1M-E 不存在** | [01-rh850/07](../01-rh850/07-clock-system.md) |
| P-Bus Guard / Slave Guard | — | P1M-E 的外设访问保护机制 | [01-rh850/07](../01-rh850/07-clock-system.md) |
| RESF | Reset Factor register | 复位原因寄存器 `0xFFF8_1000` | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| Option Byte | — | OPBT0 等，决定 WDTA 是否自动启动等（HW-E p.2884） | [01-rh850/04](../01-rh850/04-startup-process.md) |
| PORT / PMC / PFC | Port / Port Mode Control / Port Function Control | 引脚复用配置；ALT1–6 由 `[PFCAE,PFCE,PFC]` 编码 | [03-mcal/03](../03-mcal/03-port-driver.md) |
| PIPC | Port IP Control | 直接 I/O 控制；CAN 引脚必须为 0 | [04-can-mcal/04](../04-can-mcal/04-can-pin-transceiver.md) |
| OSTM | OS Timer | 32 位定时器 OSTM0/1/3–7（无 OSTM2）；OSTM0/1 = EI74/75 | [03-mcal/05](../03-mcal/05-gpt-driver.md) |
| TAUD / TAUJ | Timer Array Unit D / J | 多通道定时器，可做输入捕获（Icu）/ PWM | [03-mcal/06](../03-mcal/06-icu-driver.md) |
| WDTA | Watchdog Timer A | P1M-E 只有 WDTA0 | [01-rh850/08](../01-rh850/08-peripheral-overview.md) |
| RS-CANFD | Renesas CAN FD controller | P1M-E 的 CAN 外设，单元 RSCFD0、3 通道、基址 `0xFFD2_0000` | [04-can-mcal/02](../04-can-mcal/02-rh850-can-peripheral.md) |
| Global mode / Channel mode | — | RS-CANFD 全局（stop/reset/test/operating）与通道（stop/reset/halt/communication）模式 | [04-can-mcal/06](../04-can-mcal/06-can-controller-init.md) |
| GCFG.DCS | Global Config – Data bit-rate Clock Select | 选择 fCAN：clkc 40 MHz 或 clk_xincan 16 MHz | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md) |
| CmCFG / CmNCFG / CmDCFG | Channel bit-timing config | Classical / FD nominal / FD data 位时间寄存器 | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md) |
| 接收规则 | Receive rule (GAFLID/GAFLM/GAFLP0/GAFLP1) | 硬件过滤器；**GAFLM 位 = 1 表示比较** | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md) |
| RX FIFO / RX buffer | — | 排队式 / 覆盖式接收存储；RX FIFO 中断 = EI190 | [04-can-mcal/11](../04-can-mcal/11-can-rx-implementation.md) |
| TX buffer | Transmit buffer (TMCp/TMSTSp/TMIDp…) | 每通道 16 个；TMCp、TMSTSp 只能 8 位访问 | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| TMTRF | Transmit Result Flag | TMSTSp[2:1]，10B = 发送完成，写 00B 清除 | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| BOM | Bus-Off recovery Mode | CmCTR.BOM，决定 bus-off 后硬件行为 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| TEC / REC | Transmit / Receive Error Counter | CmSTS 中的错误计数 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| MMIO | Memory-Mapped I/O | 本项目的可注入寄存器访问层 `examples/rh850_mcal_reference/platform/Rh850_Mmio.c` | [03-mcal/01](../03-mcal/01-mcal-overview.md) |

## 2. CAN 与 ISO-TP 基础

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| CAN | Controller Area Network | 多主广播总线，按 ID 仲裁 | [04-can-mcal/01](../04-can-mcal/01-can-hardware-basics.md) |
| CAN FD | CAN with Flexible Data-rate | 数据段可更高速率、最长 64 字节 | [04-can-mcal/01](../04-can-mcal/01-can-hardware-basics.md) |
| 仲裁 | Arbitration | 显性位优先，ID 小者赢 | [04-can-mcal/01](../04-can-mcal/01-can-hardware-basics.md) |
| DLC | Data Length Code | 帧长度码 | [04-can-mcal/01](../04-can-mcal/01-can-hardware-basics.md) |
| 位时间 / Tq | Bit timing / Time quantum | `bitrate = fCAN / [divider × (1+TSEG1+TSEG2)]` | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md) |
| 采样点 | Sample point | 位内采样位置，常见 75%–87.5% | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md) |
| SJW | Synchronization Jump Width | 重同步最大调整量 | [04-can-mcal/03](../04-can-mcal/03-can-clock-bit-timing.md) |
| 收发器 | Transceiver | CAN_H/CAN_L 与 TX/RX 电平转换芯片 | [04-can-mcal/04](../04-can-mcal/04-can-pin-transceiver.md) |
| Bus-off | — | TEC > 255 后节点离开总线 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| Error passive | — | 错误计数 ≥ 128 的受限状态 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| ISO-TP | ISO 15765-2 Transport Protocol | CAN 上的分段传输协议 | [05-can-stack/04](../05-can-stack/04-isotp.md) |
| SF / FF / CF / FC | Single / First / Consecutive / Flow Control Frame | ISO-TP 四种帧 | [05-can-stack/04](../05-can-stack/04-isotp.md) |
| BS | Block Size | 一个 FC 之后允许连续发送的 CF 数 | [05-can-stack/04](../05-can-stack/04-isotp.md) |
| STmin | Separation Time minimum | CF 之间最小间隔 | [05-can-stack/04](../05-can-stack/04-isotp.md) |
| N_As / N_Ar / N_Bs / N_Br / N_Cs / N_Cr | ISO-TP network-layer timers | 发送确认、等待 FC、等待 CF 等超时 | [05-can-stack/04](../05-can-stack/04-isotp.md) |
| FC.WAIT / WFTmax | Flow Control Wait / max Wait Frame Transmissions | 接收方暂无缓冲时发 WAIT，最多 WFTmax 次 | [05-can-stack/04](../05-can-stack/04-isotp.md) |
| 物理寻址 / 功能寻址 | Physical / Functional addressing | 本教程示例：0x7E0 物理请求、0x7DF 功能请求、0x7E8 响应 | [05-can-stack/06](../05-can-stack/06-can-rx-path.md) |
| Padding | — | 帧填充到 8 字节（demo `0xCC`） | [05-can-stack/03](../05-can-stack/03-cantp.md) |

## 3. AUTOSAR 架构与方法论

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| AUTOSAR Classic Platform | CP | 面向静态配置 ECU 的 AUTOSAR 平台 | [02-autosar-classic/01](../02-autosar-classic/01-classic-platform-overview.md) |
| Adaptive Platform | AP | 面向高性能 POSIX ECU；`AUTOSAR_SWS_Diagnostics.pdf` 属 AP，不能用于 Classic DCM | [02-autosar-classic/01](../02-autosar-classic/01-classic-platform-overview.md) |
| SWS | Software Specification | 模块规范；本仓库有 MCU R24-11、CAN R22-11、DCM R20-11、IoHwAb R24-11 | [reference/autosar-module-map](autosar-module-map.md) |
| Release | — | R19-11 起 AUTOSAR 文档以 release 名作版本（如 R20-11） | [06-dcm/14](../06-dcm/14-dcm-upgrade-guide.md) |
| BSW | Basic Software | RTE 以下的标准软件 | [02-autosar-classic/02](../02-autosar-classic/02-layered-architecture.md) |
| MCAL | Microcontroller Abstraction Layer | 直接访问片上外设的驱动层（Mcu、Port、Dio、Gpt、Can…） | [03-mcal/01](../03-mcal/01-mcal-overview.md) |
| ECUAL | ECU Abstraction Layer | 抽象板级布线（CanIf、IoHwAb…） | [02-autosar-classic/02](../02-autosar-classic/02-layered-architecture.md) |
| Service Layer | — | 与硬件无关的服务（CanTp、PduR、Dcm、Dem、NvM、EcuM…） | [02-autosar-classic/02](../02-autosar-classic/02-layered-architecture.md) |
| CDD | Complex Device Driver | 不符合标准分层的特殊驱动 | [02-autosar-classic/02](../02-autosar-classic/02-layered-architecture.md) |
| RTE | Runtime Environment | SW-C 之间、SW-C 与 BSW 之间的通信层，由工具生成 | [07-rte-swc/04](../07-rte-swc/04-rte-concept.md) |
| SW-C | Software Component | 应用组件，只通过 RTE 通信 | [07-rte-swc/01](../07-rte-swc/01-swc-concept.md) |
| Runnable | Runnable Entity | SW-C 中被 RTE 调度的函数 | [07-rte-swc/03](../07-rte-swc/03-runnable-event.md) |
| RTE Event | — | 触发 runnable 的事件（TimingEvent、OperationInvokedEvent…） | [07-rte-swc/03](../07-rte-swc/03-runnable-event.md) |
| Port / Port Interface | — | SW-C 的通信端点 / 端口类型（S/R、C/S、Mode…） | [07-rte-swc/02](../07-rte-swc/02-port-interface.md) |
| Client-Server | C/S | 同步或异步的“函数调用”式接口；DCM 的 `DataServices_*` 属此类 | [07-rte-swc/06](../07-rte-swc/06-client-server.md) |
| Sender-Receiver | S/R | 数据元素的发布/接收 | [07-rte-swc/07](../07-rte-swc/07-sender-receiver.md) |
| Service Component | — | 以 SW-C 形式暴露端口的 BSW（如 Dcm、NvM） | [07-rte-swc/08](../07-rte-swc/08-dcm-rte-integration.md) |
| ECUC | ECU Configuration | 模块配置参数与容器的值 | [02-autosar-classic/04](../02-autosar-classic/04-configuration-arxml.md) |
| ARXML | AUTOSAR XML | AUTOSAR 交换格式 | [02-autosar-classic/04](../02-autosar-classic/04-configuration-arxml.md) |
| 容器 / 参数 | Container / Parameter | ECUC 的层级结构（如 `CanConfigSet/CanHardwareObject/CanObjectId`） | [02-autosar-classic/04](../02-autosar-classic/04-configuration-arxml.md) |
| Pre-compile / Link-time / Post-build | 配置类 | 值最晚确定的时机；对应 `*_Cfg.h` / `*_Lcfg.c` / `*_PBcfg.c` | [02-autosar-classic/05](../02-autosar-classic/05-generated-code.md) |
| 变体 | Variant (VARIANT-PRE-COMPILE / POST-BUILD) | 模块支持的配置实现方式 | [02-autosar-classic/04](../02-autosar-classic/04-configuration-arxml.md) |
| 生成代码 | Generated code | 由配置工具生成的配置表、RTE、SchM、MemMap | [02-autosar-classic/05](../02-autosar-classic/05-generated-code.md) |
| ConfigPtr | Configuration pointer | 传给 `<Mod>_Init` 的根配置结构体地址（`SWS_Can_00056` p.44） | [02-autosar-classic/05](../02-autosar-classic/05-generated-code.md) |
| 符号名 | Symbolic name (`<Mip>Conf_…`) | 生成头文件中代表配置实例 handle 的宏 | [02-autosar-classic/05](../02-autosar-classic/05-generated-code.md) |
| MemMap | Memory Mapping | `<MIP>_START_SEC_<CLASS>` 宏 → 编译器段 → 链接区 | [02-autosar-classic/05](../02-autosar-classic/05-generated-code.md)、[01-rh850/05](../01-rh850/05-linker-script.md) |
| 链接脚本 | Linker script | 把段放到 Flash/RAM 地址 | [01-rh850/05](../01-rh850/05-linker-script.md) |
| DET | Default Error Tracer | 开发错误上报（`Det_ReportError`） | [06-dcm/13](../06-dcm/13-dcm-debugging.md) |
| Runtime error | — | 运行时错误（`Det_ReportRuntimeError`），如 `CAN_E_DATALOST` | [04-can-mcal/15](../04-can-mcal/15-can-driver-debugging.md) |
| Std_ReturnType | — | `E_OK` / `E_NOT_OK`（及模块扩展值，如 `CAN_BUSY`） | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| PduIdType / PduInfoType | — | PDU 句柄 / 数据指针 + 长度（ComStack_Types） | [05-can-stack/01](../05-can-stack/01-canif.md) |
| BufReq_ReturnType | — | TP 接口返回值：`BUFREQ_OK / E_NOT_OK / E_BUSY / E_OVFL` | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| PDU / SDU | Protocol / Service Data Unit | PDU = 某层收发的单元；SDU = 上层交给本层的数据 | [05-can-stack/01](../05-can-stack/01-canif.md) |
| L-PDU / N-PDU / N-SDU / I-PDU | Link / Network PDU / SDU / Interaction PDU | 分别是 CanIf、CanTp 帧、CanTp 完整消息、Com/PduR 层的 PDU | [05-can-stack/01](../05-can-stack/01-canif.md)、[03](../05-can-stack/03-cantp.md) |

## 4. CAN 通信栈模块与 API 概念

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| Can Driver | — | MCAL 的 CAN 驱动，唯一上层是 CanIf（CAN SWS R22-11 p.14） | [04-can-mcal/09](../04-can-mcal/09-can-init-implementation.md) |
| CanController | — | 一个 CAN 通道的配置容器（ECUC_Can_00354） | [04-can-mcal/08](../04-can-mcal/08-can-configuration.md) |
| Hardware Object | — | CAN RAM 中的一个报文缓冲 | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md) |
| HOH | Hardware Object Handle | HRH 与 HTH 的统称；HRH 在前、连续编号（ECUC_Can_00326 p.125） | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md) |
| HRH | Hardware Receive Handle | 接收用 HOH；RS-CANFD 上对应规则 + FIFO | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md) |
| HTH | Hardware Transmit Handle | 发送用 HOH；`Can_Write` 第一个参数 | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md) |
| BasicCAN / FullCAN | `CanHandleType` | 一个硬件对象处理多个 / 单个 ID | [04-can-mcal/07](../04-can-mcal/07-hoh-hrh-hth.md) |
| swPduHandle | — | `Can_PduType` 中的 CanIf L-PDU id，Can 保存到 TxConfirmation（`SWS_Can_00276` p.45） | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| CAN_BUSY | — | `Can_Write` 的“硬件对象忙”返回值，不是错误；CanIf 缓冲重发 | [04-can-mcal/10](../04-can-mcal/10-can-write-implementation.md) |
| 控制器状态 | `Can_ControllerStateType` | UNINIT / STARTED / STOPPED / SLEEP | [04-can-mcal/06](../04-can-mcal/06-can-controller-init.md) |
| CanRxProcessing / CanTxProcessing | — | INTERRUPT / POLLING / MIXED，决定回调在 ISR 还是 MainFunction | [04-can-mcal/12](../04-can-mcal/12-can-interrupt-implementation.md) |
| CanIf | CAN Interface | L-PDU 路由、软件过滤、Tx 缓冲、控制器模式 | [05-can-stack/01](../05-can-stack/01-canif.md) |
| 软件过滤 | Software filtering | CanIf 用 (HRH, CAN ID) 查 Rx L-PDU | [05-can-stack/02](../05-can-stack/02-canif-configuration.md) |
| PduMode | — | CanIf 的 PDU 通道模式（ONLINE / OFFLINE…） | [05-can-stack/01](../05-can-stack/01-canif.md) |
| CanTp | CAN Transport Layer | ISO-TP 实现，N-PDU ↔ N-SDU | [05-can-stack/03](../05-can-stack/03-cantp.md) |
| PduR | PDU Router | 按路由表在 CanTp 与 Dcm（等）之间转发 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| 路由路径 | Routing path (PduRSrcPdu → PduRDestPdu) | PduR 的配置单元 | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| Zero-cost operation | — | PduR 被宏替换为直接调用；openAUTOSAR 中宏无条件生效是 bug | [05-can-stack/05](../05-can-stack/05-pdur.md) |
| StartOfReception / CopyRxData / TpRxIndication | — | R4.x TP 接收三段式 API | [05-can-stack/06](../05-can-stack/06-can-rx-path.md) |
| CopyTxData / TpTxConfirmation | — | R4.x TP 发送数据拉取与完成通知 | [05-can-stack/07](../05-can-stack/07-can-tx-path.md) |
| ProvideRxBuffer / ProvideTxBuffer | — | R3.x 的 TP API（openAUTOSAR 使用），上层“借出整块 buffer” | [06-dcm/14](../06-dcm/14-dcm-upgrade-guide.md) |
| CanSM | CAN State Manager | 控制器启动与 bus-off 恢复策略 | [04-can-mcal/13](../04-can-mcal/13-can-error-busoff.md) |
| ComM | Communication Manager | 通信模式请求；DCM 通过 `ComM_DCM_ActiveDiagnostic` 交互 | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| Com | Communication module | 信号 ↔ I-PDU 打包（非本教程诊断主线） | [05-can-stack/05](../05-can-stack/05-pdur.md) |

## 5. 诊断（UDS / DCM / DEM）

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| UDS | Unified Diagnostic Services (ISO 14229-1) | 应用层诊断协议 | [06-dcm/10](../06-dcm/10-uds-services.md) |
| SID | Service Identifier | 请求首字节；正响应 = SID + 0x40 | [06-dcm/10](../06-dcm/10-uds-services.md) |
| 子功能 | Sub-function | 部分服务的第二字节（低 7 位） | [06-dcm/03](../06-dcm/03-dsd.md) |
| SPRMIB | Suppress Positive Response Message Indication Bit | 子功能 bit7，=1 时不发正响应 | [06-dcm/03](../06-dcm/03-dsd.md) |
| NRC | Negative Response Code | 负响应 `7F SID NRC` | [06-dcm/10](../06-dcm/10-uds-services.md) |
| NRC 0x78 | requestCorrectlyReceived-ResponsePending | 服务需要更多时间，延长到 P2\* | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| DID | Data Identifier | 0x22/0x2E 访问的数据标识，如 F190（VIN） | [06-dcm/08](../06-dcm/08-did.md) |
| RID | Routine Identifier | 0x31 的例程标识 | [06-dcm/10](../06-dcm/10-uds-services.md) |
| DTC | Diagnostic Trouble Code | 故障码，由 Dem 存储 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| DTC 状态字节 | DTC status byte | ISO 14229-1 的 8 个状态位 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| 诊断会话 | Diagnostic session | Default 0x01 / Programming 0x02 / Extended 0x03 | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| 安全访问 | Security Access (0x27) | seed/key 解锁安全级 | [06-dcm/07](../06-dcm/07-security-access.md) |
| Seed / Key | — | ECU 给出的随机数 / 测试仪计算的密钥 | [06-dcm/07](../06-dcm/07-security-access.md) |
| P2 / P2\* | P2Server / P2\*Server | 服务器响应时限 / 发 0x78 后的延长时限 | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| S3 | S3Server | 非默认会话保持时间，固定 5 s（`SWS_Dcm_00143` p.79） | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| TesterPresent | 0x3E | 保持会话；功能寻址 `3E 80` 由 DSL 旁路 | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| ECUReset | 0x11 | 正响应发出后由 BswM/EcuM 执行复位 | [06-dcm/10](../06-dcm/10-uds-services.md) |
| ReadDataByIdentifier | 0x22 | 按 DID 读数据 | [06-dcm/08](../06-dcm/08-did.md) |
| WriteDataByIdentifier | 0x2E | 按 DID 写数据（常经 NvM） | [06-dcm/08](../06-dcm/08-did.md) |
| ReadDTCInformation / ClearDiagnosticInformation | 0x19 / 0x14 | 读 / 清 DTC | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| RoutineControl | 0x31 | Start / Stop / RequestResults | [06-dcm/10](../06-dcm/10-uds-services.md) |
| DCM | Diagnostic Communication Manager | Classic 诊断协议处理模块（DCM R20-11） | [06-dcm/01](../06-dcm/01-dcm-overview.md) |
| DSL | Diagnostic Session Layer | DCM 子模块：缓冲、时序（P2/S3）、会话/安全状态、0x78 | [06-dcm/02](../06-dcm/02-dsl.md) |
| DSD | Diagnostic Service Dispatcher | 校验 SID/会话/安全/长度并分发到 DSP（`SWS_Dcm_00221` p.100） | [06-dcm/03](../06-dcm/03-dsd.md) |
| DSP | Diagnostic Service Processing | 各服务处理函数，调用 SW-C / Dem / NvM | [06-dcm/04](../06-dcm/04-dsp.md) |
| OpStatus | `Dcm_OpStatusType` | DCM_INITIAL / PENDING / CANCEL / FORCE_RCRRP_OK（`SWS_Dcm_00984` p.301） | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| DCM_E_PENDING | — | 应用返回“尚未完成”，DCM 下周期以 DCM_PENDING 重调 | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| DCM_E_FORCE_RCRRP | — | 应用要求 DCM 立即发 0x78 | [06-dcm/11](../06-dcm/11-dcm-runtime-flow.md) |
| DataServices_\<Data\> | — | DCM 的 C/S 端口接口（ReadData / WriteData…，`SWS_Dcm_00686` p.341） | [07-rte-swc/08](../07-rte-swc/08-dcm-rte-integration.md) |
| DcmDspDataUsePort | — | 决定 DID 数据如何访问（SYNCH/ASYNCH C/S、FNC、S/R、BLOCK_ID…） | [reference/dcm-configuration-map](dcm-configuration-map.md) |
| DcmTaskTime | — | `Dcm_MainFunction` 周期（ECUC_Dcm_00820 p.678） | [06-dcm/12](../06-dcm/12-dcm-mainfunction.md) |
| 服务表 | Service table (DcmDsdServiceTable) | SID → 授权 → 处理函数 | [06-dcm/05](../06-dcm/05-dcm-configuration.md) |
| Mode Declaration Group | — | 如 `DcmDiagnosticSessionControl`、`DcmEcuReset`；DCM 是 mode manager | [06-dcm/06](../06-dcm/06-diagnostic-session.md) |
| Mode Rule | — | DCM 中基于模式条件的服务许可规则 | [06-dcm/03](../06-dcm/03-dsd.md) |
| DEM | Diagnostic Event Manager | 事件/DTC 存储与状态管理（本仓库无 SWS） | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| ClientId | `DcmDemClientRef` | DCM 调用 Dem API 时的客户端号（R4.3.0 起） | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| Freeze Frame / Extended Data | Snapshot / Extended data record | DTC 附带数据 | [06-dcm/09](../06-dcm/09-dtc-dem.md) |
| VIN | Vehicle Identification Number | DID F190，17 字节 | [08-integration/04](../08-integration/04-f190-vin-demo.md) |
| CDD / ODX | CANdela Diagnostic Description / Open Diagnostic data eXchange | 诊断描述文件，常被导入为 Dcm/Dem 配置 | [09-real-project-preparation/03](../09-real-project-preparation/03-how-to-read-dcm.md) |
| CANoe | — | 常用的测试仪 / 总线仿真工具 | [08-integration/06](../08-integration/06-canoe-test.md) |

## 6. 系统服务、调度与存储

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| EcuM | ECU State Manager | 启动、DriverInit 序列、关机 | [02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md) |
| BswM | BSW Mode Manager | 规则 + 动作列表，处理模式切换（如复位） | [02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md) |
| DriverInitZero/One/Two/Three | — | EcuM 分阶段初始化 callout | [02-autosar-classic/03](../02-autosar-classic/03-ecu-startup.md) |
| OS | AUTOSAR OS (OSEK-based) | 任务、ISR、Alarm、Counter、Schedule Table | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| Task | — | OS 调度单元（Basic / Extended） | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| Cat 1 / Cat 2 ISR | Category 1 / 2 ISR | Cat 2 可调用 OS 服务、由 OS 包装 | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| Counter / Alarm / Schedule Table | — | OS 时基与周期激活机制 | [02-autosar-classic/06](../02-autosar-classic/06-os-task-isr.md) |
| SchM | BSW Scheduler | 调度 BSW MainFunction，提供临界区与模式切换 | [02-autosar-classic/07](../02-autosar-classic/07-mainfunction-scheduling.md) |
| MainFunction | `<Mod>_MainFunction` | BSW 周期函数，无参数、不可重入 | [02-autosar-classic/07](../02-autosar-classic/07-mainfunction-scheduling.md) |
| Exclusive Area | — | `SchM_Enter/Exit_<Mod>_<EA>` 保护的临界区 | [02-autosar-classic/07](../02-autosar-classic/07-mainfunction-scheduling.md) |
| NvM | NVRAM Manager | 非易失数据块管理，异步作业 | [07-rte-swc/09](../07-rte-swc/09-diagnostic-swc-example.md) |
| Fee / Fls | Flash EEPROM Emulation / Flash Driver | NvM 的下层，落到 Data Flash | [01-rh850/03](../01-rh850/03-memory-map.md) |
| Mcu Driver | — | 时钟、RAM 初始化、复位、低功耗（MCU R24-11） | [03-mcal/02](../03-mcal/02-mcu-driver.md) |
| Port / Dio / Gpt / Icu | — | 引脚复用 / 数字 I/O / 通用定时器 / 输入捕获驱动 | [03-mcal/03](../03-mcal/03-port-driver.md)、[04](../03-mcal/04-dio-driver.md)、[05](../03-mcal/05-gpt-driver.md)、[06](../03-mcal/06-icu-driver.md) |
| IoHwAb | I/O Hardware Abstraction | 把 MCAL 访问封装成 ECU signal（IoHwAb R24-11） | [02-autosar-classic/02](../02-autosar-classic/02-layered-architecture.md) |

## 7. 工具链与工程环境

| 术语 | 英文 / 全称 | 说明 | 章节 |
|---|---|---|---|
| openAUTOSAR | Arctic Core 2.18.0 fork | 本教程的 R3.1.5 风格参考实现，能编译不能运行 | [reference/source-traceability](source-traceability.md) |
| RTA-CAR / RTA-OS | ETAS 工具链 | 截图中“可能的真实环境”，本仓库无法验证 | [09-real-project-preparation/07](../09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md) |
| GHS | Green Hills compiler (`ccrh850`) | 截图中的编译器；本仓库未安装 | [01-rh850/05](../01-rh850/05-linker-script.md) |
| Host test | 主机测试 | 用 PC gcc 编译教学代码并运行测试（`tools/run_uds_demo.py`、`tools/run_host_tests.py`） | [08-integration/05](../08-integration/05-uds-end-to-end.md) |
| `[Educational Implementation]` 等标签 | — | 本教程用于区分规范、硬件事实、教学实现、概念示意、真实项目注意事项的标记 | [00-learning-roadmap](../00-learning-roadmap.md) |
