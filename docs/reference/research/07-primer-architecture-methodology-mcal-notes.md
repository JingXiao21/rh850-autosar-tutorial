# 07 — Part XI「Classic AUTOSAR 入门：原理与工作流」Phase 1 事实笔记

> 用途：为新增的 Part XI（面向初学者的 Classic AUTOSAR 原理 + 工作流）提供可核对的事实底稿。
> 范围：分层架构、VFB、Methodology、ECUC、SWC Template、BSWGeneral/MemMap、MCAL、IoHwAb、"signal 的一生"通信路径。RTE 内部细节由另一份笔记负责，这里只写 SWC↔RTE 的高层关系。

## 0. 阅读约定（必读）

- **版本**：除非特别说明，所有引用均为 **R25-11**（`artifacts/pdf-text/autosar-cp-R25-11/`，原 PDF 在 `specs/autosar-cp-R25-11/`）。`00-writing-conventions.md` 中"仓库缺少 CanIf/PduR/Com/EcuM/Rte 等 SWS"的说法已过时，这些文件现在都在 R25-11 目录里。真实项目（RTA-CAR 12.9.0）所用 release 以项目为准，这里的函数名/文件名需在项目 release 上复核（见第 11 节"版本差异提醒"）。
- **页码** = 文本文件中 `=== PDF PAGE n ===` 的 **PDF 页码**。`EXP_LayeredSoftwareArchitecture` 是幻灯片式文档，PDF 页脚印的 slide 编号 = PDF 页码 + 10（例：PDF p.20 页脚写 30），本文一律用 PDF 页码。
- **文档简称**：
  `EXP` = EXP_LayeredSoftwareArchitecture；`VFB` = TR_VFB；`METH` = TR_Methodology；`ECUC` = TPS_ECUConfiguration；`SWCT` = TPS_SoftwareComponentTemplate；`SWMG` = TR_SWCModelingGuide；`BSWG` = SWS_BSWGeneral；`MEMMAP` = SWS_MemoryMapping；`MCU/PORT/DIO/ADC` = 对应 Driver SWS；`IOHWAB` = SWS_IOHardwareAbstraction；`COM`、`PDUR`、`CANIF` = 对应 SWS；补充引用 `ECUM` = SWS_ECUStateManager、`DET` = SWS_DefaultErrorTracer、`RTE` = SWS_RTE（这三份文本在同一目录，虽不在任务清单内，但用于核对"谁调用 Init"和 RTE↔Com 映射）。
- **标注**：「**[规范]**」= 文档明文陈述；「**[业界实践]**」= 行业常见做法，规范未规定，不得当作 AUTOSAR 要求写进教程；「**[推断]**」= 由多处文字推出，非原文。
- 不复制原文，均为概括；ID 形如 `SWS_BSW_00150`、`TR_METH_01047`、`TPS_SWCT_01108`、`TPS_ECUC_02005`。

---

## 1. 分层架构（EXP）

### 1.1 文档定位

- EXP 自述：**"does not contain requirements and is informative only"**，只给静态概念视图；真正的接口/动态行为在各 BSW 模块 SWS 中（EXP p.10）。所以教程里写"某层必须/不得"时，凡出自 EXP 的应标"EXP 描述性规则（matrix 标 normative 的除外，见 1.3）"。
- 适用对象：AUTOSAR 意义上的 **ECU = 一个 microcontroller + 外设 + 对应软件/配置**；一个壳体内有多颗 MCU，每颗都是独立的 AUTOSAR-ECU 实例（EXP p.11）。
- 可扩展性：标准模块可扩功能（配置仍要纳入自动配置流程）；非标准模块以 **Complex Driver** 形式接入；**不可再增加新的层**（EXP p.11）。

### 1.2 层与功能组

三层顶视图：**Application Layer / Runtime Environment (RTE) / Basic Software (BSW)**，下面是 Microcontroller（EXP p.12）。BSW 再分 **Services Layer / ECU Abstraction Layer / Microcontroller Abstraction Layer（MCAL）/ Complex Drivers**（EXP p.13）。

| 层 | 职责与属性（EXP 页） |
|---|---|
| **MCAL** | 最底层 BSW，含"内部驱动"（直接访问 µC 及片内外设）；目的：让上层独立于 µC。实现 µC 相关，**向上接口标准化且 µC 无关**（p.15）。内部设备=片内 EEPROM/CAN controller/ADC 等（p.21）。 |
| **ECU Abstraction** | 接 MCAL 驱动 + 含**外部设备**驱动（外部 EEPROM/watchdog/flash、SBC 内 transceiver 等，p.22）；让上层独立于 ECU 硬件布局。实现 µC 无关、ECU 硬件相关；上层接口 µC 和 ECU 硬件无关（p.16）。例外：**memory-mapped 的外部 flash** 驱动放 MCAL（因 µC 相关，p.22）。 |
| **Services** | BSW 最高层，提供 OS 功能、车载网络通信与管理、NVRAM 管理、诊断（含 UDS、错误存储）、ECU 状态/模式管理、WdgM 等（p.18）。实现大多 µC/ECU 无关。 |
| **Complex Drivers (CDD)** | 横跨硬件到 RTE（p.17）。 |
| **RTE** | 向应用 SWC 提供通信服务；**RTE 之上架构风格从"layered"变为"component style"**；实现与 ECU、应用相关（**每个 ECU 单独生成**）；上接口完全 ECU 无关（p.19）。 |

**功能组（functional groups）**：BSW 服务按类型分 I/O、Memory、Crypto、Communication、Off-board Communication、System（p.20）。详图（p.14、p.29）按组横切：每组（Memory/Crypto/Wireless/Comm/I-O）都各自有 `... Drivers`（MCAL）→ `... Hardware Abstraction`（ECU Abstraction）→ `... Services`（Services）。MCAL 内模块组（p.29，文字）：Microcontroller Drivers（Watchdog、GPT、MCU、Core/RAM/Flash Test 等）、Communication Drivers（SPI、I2C、LIN、CAN、FlexRay、Ethernet）、Memory Drivers（片内 flash/EEPROM，及 memory-mapped 外部 flash）、I/O Drivers（ADC、DIO、PORT、PWM、ICU、OCU）、Crypto Drivers、Wireless Communication Drivers。

**ECU Abstraction 子组**：I/O Hardware Abstraction（抽象 I/O 位置与 pin 连接、电平反相等，**不抽象传感器/执行器本身**，p.33）、Communication Hardware Abstraction（如 CanIf，p.34；R25-11 图中其上还有 L-SDU Router）、Memory Hardware Abstraction（p.35）、Onboard Device Abstraction（外部 watchdog 等，p.36）、Crypto Hardware Abstraction（p.37）。

**模块类型术语**（p.21–25）：Driver（internal/external）、Interface（不改数据内容，通常在 ECU Abstraction，如 CanIf，p.23）、Handler（并发/多客户端访问仲裁：缓冲、排队、复用，常并入 driver，例 SPIHandlerDriver，p.24、p.31）、Manager（可评估/改变数据内容，通常在 Services，例 NvM，p.25）。

**Libraries**（p.26–27）：可被 BSW、SWC、库、集成代码调用；在调用者上下文执行；**只能调库，不可调 BSW 模块**；可重入、无内部状态、无需 init、同步无等待点；配置不鼓励。BSWG 对应 `SWS_BSW_00259/00260/00261`（p.54）。

**Stack**（通信栈为主）：CAN 栈 = Com / PduR / (L-SDU Router) / CanIf / CanTrcv / Can driver，旁边有 CanSM、CanNM、CanTp、ComM、Generic NM Interface 等；COM、Generic NM、DCM 是 per ECU 一份，CanNM/CanSM 按 CAN channel 实例化（p.39–41）。Ethernet、CAN XL、J1939 为扩展（p.42–45）。

### 1.3 层间调用规则

- **General Interfacing Rules**（p.79）：
  - 水平接口：Services 层、ECU Abstraction 层**允许**；**MCAL 层不允许**（例外：为性能而配置的 notification）。
  - 垂直接口：一层可访问其下一层的所有接口；**绕过一层应避免，绕过两层或以上不允许；绕过 MCAL 不允许**；模块可访问另一层组中较低层模块（例：外部 HW 用的 SPI）；**所有层都可与 System Services 交互**。
- **Layer Interaction Matrix**（p.80）：页面自称 **normative**。行读法："Microcontroller Drivers / Memory Drivers / I/O Drivers 只可用 System Services 与 Hardware，不用其他 BSW 层"；Crypto Drivers 另可用 Memory Services 一项（R20-11 改动，p.3）；Communication Drivers 另可用 hardware 侧项。矩阵中 SW Components/RTE 一行不含 MCAL 列——即 **SWC 不直接调 MCAL**（[推断]，矩阵文本对列对齐不稳，教程写此结论时同时引 p.79 "绕过 MCAL 不允许" 与 VFB p.86）。
- **SWC 与硬件**：VFB 章节明确"对硬件的访问经 MCAL，避免高层直接访问 µC 寄存器"（VFB p.86）。
- **Complex Driver 规则**（p.81–83）：
  - 层内模块调 CDD：仅当 CDD 提供可被调用方"通用配置"的接口（典型：PduR 配置里把 CDD 当作新总线的 interface 模块）。
  - CDD 调标准模块：仅当目标模块可重入、callback 名可配置、且其上无做状态管理的上层模块。可访问：SPI、GPT、I/O drivers（受通道/组并发限制）、NvM（作为 memory stack 唯一入口）、WdgM、PduR、总线 Interface、NM Interface、ComM/BswM、Det/Dem/Dlt、OS（仅限不与分层模块冲突的对象）。
  - `init` 函数通常不可重入，**只应由 EcuM 调用**（p.82）。
  - 多核：CDD 访问 BSW 标准接口须与 BSW 同核，否则用 satellite 或自带 stub + IOC（p.83）。

### 1.4 CDD 是什么

- 实现**非标准化**功能的模块；典型：复杂传感/执行（喷油控制、电子阀控制、增量位置检测）、直接使用特定中断/复杂 µC 外设（TPU、PCP、CCU）（EXP p.32）。
- 三类动机（EXP p.17；VFB p.88）：**AUTOSAR 未规定的设备**、**高实时约束**、**迁移**（旧应用可先以 CDD 形式存在）。VFB 还提到新总线驱动可放 CDD，通信服务调用 CDD 而非 Communication HW Abstraction（VFB p.88）。
- 属性（EXP p.17、p.32）：实现可依赖应用、µC、ECU；**对 SWC 的上接口按 AUTOSAR interface 规范**；下接口受限。
- SWC Template 里的表示：`ComplexDeviceDriverSwComponentType`（`TPS_SWCT_01393/01394/01395`，SWCT p.656）；是 SWC 与 BSW 的"混合体"，BSW 部分由 BSW Module Description 描述（`TPS_SWCT_01396`，p.656）。ECUC 里 CDD 的配置类/变体由实现者在 VSMD 里自定（`TPS_ECUC_02139`、`TPS_ECUC_02144`，ECUC p.273）。

### 1.5 接口类型

（EXP p.77–78）

| 术语 | 含义 |
|---|---|
| **AUTOSAR Interface** | 语言/ECU/网络无关的"软件组件和/或 BSW 模块间信息交换"描述；用于定义 port；可本地也可经网络实现。 |
| **Standardized AUTOSAR Interface** | 语法语义由 AUTOSAR 标准化的 AUTOSAR Interface；典型用于定义 **AUTOSAR Services**（BSW 提供给应用 SWC 的标准服务）。 |
| **Standardized Interface** | 不用 AUTOSAR Interface 技术、直接标准化的 API（通常 C 语言）；用于**同一 ECU 内**模块间；**无法再路由经过网络**。 |

### 1.6 BSW 调度（main function）

- BSW Scheduler（SchM）与 RTE **一起生成**，使同一 OS Task 可调度 BSW main functions 与 SWC runnables、可交错执行、可协调 mode 切换、可用同一个 ExternalTriggerOccurredEvent 同步触发（EXP p.133）。
- 原则（p.134）：BSW 调度集中于 BSW Scheduler，由 ECU/BSW integrator 配置、由 RTE generator 与 RTE 一起生成；**只有 BSW Scheduler 与 RTE 可使用 OS 对象/服务**（例外：EcuM、CDD，OS 的 GetCounterValue/GetElapsedCounterValue，MCAL 可开关中断）。对应 BSWG `SWS_BSW_00257` 的 OS 服务使用表（BSWG p.47）。
- 调度对象（p.135）：main function（每模块 n 个，各层都有）；触发事件：BswTimingEvent、BswBackgroundEvent、BswModeSwitchEvent、BswModeSwitchedAckEvent、BswInternalTriggerOccurredEvent、BswExternalTriggerOccurredEvent、BswOperationInvokedEvent；main function 可按 mode 禁用。
- 逻辑→技术：把 scheduling objects 映射到 OS Tasks，并指定任务内顺序、策略（p.136）；Runnable 与 BSW main 的任务映射关系见 p.125。
- BSWG：main function 命名 `<Mip>_MainFunction[<Sd>]`（`SWS_BSW_00153`，BSWG p.79）、无参数无返回值（`00154`）、不得进入等待状态（`00156`）；**模块内禁止互相调用 main function**（`00133`，p.43）；未初始化时被调用须直接返回、不报错（`00037`，p.43）；这些原型不放在模块头文件，由 SchM 头 `SchM_<Mip>.h` 提供（`00210`，p.28；`00007`，p.22）。

### 1.7 EXP 里的"初始化概念"

EXP 本身**没有**完整的 EcuM 启动序列，只给出**配置数据指针**的概念（p.111–123）：

- 配置类：pre-compile / link-time / post-build（p.111）；变体 `VARIANT-PRE-COMPILE/LINK-TIME/POST-BUILD`（p.121）。
- link-time（p.115）：单配置集——直接访问外部常量；多配置集——Init 时传指针。
- post-build（p.118–123）：配置结构在可重刷的独立内存段；**模块在 init 时拿到基地址/指针**；举例 `Can_Init(&MySimpleCanConfig[0])` 由 EcuM 调用（p.120）；EcuM 持有索引表（`&xx_configuration` 等），可用 `selector` 在多套配置间选（p.122–123）。
- 完整启动序列应补引 ECUM（见 7.5）。

---

## 2. VFB 概念（VFB）

- **VFB** 是让 SWC 互相交互的**抽象通信机制**，与 ECU/网络无关；应用 = 互连组件的组合；虚拟连接随后映射为 ECU 内本地连接或网络（CAN/FlexRay 帧）；**SWC 与 SWC / SWC 与 BSW 之间的具体接口 = RTE**（VFB p.13）。SWC = implementation + 形式化 SWC description；应用与基础设施严格分离，以达 relocatability（p.14）。VFB 还承载：组件间通信、传感器/执行器通信、标准服务（NVRAM 等）、mode 变化响应、标定/测量（p.14）。
- **组件与端口**：组件通过 **port** 交互（`TR_VFB_00001/00002`），一个 port 属于唯一组件；组件类型可多次实例化（`TR_VFB_00084`，p.15–17）。每个 port 由**恰好一个** port-interface 类型化（`TR_VFB_00003`，p.17）。PPort/RPort/PRPort（p.19–20；PRPort 不支持 ParameterInterface，p.20）。
- **Port-interface 种类**（VFB Table 3.1，p.17–18）：Client-Server、Sender-Receiver（含 event、mode 通知）、Parameter Interface（const/fixed/calibration 数据）、Non-volatile Data Interface（元素级 NV 访问，对照 NV block 访问）、Trigger Interface（触发其他 SWC 执行，例曲轴/凸轮轴）、Mode Switch Interface（mode manager 通知 mode user）。
- **S/R 语义**（p.65–75）：数据元素必须声明 **queued** 或 **last-is-best**（`TR_VFB_00012`），last-is-best 可支持 invalidation；单个连接内 VFB 保证同一元素的变化顺序（`TR_VFB_00029`），但不保证不同元素/不同连接间顺序（p.75）。错误分 infrastructure error（如 timeout）与 application error（p.65）。
- **C/S 语义**（p.75–82）：静态 n:1（n 个 client，1 个 server）；VFB 注意区分 C/S 的"service"与"AUTOSAR service"（p.75 脚注）。
- **连接器**（p.40–42）：assembly connector 连接一个 PPort/PRPort 与一个 RPort/PRPort（`TR_VFB_00010`），且要兼容（`TR_VFB_00113`）；**未连接则不能通信**（`TR_VFB_00009`）——**例外：AUTOSAR Services 的连接在 ECU 配置阶段才建立**（p.41 脚注）。一个 PPort 可多播到多个 RPort；S/R 中多个 PPort 可汇聚到一个 RPort。未连接 RPort（S/R）提供初值并报告"未连接"（p.42）。delegation / pass-through connector 用于 composition（SWCT Table/§6.4）。
- **Composition vs atomic**（p.42–43）：composition 本身也是 component type，可嵌套，composition 内的使用称 prototype；**atomic** 不可再分，必须整体映射到单一 ECU。composition 不能有"服务端口"，服务端口在 ECU 配置阶段加到打平后的原子 SWC 上（p.43）。
- **VFB → ECU/RTE**（p.44–45）：把原子 SWC 部署到 ECU 上；连接器实现为 intra-ECU 或 inter-ECU 机制。**每个 ECU 的 RTE 单独生成**，实现本地连接，并把远端组件的 port 路由到下层通信栈；同时把 SWC 接到本地标准服务（nv、ecuMode）。
- **runnable 概念**（p.57–59）：SWC 实现由 **runnable entities** 组成，由 RTE 启动，运行在 **task** 上下文；SWC 的"描述"向 RTE 声明：哪些 runnable 周期调用、哪些响应事件、如何访问 port 数据、需要什么资源；RTE（连同正确配置的 BSW）满足这些要求（p.59）。VFB 脚注：优化时同 ECU 同任务的同步调用可退化为直接函数调用（p.57）。
- **硬件交互分层**（p.85–88）：MCAL（避免直接访问 µC 寄存器，p.86）→ ECU Abstraction（给 SW 一个电气量接口，p.87）→ Sensor-Actuator SWC（针对具体传感器/执行器，使用 ECU abstraction 接口，p.87–88）→ Application SWC。理由：更换 µC 只需换 MCAL/ECU Abstraction；换 ECU 可复用 SA-SWC + 应用；换传感器可复用应用；各层可由不同专家/公司开发（p.85）。
- **SWC 种类（VFB 3.7）**：Application（`TR_VFB_00209`）、Sensor-actuator（`00121`）、Parameter（`00122`）、Composition（`00123`）、Service Proxy（`00124`，负责 mode 在系统内分发）、Service（`00125`，经标准化接口提供标准服务）、ECU-abstraction（`00126`）、Complex driver（`00127`，泛化 ECU-abstraction，可直接和 BSW 交互）（VFB p.47–54）。
- **AUTOSAR Services 表示**（p.89–91）：Services 是"BSW 模块 + SWC"的混合概念；VFB 上只在请求服务的 SWC 处可见；常用 provide port + C/S（像库调用），也可 S/R；SWC 用 `isService`/`ServiceNeeds` 属性标注（p.90）；**服务实现必须与使用它的 SWC 位于同一 ECU**（p.90–91）。

---

## 3. Methodology（METH）

### 3.1 文档里的 "Role"（**原文角色名，不含 OEM/Tier1 等组织名**）

METH 的"角色"是任务执行者（SPEM Role Definition），**不是公司类型**；一人可兼多个 role，一个 role 可由多人担任（`TR_METH_01023/01024`，METH p.30）。R25-11 §3.1.4（p.182–189）列出的 role：

| Role（原文） | 一句话职责 | 页 |
|---|---|---|
| **System Engineer** | 系统创建/管理/开发/集成；做 System Description 相关任务：Define System Topology、Define Communication Matrix、Define Signal PDUs/Frames/Network Management/Gateway、Deploy Software Component、Derive Communication Needs、Extract the ECU Communication、Flatten Software Composition 等 | p.187–188 |
| **Software Component Designer** | 设计 SWC 与 VFB 系统：Define VFB Application Software Component/Composition/Interfaces/Types/Modes、Define Atomic SWC Internal Behavior、Define Complex Driver Component、Define ECU Abstraction、Define VFB Sensor or Actuator Component、Map Software Component to BSW、Define VFB Timing/Variants | p.185–186 |
| **Software Component Developer** | 写 SWC 代码：Generate Atomic SWC **Contract** Header Files、Implement Atomic SWC、Compile Atomic SWC、Generate Component Prebuild Data Set、Measure Component Resources、Define SW Component Timing | p.186–187 |
| **Basic Software Designer** | 总体设计 BSW，**负责模块间接口/数据类型一致性**：Define BSW Behavior/Entries/Interfaces/Types、Create Transformer Specification | p.182 |
| **Basic Software Module Developer** | 开发并交付一个 BSW 模块：Implement a BSW Module、Compile BSW Core Code、Generate BSW Module Prebuild Data Set、Define BSW Module Timing、Create Library、Generate BSWM Contract Header Files、Define Memory Addressing Modes | p.183 |
| **ECU Integrator** | **在 ECU 上集成全部软件**：生成必要代码、完成所有 SWC 与 BSW 模块配置。含 Configure ECUC/OS/RTE/Com/Diagnostics/NvM/Watchdog Manager/Mode Management/**IO Hardware abstraction/MCAL**、Create/Connect Service Component、Generate BSW Configuration Code、**Generate RTE / RTE Prebuild/Postbuild Dataset / Scheduler / OS**、Generate BSW/SWC Memory Mapping Header、Compile ECU Source Code、Generate ECU Executable、Extract ECU System/Topology/Communication 等 | p.184–185 |
| Calibration Engineer / Safety Engineer / Rapid Prototyping Engineer / Non-AUTOSAR System Integrator / Certification Agency / AUTOSAR Partnership | 标定、功能安全、快速原型、非 AUTOSAR 系统接入、一致性认证、标准产物 | p.182–189 |

> 注意：**"Configure MCAL"、"Configure IO Hardware abstraction" 在 METH 中是 ECU Integrator 的任务**（p.184），而不是单独的 "MCAL Developer" role。

**METH 里实际出现的组织名**（仅此几处，是写"谁做什么"时能引的全部明文）：
- 两阶段开发：**primary organization（usually OEM）** 定义整体系统（System Extract 交付），**多个 other organizations（usually suppliers）** 并行定义子系统（`TR_METH_01047`，p.40–41）；接收方可把结构转换成自己的 ECU System Description，System Extract 相当于"需求"，子系统是"解决方案"（`TR_METH_01049`，p.41）。
- 序列化 use case：**OEM 定义 ISignal 级网络表示**，由 Serializer Transformer 生成字节流；**未由 OEM 提供时 Tier1 可自由选择应用 SW 的 implementation data types**（`TR_METH_01156`，p.71）；或 OEM 为 root software composition 定义相同的 implementation data types，Tier1 在 root composition 内部可任意（`TR_METH_01157`，p.72）。
- 诊断 use case：**"Obviously, the OEM acts as a diagnostic requester and the ECU supplier as the diagnostic integrator"**，但如自研应用 SWC，OEM 也可兼任 integrator（`TR_METH_01139`，p.137）；DTC 由 OEM 定义、SWC 由其他 supplier 实现时，integrator 需把二者映射到一起（p.138）。
- **METH 没有 "BSW vendor" / "tool vendor" 这种 role 名**。涉及工具的是 **Tool Definition**：Compiler、Linker（p.190）、**Component API Generator Tool**（p.332）、**RTE Generator**（用于 Generate RTE / RTE Prebuild/Postbuild Dataset / Generate Scheduler，p.414）、**BSW Generator Framework**（使用随各模块交付的 BSW generators，p.414）。

### 3.2 产物（work products / deliverables）与活动流

**整体视图**（METH §2.1，p.38–45）：
- 三种系统视图：abstract system、overall technical system、subsystem（`TR_METH_01041–01043`，p.38–39）；VFB 是 ECU/网络无关的抽象视图（`TR_METH_01039`）。
- 活动顶层（Methodology Overview，p.43–45）：**Develop an Abstract System Description**（可选，`01044`）→ **Develop a VFB System Description**（`01045`）→ **Develop System**（`01046`，定义 ECU/网络拓扑、部署 SWC、导出通信矩阵）→ **Develop Sub-System** → 并行 **Develop Application Software** / **Develop Basic Software** → **Integrate Software for ECU**。
- 并行开发：SWC 按 abstract VFB/VFB/子系统 VFB 的定义实现，"SWC 的实现在很大程度上独立于 ECU 配置——这是 AUTOSAR 方法学的关键特性"（`TR_METH_01110`，p.41–42）；BSW 模块独立于 VFB，可在 ECU 集成前任何时间开发（`TR_METH_01111`，p.42）；**EcuInstance 集成在 "BSW Module Delivered Bundles + ECU Extract + 所有 Delivered Atomic Software Components" 齐备后开始**：配置→生成 RTE→编译链接（`TR_METH_01112`，p.42）。

**关键产物**（原文名）：

| 产物 | 说明 | 页 |
|---|---|---|
| **System Description**（generic deliverable，扩展为 Abstract System Description / System Constraint Description / **System Configuration Description** / **System Extract**） | 系统级描述；注意 METH 的 "System Description" 是泛称，不等同 System Template 里 category=SYSTEM_DESCRIPTION 的那种 | p.258–259 |
| **System Configuration Description** 内容 | ECU 清单、通信系统及配置、通信矩阵、SWC 的 port/interface/connection（引用 SWC Description）、SWC→ECU 映射 | p.95 |
| **ECU Extract** | 与 System Configuration Description 同格式，**只含单个 ECU 相关元素**；完全分解、仅含原子 SWC；**ECU 配置的基础**（`TR_METH_01109`，p.41；p.95） | p.41、p.83、p.95 |
| **SWC description**（含 SwcInternalBehavior 与 Implementation） / **Delivered Atomic Software Components** | SWC 向 RTE 声明的需求 + 实现；SWC 实现完成后测资源并存入描述供下游使用 | p.56–58、p.97 |
| **BSW Module Description（BSWMD）/ BSW Module Delivered Bundle** | BSWMD 按 BSW Module Description Template；**Delivered Bundle** 含 Basic Software Module Implementation Description、Internal Behavior、Generator、BSW Build Action Manifest、Preconfigured/Recommended Configuration 等（SWS `SWS_BSW_00001`：每个 BSW 模块须提供 `.arxml` 形式的 BSW Module description，BSWG p.19） | METH p.348；p.94 |
| **ECU Configuration Values（ECUC）** | **包含单个 ECU 全部 BSW 模块配置的单一格式**，每个模块 generator 从中取自己所需子集（`TR_METH_01116`，p.96）；configuration editors 与 generators 都读写它（p.95） | p.94–96 |
| **BSW Module Vendor-Specific Configuration Parameter Definition**（VSMD） | 定义所有可能的配置参数及结构；纳入 ECUC 因其结构不固定（`TR_METH_01088`，p.98） | p.98 |
| **ECU Software Delivered / ECU Executable** | Integrate Software for ECU 的输出 | p.97、p.101 |

**ECU 集成活动流**（METH §2.7）：
- 主活动四个（+1 可选）：**Prepare ECU Configuration → Configure BSW and RTE → Generate BSW and RTE → Build Executable**，另可选 **Model ECU Timing**（`TR_METH_01087`，p.94）。
- **ECU 配置有两路输入**：System Configuration（跨 ECU 必须一致的配置 → ECU Extract）与 BSW 模块描述（`TR_METH_01114`，p.94）。
- **Prepare**（`01088`、`01117`，p.98）：在 ECU Extract 上叠加 SWC/BSW 的 **Service Needs**、BSW 的 Preconfigured/Recommended Configuration，选定每个模块的实现（引用 delivered BSWMD），产出 **base ECU Configuration**。
- **Configure BSW and RTE**（`01089`，`01090`，p.99–101）：主要为（工具辅助的）编辑；**方法学不规定配置顺序**；Configure RTE 还需要该 ECU 所有 Atomic SWC Implementations，SWC 变动要重做；任务列表见 p.100–101（Configure Com/Diagnostics/ECUC/IO Hardware abstraction/**MCAL**/Memmap Allocation/Mode Management/NvM/OS/RTE/Transformer/Watchdog Manager、Create/Connect Service Component）；Diagnostics 预期先于 NvM（p.101）。
- **Generate BSW and RTE**（`TR_METH_01092`，p.103–104、107）：generator 从 ECU Configuration Values 读取参数，生成 BSW Module Configuration Data Source Code / Header、**RTE Source Code**、OS 配置、BSW/SWC Memory Mapping Header 等；生成器应做完整性/一致性检查；**抽象参数被翻译为与模块实现相关的硬件/实现特定数据结构**（p.104）。具体生成方式取决于配置类和实现者选择。
- **Build Executable**（`TR_METH_01093`，p.107）：生成完毕后，所有源码与应用、库、目标码一起**编译并链接**为 ECU Executable；同时 Generate A2L。

**RTE 的 contract phase vs generation phase**（METH 提及，细节归 RTE 笔记）：
- **contract phase**（SWC 侧，**Software Component Developer** 执行）：任务 *Generate Atomic Software Component Contract Header Files*，产出 **Application Header File** 与 **Software Component Data Types Header**，"让 SWC 之后能与 RTE 链接"；SWC 实现可先于 ECU 配置完成（p.307–308、p.186）。
- **generation phase**（ECU 侧，**ECU Integrator** 执行）：任务 *Generate RTE* 产出 RTE Core Source Code、BSW Scheduler Code、RTE Implementation Description、测量/标定支持数据，并可选写入 ECU 配置（用于 OS 预配置）（p.386–387）；另有 *Generate RTE Prebuild/Postbuild Dataset*（p.184–185、p.414）。

**配置类在方法学里的流程**（METH §2.7.9，p.109–120）：
- 一个模块内参数的配置类可混合（`TR_METH_01115`，p.110）；参数的配置类由所选实现变体决定、实现后固定（p.109）。
- **Pre-compile**（`TR_METH_01095–01097`）：编译前选值；可只生成配置 header（核心源码不动）或同时生成 header+source（无核心代码）。
- **Link time**（`01098–01103`）：配置数据在独立源文件，单独编译后链接解析外部引用；值变化需重新生成/编译配置目标码；常用于仅交付目标码的模块。
- **Post-build**（`01104–01105`，p.116）：参数放在已知内存位置，独立文件下载到 ECU，**免重编重链**；产出两个可加载文件：ECU Executable（含应用、BSW、pre-compile & link-time 配置）与"仅含 post-build 配置的 BSW Module Configuration Data Loadable to ECU Memory"（p.101）；后续可经 *Update ECU Configuration*（`TR_METH_01151`）仅更新后者。

### 3.3 "谁通常做什么"

**[规范]**（上述 role/任务表）：
- ECU Integrator：完成 ECU 配置（含 MCAL、IoHwAb、OS、Com、RTE、Diagnostics、NvM…）、运行各 generator、编译链接（METH p.184–185）。
- Software Component Designer/Developer：设计 SWC、写 SWC 代码并在 contract phase 产出 contract header（p.185–187）。
- Basic Software Module Developer / Designer：开发并交付 BSW 模块（含 BSWMD、生成器、预配置）；Designer 管模块间接口一致性（p.182–183）。
- System Engineer：系统拓扑、通信矩阵、SWC 部署、ECU Extract（p.187–188）。
- 组织层面仅有：primary organization "usually OEM"、supplier "usually suppliers"（p.40）；OEM 与 Tier1 在数据类型/序列化（p.71–72）、OEM 与 ECU supplier 在诊断（p.137）。

**[业界实践]**（规范未规定，教程务必标注"典型做法，因项目而异"）：
- OEM 通常维护系统级：网络/通信矩阵（DBC/ARXML）、system description、system extract / ECU extract 交付给 supplier；OEM 也常自研部分应用或功能 SWC（尤其动力/底盘功能 owner、或集中式架构），再把 SWC（源码或目标码 + SWC description）交给 Tier1。
- **Tier1（ECU supplier）** 通常担任 METH 中的 **ECU Integrator**：购买/授权 BSW 栈（含 RTE/OS 生成器）与芯片厂 MCAL，配置、集成、编译、刷写，并自研底层 CDD 与大部分 ECU 专属应用。
- **BSW 供应商**（对应 METH 的 "Basic Software Module Developer" + 提供 BSW Generator）与 **MCAL 供应商（通常是芯片厂，如 Renesas）** 交付带 BSWMD/ECUC 定义/配置工具的软件包；**工具链厂商**提供 RTE Generator、BSW Generator Framework、配置编辑器（METH 以 Tool 概念描述它们，p.414）。
- 即：**OEM "开发 Classic AUTOSAR 软件吗？"** → 规范层面只说"OEM 通常是 primary organization 定义系统"，**未规定 OEM 是否写 SWC 或 BSW**；实践中 OEM 常写应用 SWC 与系统配置，很少写/配置 MCAL，视项目分工（见第 9 节）。

---

## 4. ECU Configuration（ECUC）

- **目标**（ECUC p.18、p.21）：定义**一种通用的配置描述语言**（container + parameter），分两部分：
  - **ECU Configuration Parameter Definition**（配置"模板/词典"：有哪些模块、容器、参数、取值范围、多重性、配置类，供 editor/generator 知道结构与限制；SWS 每个模块第 10 章就是它的 M1 实例，p.23）；
  - **ECU Configuration Values**（单个 ECU 的实际值，ARXML，p.21–24）。
  二者独立：`EcucDefinitionCollection` 与 `EcucValueCollection` 互不依赖（p.118）。
- **定义侧元素**（ECUC p.27–36）：`EcucDefinitionCollection`（`TPS_ECUC_02003/02004`）→ `EcucModuleDef`（`TPS_ECUC_02005`，BSW、RTE、基础设施、SWC 等"软件模块"皆可）→ `EcucContainerDef`（`EcucParamConfContainerDef`、`EcucChoiceContainerDef`，p.41–45）→ 参数/引用定义（Boolean/Integer/Float/String/Enumeration/FunctionName/LinkerSymbol；Reference/ForeignReference/InstanceReference/SymbolicNameReference，p.57–82）。
  - **StMD vs VSMD**：`category` 为 `STANDARDIZED_MODULE_DEFINITION`（AUTOSAR 随 SWS 交付，包路径 `/AUTOSAR/EcucDefs/`，`TPS_ECUC_02130`）或 `VENDOR_SPECIFIC_MODULE_DEFINITION`（厂商扩展，须引用其细化的 StMD，`refinedModuleDef`，`TPS_ECUC_06043/06044`，p.36）。CDD 的 StMD 不规定配置类/变体，由实现者在 VSMD 里定（`TPS_ECUC_02139/02144`，p.273）。
- **值侧元素**（p.118–134）：`EcucValueCollection`（引用 ecuExtract，`constr_3588`）→ `EcucModuleConfigurationValues`（引用 definition，带 `implementationConfigVariant`，`TPS_ECUC_03016`，p.120）→ `EcucContainerValue`（`TPS_ECUC_03012`，p.128）→ 参数值/引用值（p.134–153）。可有多份 EcucValueCollection 对应同一 ecuExtract（p.119）。
- **配置类（Configuration Class）与变体（Variant）**（p.49–55、p.52–53）：
  - `EcucConfigurationClassEnum`：`PublishedInformation`（预编译前就固定）、`PreCompile`、`Link`、`PostBuild`（p.49）。
  - `EcucConfigurationVariantEnum`：`VariantPreCompile`、`VariantLinkTime`、`VariantPostBuild`（以及 Preconfigured/RecommendedConfiguration）（p.49、p.52：`TPS_ECUC_02097–02100`）；XML 里写作 `VARIANT-PRE-COMPILE/LINK-TIME/POST-BUILD`。
  - 每个参数/引用用 `valueConfigClass`（值最迟何时可变）和 `multiplicityConfigClass`（实例个数最迟何时可变）为每个 variant 指定配置类（`TPS_ECUC_08034/08035`，p.52）；各自多重性不得超过 3（`constr_3091/5015`，p.53）。**同一 variant 内一个参数只能属于一个配置类；不同参数可在同一 variant 里是不同配置类**（对应 EXP p.121 的同义描述）。
  - 模块支持 post-build 变体：`postBuildVariantSupport`（`TPS_ECUC_08012`，p.35），容器/参数另有 `postBuildVariantMultiplicity/Value`（p.50）。
  - 值侧：`EcucModuleConfigurationValues.implementationConfigVariant` 表明该模块这次采用哪个 variant（`constr_3590`，p.119）。
  - 三类配置类的**含义**，权威描述在 EXP（p.111–118）与 METH（p.109–120），见 1.7 与 3.2：pre-compile=改值须重新编译，模块须有源码；link-time=适合仅有目标码的模块，配置在单独编译单元；post-build=配置放独立可重刷内存段，模块 init 时拿指针。
- **工具链如何消费**（ECUC p.18、METH p.94–104）：
  - ECUC 方法本身**不规定工具策略**，只规定交换格式（p.18）；configuration editors 写 ECU Configuration Values，**module generators 读取其中属于自己的子集并生成代码/数据结构**（METH `TR_METH_01116`，p.96；p.104）。
  - 附录列出两种实现策略（informative）：**custom editors+generators**（每个 BSW 模块配自己的编辑器与生成器，如 RTE/COM/OS，ECUC p.279）与 **generic tools**（通用 editor 读取各模块 Parameter Definition，通用 generator，p.280）。
  - 建立 base ECU configuration 的规则：强制容器/参数（lowerMultiplicity>0）至少生成该数量实例（`TPS_ECUC_01016`，p.273）。
  - 运行时一致性：post-build 参数与 pre-compile/link-time 参数必须匹配，EcuM 在初始化第一个 BSW 模块前做一次配置一致性检查（ECUM p.43，`SWS_EcuM_02796`）。

---

## 5. SWC Template 基础（SWCT / SWMG）

### 5.1 SwComponentType 种类

SWCT Figure 3.4（p.71）：`SwComponentType` → `AtomicSwComponentType`（抽象，**唯一可聚合 `SwcInternalBehavior`**，`TPS_SWCT_01108`，p.70）、`ParameterSwComponentType`（**不能**有 InternalBehavior，且只能有 ParameterInterface 的 PPort，`constr_1092`，p.71–72）、`CompositionSwComponentType`（不能有 InternalBehavior/RunnableEntity，`TPS_SWCT_01097/01098`，p.525）。`AtomicSwComponentType` 的子类：

| 类型 | 说明与页 |
|---|---|
| `ApplicationSwComponentType` | 硬件无关应用软件（p.71） |
| `SensorActuatorSwComponentType` | 与具体传感/执行器绑定；与 Application 不同，**可直接通过 port 使用 I/O hardware abstraction**（`TPS_SWCT_01047/01048`，p.653–654） |
| `EcuAbstractionSwComponentType` | I/O Hardware Abstraction 在 VFB 上的表示（`TPS_SWCT_01389/01391`；可有子结构 `01390`；BSW 部分与 BswModuleDescription 映射 `01392`，p.654–656）；IOHWAB 要求其实现为一个或多个该类型实例（`SWS_IoHwAb_00025`，IOHWAB p.24） |
| `ComplexDeviceDriverSwComponentType` | CDD（见 1.4） |
| `ServiceSwComponentType` | 表示标准 BSW 服务；**只在 ECU 配置阶段加入**，每个服务一个 SwComponentPrototype（`TPS_SWCT_01412`，p.667）；与应用 SWC 只能本地通信（`01413`，p.667） |
| `ServiceProxySwComponentType` | 服务的代理，用于远端 ECU 上的应用访问（如 mode 管理，`TPS_SWCT_01414–01416`，p.667–669；VFB p.51） |
| `NvBlockSwComponentType` | 在 VFB 上提供非易失数据（`TPS_SWCT_01142/01143`，p.670） |

SWMG 提醒：这是**建模/命名约定**文档，不含工作流；例如用 AR Package 区分不同提供者（`TR_SWMG_00003/00017`，SWMG p.13）、不要为变体定义不同 interface 而是复用 interface 建多个 port（`TR_SWMG_00011`，p.14）、功能相关元素聚类放一起（`TR_SWMG_00008`，p.16）。

### 5.2 InternalBehavior / RunnableEntity / RTEEvent

- `SwcInternalBehavior`：形式化描述 AtomicSwComponentType 对 RTE 的需求（RunnableEntity 与其响应的 RTEEvent 等）（`TPS_SWCT_01075`，SWCT p.520）；聚合是 `atpSplitable`，可在 VFB 视图完成之后再补（p.70、p.520）。其下聚合：`runnable`、`event`、`exclusiveArea(Policy)`、`explicit/implicitInterRunnableVariable`、`arTypedPerInstanceMemory`、`perInstanceMemory`、`portAPIOption`、`includedDataTypeSet`、`includedModeDeclarationGroupSet`、`staticMemory/constantMemory`、`sharedParameter/perInstanceParameter` 等（p.520–525）。
- `RunnableEntity`：SWC 提供的**最小代码片段**，（至少间接）受 OS 调度，极少数在 ISR 上下文（`TPS_SWCT_01030`，p.525）。**只有原子 SWC 才有 runnable**（`01098`）。运行时所属 task 由 integrator 在 ECU 配置时映射（METH p.42；EXP p.125）。
- **RTEEvent**（`TPS_SWCT_01314`，p.545）：定义"触发是什么 / 哪些 ModeDeclaration 禁用 / 启动哪个 RunnableEntity（`startOnEvent`）"。子类（p.553–554、p.544–552）：
  - 通信类：`DataReceivedEvent`、`DataReceiveErrorEvent`、`DataSendCompletedEvent`、`DataWriteCompletedEvent`；
  - C/S：`OperationInvokedEvent`、`AsynchronousServerCallReturnsEvent`；
  - 触发类：`ExternalTriggerOccurredEvent`、`InternalTriggerOccurredEvent`；
  - 模式类：`SwcModeSwitchEvent`、`SwcModeManagerErrorEvent`、`ModeSwitchedAckEvent`；
  - 其他：`TimingEvent`、`BackgroundEvent`、`InitEvent`（`TPS_SWCT_01525`，p.544；约束：`minimumStartInterval` 必为 0，`constr_1258`，p.545）、`OsTaskExecutionEvent`、`TransformerHardErrorEvent`（p.552）。
- **同一 SWC 内 runnable 之间通信**（p.557–565）：两种建模——**ExclusiveArea**（只是对 RTE 调度策略的约束：引用同一 ExclusiveArea 的 runnable 同一时间只允许一个在区内，**不隐含具体实现如信号量**，`TPS_SWCT_01031`，p.557–558）；或 **inter-runnable variable**（implicit/explicit，`explicit/implicitInterRunnableVariable`，p.521、p.562）。**同一 SWC 不同实例的 runnable 间通信只能经 port**（`TPS_SWCT_01592`，p.557）。
- **数据访问点：implicit vs explicit**（SWCT §7.5.1，p.570–571）：术语约定——"implicit"= 基于 **data access**（`dataReadAccess/dataWriteAccess`，在 runnable 开始/结束时按 RTE 缓存语义读写，仅适用于 category 1 runnable，因其保证有限响应时间，`TPS_SWCT_01323/01325`，p.571）；"explicit"= 基于 **data points**（`dataSendPoint`、`dataReceivePointByValue/ByArgument`，显式调用 RTE API，p.570）。文档自述这套命名沿用 RTE SWS 早期术语（p.570–571）。S/R 的形式化 implicit 行为定义见 SWCT §4.8（p.216）。
- **PerInstanceMemory**（SWCT §7.7，p.601–604）：支持多实例（`supportsMultipleInstantiation == true`）的 SWC 通常需要每实例私有内存，**由 RTE 提供各实例访问自己实例内存的机制**（`TPS_SWCT_01359`）；可定义任意多块（`01360`）；**不支持多实例**的 SWC 不一定需要 PerInstanceMemory，可用静态变量，但仍允许用（`01361`）；有 C 类型与 AUTOSAR 类型两种（p.602–604，`arTypedPerInstanceMemory`）。
- **SWC 向 RTE 声明的服务依赖**：`ServiceNeeds`（SWCT §7.11，p.607–620），具体 use case 见第 13 章（NvM、Watchdog、ComM、EcuM、BswM、Crypto、Diagnostic 等，p.708+）。

---

## 6. BSW 通用规则（BSWG）与 Memory Mapping（MEMMAP）

### 6.1 模块前缀与文件结构（BSWG §5.1，p.16–30）

- **Module abbreviation `<Ma>`**（如 EcuM、CanIf、Com）取自 "List of BSW Modules"（`SWS_BSW_00101`，p.16）；**Module implementation prefix `<Mip>` = `<Ma>[_<vi>_<ai>]`**（vendorId/vendorApiInfix，多实例或厂商区分用；CDD 和 transformer 的 `<Mip>` 来自 `apiServicePrefix`）（`SWS_BSW_00102`，p.16–17）。文件名 `<Mip>[_<Ie>]*.*`，区分大小写，不得仅大小写不同（`SWS_BSW_00103/00170/00171`，p.18）。
- 文件类型表 Table 5.1（p.17）：Module documentation、BSW Module description（`.arxml`，`SWS_BSW_00001`，p.19）、Implementation source `Com.c`、Implementation header `Com.h`、**pre-compile 配置 source `Com_Cfg.c`（conditional）**、**link-time `Com_Lcfg.c`**、**post-build `Com_PBcfg.c`**、interrupt frame `Gpt_Irq.c`。
  - 补充：post-build 配置源文件名 `<Mip>[_<Ie>]_PBcfg.c`（`SWS_BSW_00015`，p.24）；link-time 源文件 `_Lcfg.c` 或 `_Cfg.c`（`SWS_BSW_00013`，p.24）；`<Mip>_MemMap.h`（`SWS_BSW_00006`，p.22）；`SchM_<Mip>.h`（`SWS_BSW_00007`，p.22）；`<Mip>_Externals.h`（callout 声明，`SWS_BSW_00254`，p.27–28）；ISR 建议单独成文件（`SWS_BSW_00181`，p.23）。
  - **`Mod_Cfg.h`**：BSWG 示例中使用（如 `Nm_Cfg.h`、`CanTp_Cfg.h`、`Eep_21_LDExt_Cfg.h`，p.56–57、p.76、p.88），但 Table 5.1 只把 `_Cfg.c` 列为 pre-compile 配置 source，**没有把 `<Mip>_Cfg.h` 列成独立"必需文件类型"**——这是 pre-compile 配置 header 的常规做法（由 generator 产生，METH `TR_METH_01096` 称 "BSW Module Configuration Header File"，p.110）。
  - **`Mod_Cbk.h`**：BSWG R25-11 **没有**这个文件名；它只规定"调用其他模块 callback 时 include 对应模块头文件"（`SWS_BSW_00010`，p.23），回调原型出现在被调用模块的 SWS "Callback notifications"章节与 "Available via" 栏。`_Cbk.h` 是业界/具体 SWS 的惯用文件名，教程若提到须标"以具体模块 SWS/实现为准"。例：PduR 要求为各下层模块提供各自头文件 `PduR_CanIf.h` 等（`SWS_PduR_00216`，PDUR p.25）。
  - 头文件要求：Implementation header `<Mip>.h` 至少存在（`SWS_BSW_00020`，p.26），声明 API（`SWS_BSW_00048`），用 include guard（`SWS_BSW_00249`，p.28–29），**不得**含需被 RTE/BswScheduler 调用的 MainFunction 与 BswModuleClientServerEntry 原型（`SWS_BSW_00210`，p.28）；包含 `Std_Types.h`（`SWS_BSW_00024`，p.27）；使用 RTE 接口的模块文件含 `Rte_<swc>.h`，但**不得放进被其他模块包含的头**（`SWS_BSW_00025/00069`，p.29）。
- **Published information**（p.91–92）：`<MIP>_VENDOR_ID`、`<MIP>_MODULE_ID`、`<MIP>_AR_RELEASE_{MAJOR,MINOR,REVISION}_VERSION`、`<MIP>_SW_{MAJOR,MINOR,PATCH}_VERSION` 等 `#define`，配置自 BSWMD（`SWS_BSW_00059/00256`）。**Inter Module Checks**：模块用预处理检查所有被包含头的版本，工具检查所有集成模块属于同一 AUTOSAR 主/次版本（`SWS_BSW_00036`，p.30）。
- **GetVersionInfo**（p.76–77）：同步、可重入（`SWS_BSW_00064`）；参数为 `Std_VersionInfoType*`，含 vendorId、moduleId、软件版本（`00052`）；API 可用性由 `<Ma>VersionInfoApi` pre-compile 开关控制（`00051`），**默认不可用**（`00236`）；可在 init 前任何时间调用（`00164`）。
- **Module ID**：用于 `Det_ReportError` 的 `ModuleId` 参数，值取自附录 "List of Basic Software Modules"（`SWS_BSW_00045`，p.57；`<MIP>_MODULE_ID`，p.92）。

### 6.2 Init / DeInit 规则（BSWG §8.3.2–8.3.3，p.73–75）

- 命名 `<Mip>_Init`、`<Mip>_DeInit`（SRS_BSW_00310）；**不是所有模块都有**。
- **只有 EcuM 与 BswM 可调用 Init/DeInit**（`SWS_BSW_00150`、`00152`，p.73、p.75）；配置集选择由 EcuM/BswM 通过参数传入（p.73）。
- **Config 指针规则**（`SWS_BSW_00050`，p.74）：参数检查开启时检查指针；**VariantPostBuild（配置可加载）或 `postBuildVariantUsed` 为真时必须是非 NULL**，否则报 "Invalid configuration set selection" 开发错误；其余情况下 NULL_PTR 是合法值（`SWS_BSW_00212` 把 pre-compile 变体 Init 的配置指针列为合法 NULL 例子，p.73）。
- Init 结束设置模块状态（`SWS_BSW_00071`，用于 dev error 检测）；复位后须先于其他函数调用 Init（`SWS_BSW_00230`；个别例外如 Dem pre-init、GetVersionInfo）；不得重复调用，除非 DeInit 之后（`00231`）；DeInit 只在已初始化后调用、不得重复（`00232/00233`，p.75）。

### 6.3 MainFunction / Scheduled functions

见 1.6：命名、无参无返回、不可等待、模块内禁止互调、未初始化立即返回、原型由 SchM 提供。另：Exclusive area 在 BSWMD 中定义，**只保护模块内部数据**（`SWS_BSW_00038/00134`，p.44）。示例命名：`Com_MainFunctionRx/Tx/RouteSignals`（p.79）；**R25-11 的 COM 更进一步：每个配置的 `ComMainFunctionRx/Tx` 容器对应一个 `Com_MainFunctionRx_<shortName>`/`Com_MainFunctionTx_<shortName>`**（`SWS_Com_00398/00399`，COM p.132–133）。

### 6.4 DET 使用与错误分类（BSWG §7.2，p.54–60）

- 错误分类 `SWS_BSW_00144`（p.55）：**development errors、runtime errors、production errors、extended production errors**。
- **Development errors**：软件 bug（如使用未初始化模块）；经 `Det_ReportError` 报给 Det，行为应"像 assertion"（停机或复位，p.55）；**受开关 `<Ma>DevErrorDetect` 控制，默认关闭**（`SWS_BSW_00042`，p.56）；开启 → API 参数检查开启（`00203`）；检测到须报 Det，使用模块 ID（`SWS_BSW_00045`，p.57）；错误值类型 uint8（`00201`）；**若开启 dev 检测，除 GetVersionInfo、Init、调度函数外，在未初始化时调用应报 `<MIP>_E_UNINIT`**（`SWS_BSW_00243`，p.57），且 UNINIT 检查应最先做（`00255`，p.55）；指针参数要查 NULL_PTR（`SWS_BSW_00212`，p.73）。
- **Runtime errors**：运行中由软硬件故障引起（队列溢出、API 在错误时刻调用、辐射干扰等），可自愈；**不能通过配置关闭**；经 `Det_ReportRuntimeError` 报告（`SWS_BSW_00222`，p.58–59）。
- **Production errors / extended production errors**：硬件相关故障（老化、短路…），报 Dem（`Dem_SetEventStatus`，p.59–60、`SWS_BSW_00205`）。
- Det API（R25-11）：`Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)` **返回 `Std_ReturnType`**（`SWS_Det_00009`，DET p.21，ID 0x01）；`Det_ReportRuntimeError`（`SWS_Det_01001`，p.23，ID 0x04）；`Det_Init`（`SWS_Det_00008`，p.21）。（旧 release 的 Det_ReportError 返回 void——写教程引代码时须用项目 release 的签名。）

### 6.5 Callback 与 Callout

- **Callback**（p.77–78）：作为 AUTOSAR Service（经 RTE 路由）的回调，签名跟随 RTE `Rte_Call` 的 server 调用签名（`SWS_BSW_00180`）；其他回调返回类型尽量 void（`00172`），允许参数（`00173`）；SWS 会声明每个回调是否可能在**中断上下文**调用，须尽量短（`SWS_BSW_00167`）；**BSW 模块在自己的首次 MainFunction 调用之前不得调用 RTE 接口**（`SWS_BSW_00218`，p.78）。
- **Callout**（p.44）：模块不知道其内存段，integrator 可独立映射（`SWS_BSW_00136`）；声明放 `<Mip>_Externals.h`（`SWS_BSW_00254`，p.27）；MEMMAP 有 `CALLOUT_CODE` 段类型（`SWS_MemMap_00083`，MEMMAP p.36）。
- **ISR / 寄存器**（p.45–49）：ISR 与 OS task 转换受限（`SWS_BSW_00182`）、BSW 对 OS 服务的使用受限（`00138`；`00257` 表）；直接访问硬件寄存器的模块必须容忍并发访问（`SWS_BSW_00179`）；"write-once"寄存器由 MCAL driver 提供配置选项（`SWS_BSW_00188`）。

### 6.6 Memory Mapping（MEMMAP）基础

- **目的**：memory-mapping 头文件含编译器/链接器相关关键字，把变量/函数放入指定 section，使实现与编译器/µC 无关；**section 到物理内存区间的分配不在 MemMap 范围，通常由链接器控制文件完成**（`SWS_MemMap_00001` 附近，MEMMAP p.19）。每个构建场景（如 Boot loader、ECU Application）要有自己的一套 memmap 文件（`SWS_MemMap_00001`）。
- **文件命名**：BSW 为 `{Mip}_MemMap.h`，SWC 为 `{componentTypeName}_MemMap.h`（`SWS_MemMap_00002`，p.19；`00032/00029`，p.14）。**由 integrator/工具根据 BswImplementation / SwcImplementation 的 MemorySection 生成**（`SWS_MemMap_00026/00027`，p.39–40）。
- **机制**（p.39–40）：源码在声明变量/函数前 `#define <PREFIX>_START_SEC_<name>…`，再 `#include "<Mip>_MemMap.h"`；memmap 头据此插入 `#pragma` 之类；结束时 `<PREFIX>_STOP_SEC_<name>`（`SWS_MemMap_00005/00015`）。编译器不需要特殊命令时关键字可 undef（`SWS_MemMap_00010`，p.19）；编译器不支持所需功能时必须定义为报错（`SWS_MemMap_00036`，p.20）。
- **标准 section 类型**（`SWS_MemMap_00038`，p.21）：`VAR`、`VAR_FAST`、`VAR_SLOW`、`INTERNAL_VAR`、`VAR_SAVED_ZONE`、`CONST`、`CONST_SAVED_RECOVERY_ZONE`、`CONFIG_DATA`、`CALIB`、`CODE`、`CODE_FAST`、`CODE_SLOW`、`CALLOUT_CODE`；关键字语法如 `{PREFIX}_START_SEC_CALIB[_{refinement}][_{safety}]_{ALIGNMENT}`（`SWS_MemMap_00073`，p.34），`{ALIGNMENT}` 取 BOOLEAN/8/16/32 等（p.21–22）。
- 方法学里的位置：*Configure Memmap Allocation*（ECU Integrator）、*Generate BSW Memory Mapping Header / Generate SWC Memory Mapping Header*（METH p.100–101、p.184–185、p.104）；SWC 的 Runnable 通过 SwAddrMethod 表达"放入 CODE section"（MEMMAP §7.4.1，p.44）。

---

## 7. MCAL 通用架构与 Mcu/Port/Dio/Adc 要点

### 7.1 MCAL 做什么 [规范]

- **位置与目的**：BSW 最低层；包含对 µC 及片内外设有直接访问的驱动；使上层独立于 µC；实现 µC 相关，上接口标准化（EXP p.15、p.29）。"MCAL is available on each standard microcontroller"，对 BSW 提供 µC 无关的值，并通过 notification 机制把命令、响应、信息分发给不同进程（VFB p.86–87）。
- **模块**：见 1.2（Microcontroller / Communication / Memory / I/O / Crypto / Wireless Drivers）。本次 R25-11 变更：EXP 的变更历史显示 **R25-11 从该文档中移除了 Fls、Eep**（p.2）、此前 R21-11 引入新的 Memory Driver 与 Memory Access 概念（p.3）；教程介绍存储驱动时不要再把 Fls/Eep 当作 R25-11 MCAL 标准模块，而应说明"取决于项目 release"。
- **典型 MCAL 共同结构**（以下由 Mcu/Port/Dio/Adc 的 SWS 归纳；**[推断]**，非 EXP 明文）：① 一个带 `ConfigPtr` 的 `<Mip>_Init`（Dio 例外）；② 取决于 pre-compile/post-build 变体，ConfigPtr 可为 NULL（见 7.3）；③ `GetVersionInfo`；④ DET 开发错误（`<MOD>_E_UNINIT` 等）；⑤ notification 回调（上层 IoHwAb 提供，名称可配置）与/或 `MainFunction`；⑥ 寄存器初始化职责划分规则（见 7.3，MCU/PORT/ADC 三个 SWS 重复使用同一组规则）。
- **并发**：MCAL 水平接口不允许（EXP p.79），MCAL 模块可开关中断（EXP p.134）。

### 7.2 谁调用 MCAL（带引用）

| 调用者 | 内容 | 出处 |
|---|---|---|
| **EcuM** | 调用 MCAL 的 Init（驱动初始化 callouts `EcuM_AL_DriverInitZero/One`，以及 `EcuMDriverInitListBswM`）；Init 只允许 EcuM/BswM 调 | BSWG `SWS_BSW_00150`（p.73）；ECUM p.37–42；ADC 序列图 EcuM→`Adc_Init` / `Adc_DeInit`（ADC p.83）；EcuM 结构图含 `Adc_Init`、`Can_Init`、`CanTrcv_Init` 的可选调用（ECUM p.35） |
| **上层 BSW（ECU Abstraction）** | 例：CanIf 调 Can driver；IoHwAb 调 ADC/DIO/PORT/PWM/ICU/OCU/GPT API，并接收 ADC/PWM/ICU/GPT 通知；Memory Hardware Abstraction 调 memory driver | EXP p.34、p.80；IOHWAB `SWS_IoHwAb_00078`、表 p.15 |
| **IoHwAb** | "I/O Hardware Abstraction 位于 MCAL 驱动之上，调用驱动 API 管理片内设备"，**须让 SWC 能访问所有 MCAL 驱动**；ADC 通知回调命名为 `IoHwAb_Adc_Notification<#groupID>`（Adc 的 configurable interface，ISR 上下文） | IOHWAB p.14–15；ADC `SWS_Adc_00082`（p.81） |
| **CDD** | 直接访问 µC；也可使用 SPI、GPT、I/O drivers（受并发限制） | EXP p.32、p.82 |
| **SWC** | **不直接调 MCAL**：层间规则不允许绕过；SWC 通过 RTE→Sensor/Actuator SWC / EcuAbstraction SWC（IoHwAb）访问 I/O。唯一例外是 SensorActuator SWC 可经 port 用 I/O hardware abstraction，而不是 MCAL | EXP p.79–80；VFB p.86–88；SWCT `TPS_SWCT_01048`（p.654） |

### 7.3 MCU / PORT / DIO / ADC 的 API 要点（R25-11）

**公共规则（三份 SWS 重复）**——"控制器寄存器初始化"归属：只允许一个模块使用的寄存器由该模块初始化；**影响多个硬件模块的 I/O 寄存器由 PORT driver 初始化**，**影响多个硬件模块的非 I/O 寄存器由 MCU driver 初始化**；复位后须立刻初始化的一次性可写寄存器与其余寄存器由 **start-up code** 初始化。ID：MCU `SWS_Mcu_00116/00244/00245/00246/00247`（MCU p.25）；PORT `SWS_Port_00113/00214/00215/00217/00218`（PORT p.25）；ADC `SWS_Adc_00246–00249`（ADC p.52–53）。

**Mcu**（MCU p.17–32）：
- 提供 **Clock 与 RAM 初始化**、复位原因/复位执行、power mode（`SWS_Mcu_00055/00052/00248/00164`，p.17–18）；开关 ECU/µC 电源**不是** MCU driver 的任务（p.13）。
- 调用顺序：start-up code（栈、中断向量基址等极少量 µC 相关初始化，**此时 clock/PLL 尚未初始化**，MCU p.14）→ `Mcu_Init(const Mcu_ConfigType* ConfigPtr)`（`SWS_Mcu_00153`，ID 0x00，同步、不可重入，p.25；使 power-down/clock/RAM 配置"可见"，`SWS_Mcu_00026`）→ `Mcu_InitRamSection`（`00154`，0x01，`Std_ReturnType`，须在 Init 之后，`00136`，p.26）→ `Mcu_InitClock(Mcu_ClockType)`（`00155`，0x02，须在 Init 之后，`00139`，p.26–27）→ 等 PLL 锁定 → `Mcu_DistributePllClock`（`00156`，p.27；**PLL 未锁定则立即返回 E_NOT_OK**，`SWS_Mcu_00142`，p.28）。另有 `Mcu_GetPllStatus`、`Mcu_GetResetReason`（`00158`）、`Mcu_GetResetRawValue`、`Mcu_PerformReset`（`00160`）、`Mcu_SetMode`（`00161`）、`Mcu_GetVersionInfo`、`Mcu_GetRamState`（`00207`）。
- DET：未初始化调用其他函数报 `MCU_E_UNINIT`（`SWS_Mcu_00125`，p.34）。
- 提醒（`00-writing-conventions.md`）：P1M-E 无 PLL 寄存器；写 `Mcu_InitClock` 时先讲通用模型，再讲 P1M-E 的实际情况，其他 derivative 注明"以芯片手册为准"。

**Port**（PORT p.24–29）：
- `Port_Init(const Port_ConfigType* ConfigPtr)`（`SWS_Port_00140`，ID 0x00）：**初始化所有端口与引脚**（`00041`），**须是第一个被调用的 Port 函数，未调用则不能对 MCU 引脚做任何操作**（`00078/00213`）；避免 glitch（`00043`）；若硬件有输出锁存，先写默认电平再切方向为输出（`00055`，p.26）；**函数参数总是指针，pre-compile 变体下环境传 NULL**（`SWS_Port_00121`，p.26）；复位后须再调用（`00071`）；运行中不得再调（多调用者时，p.26）。
- 运行时 API：`Port_SetPinDirection`（`00141`，0x01，可重入，p.26–27）、`Port_RefreshPortDirection`（`00142`）、`Port_SetPinMode`（`00145`，0x04）、`Port_GetVersionInfo`（`00143`）。
- DET 代码：`PORT_E_PARAM_PIN 0x0A`、`PORT_E_DIRECTION_UNCHANGEABLE 0x0B`、`PORT_E_INIT_FAILED 0x0C`、`PORT_E_UNINIT 0x0F`、`PORT_E_PARAM_POINTER 0x10`…（PORT p.20）。

**Dio**（DIO p.9–35）：
- **Dio 没有 Init**：只对"已由 PORT driver 配置好的"pin/port 读写，**不得提供初始化硬件的接口**（`SWS_Dio_00001`，p.19）、**不提供整体配置/初始化 API**（`SWS_Dio_00061`，p.14）；**用户须在 Port driver 初始化后才用 Dio，否则行为未定义**（`SWS_Dio_00102`，p.14）。
- API（同步、可重入）：`Dio_ReadChannel`（`SWS_Dio_00133`，0x00，p.28）、`Dio_WriteChannel`（`00134`，0x01）、`Dio_ReadPort/WritePort`（`00135/00136`）、`Dio_ReadChannelGroup/WriteChannelGroup`（`00137/00138`）、`Dio_FlipChannel`（`00190`，0x11，p.34）、`Dio_MaskedWritePort`（`00300`，0x13）、`Dio_GetVersionInfo`（`00139`，0x12）。
- DET：`DIO_E_PARAM_INVALID_CHANNEL_ID 0x0A`、`DIO_E_PARAM_INVALID_PORT_ID 0x14`、`DIO_E_PARAM_POINTER 0x20`（DIO p.23–24）。

**Adc**（ADC p.42–82）：
- `Adc_Init(const Adc_ConfigType* ConfigPtr)`（`SWS_Adc_00365`，0x00，不可重入，p.52）：**Variant PB 时按 ConfigPtr 初始化；Variant PC 需传 NULL_PTR**（p.52 参数栏）；只初始化已配置资源（`SWS_Adc_00056`）；重复初始化报 `ADC_E_ALREADY_INITIALIZED`（0x0D），未初始化调用报 `ADC_E_UNINIT`（0x0A）（p.40）。EcuM→Adc_Init 序列见 p.83。
- **组（group）状态** `Adc_StatusType`：`ADC_IDLE`、`ADC_BUSY`、`ADC_COMPLETED`、`ADC_STREAM_COMPLETED`（`SWS_Adc_00513`，p.45）；状态迁移图 p.34–36。
- 典型流程 API：`Adc_SetupResultBuffer`（`SWS_Adc_91000`，p.53）→ `Adc_StartGroupConversion`（`00367`，p.56）/ `Adc_EnableHardwareTrigger`（`91001`）→ `Adc_GetGroupStatus`（`00374`）→ `Adc_ReadGroup`（`00369`，p.61）、`Adc_StopGroupConversion`（`00368`）、`Adc_EnableGroupNotification`（`91003`）、`Adc_GetStreamLastPointer`（`00375`）、`Adc_DeInit`（`00366`）、电源状态 API（`00475–00477`）。
- 通知：ISR 中调用 `IoHwAb_Adc_Notification<#groupID>`，名称可配置，须尽量短（`SWS_Adc_00078/00082`，p.81）。

### 7.4 IoHwAb：SWC ↔ MCAL 的桥

- 定位：属于 ECU Abstraction 的 I/O Hardware Abstraction，**不是单一模块**（可由多个模块组成）；目的：**通过把 I/O Hardware Abstraction port 映射到 ECU signal 来访问 MCAL 驱动**，提供给 SWC 的数据完全脱离物理层数值，SWC 设计者不再需要知道 MCAL API 与物理量单位（IOHWAB p.8）。
- **规范并不给出 C API**（只是实现指南，"not intended to standardize this module"，p.8）；**总是 ECU 专属实现**（p.8）；是 **integration code**（ECU 原理图相关的软件，位于 RTE 之下，p.20）；须含硬件保护策略（`SWS_IoHwAb_00038`），**不含故障恢复策略——恢复由负责的 SWC 决定**（`SWS_IoHwAb_00039`，p.21）。
- **向上**：不能提供 Standardized AUTOSAR Interface（接口取决于信号采集链），而是提供代表"ECU 输入/输出电气信号"的 AUTOSAR Interface（p.21）；**必须基于 SWC Template，实现为一个或多个 `EcuAbstractionSwComponentType`**（`SWS_IoHwAb_00001/00025`，p.24）；只能通过 PortPrototype 与上层交互，不允许隐藏依赖（p.24）。ECU signal 要带 Filtering/Debounce、Age 等属性（`SWS_IoHwAb_00019/00021`，p.23）；用 `IoHwAbstractionServerAnnotation` 标注 port（p.25）。
- **向下**：调用 ADC/OCU/PWM/ICU/DIO/PORT/GPT driver API，并接收 ADC/OCU/PWM/GPT/ICU 的 notification（DIO/PORT 无 notification）（p.15）；若有板载器件，还经 SPI 等通信驱动访问（`SWS_IoHwAb_00079`，p.15）；与 System Services 接口：EcuM（init 函数）、Det、BSW Scheduler（`SWS_IoHwAb_00044`，p.16）；向 DCM 提供 `IoHwAb_Dcm.h` 接口以做功能诊断（p.17）。
- **Init**：`IoHwAb_Init<Init_Id>(const IoHwAb{Init_Id}_ConfigType* ConfigPtr)`（`SWS_IoHwAb_00119`，ID 0x01）；**ConfigPtr 目前必须是 NULL_PTR**（`00158`）；多个外部器件可各自有 init（`00060`）；**由 EcuM 调用，init 顺序可由 integrator 在 EcuM 里配置**（`00061`，p.42）。
- 与 VFB 的对应：VFB 把 ECU abstraction 表示为 EcuAbstraction SWC，Sensor-Actuator SWC 通过它拿 AUTOSAR signal（VFB p.53、p.87；SWCT p.652–654）。

### 7.5 初始化顺序（补充 ECUM，R25-11）

- EcuM 假设调用 `EcuM_Init` 之前已完成最小 µC 初始化（栈与 C 变量初始化，`ECUM p.37`）。
- **StartPreOS Sequence**（`SWS_EcuM_02411`，p.37–38）：`EcuM_AL_SetProgrammableInterrupts`（可选）→ **Init block 0**：callout `EcuM_AL_DriverInitZero`（只能初始化**不使用 post-build 配置参数**的 BSW 模块）→ `EcuM_DeterminePbConfiguration`（返回含所有 BSW post-build 配置的指针结构）→ 配置一致性检查 → **Init block I**：`EcuM_AL_DriverInitOne` → 取 reset reason（`Mcu_GetResetReason`）→ 选默认 shutdown target …；**StartPreOS 应初始化启动 OS 所需的所有 BSW**（`SWS_EcuM_02603`，p.41）；"MCU_Init 并不提供完整 MCU 初始化，其余硬件相关步骤须在 InitZero/InitOne callouts 中完成"（p.41）。
- 然后 `StartOS`；integrator 须有自动启动的 OS task，首个动作调用 `EcuM_StartupTwo`（p.37）→ **StartPostOS Sequence**（`SWS_EcuM_02934`，p.41–42）：Start BSW Scheduler（`SchM_Start`：`SchM_Init` + `SchM_StartTiming`）、**`BswM_Init`**；其余驱动可经 `EcuMDriverInitListBswM`/`EcuM_AL_DriverInitBswM_<x>` 在此阶段由 BswM 触发（ECUM p.35、p.12 目录）。
- **[规范]**：BSWG 允许 EcuM/BswM 调 Init；EcuM 持有各模块 post-build 配置指针（EXP p.122–123）。**[业界实践]**：具体哪些驱动放 InitZero/InitOne/BswM 列表由 integrator 配置，要以项目配置为准。

---

## 8. 通信路径："一个 signal 的一生"

> 只写规范能支撑的函数与方向。R25-11 的 CanIf↔PduR 之间多了一层可选的 **L-SDU Router**，且 PduR 的 API 是**带占位符的通用接口**；见 8.4 的版本提醒。

### 8.1 概念链

`Com signal` →（打包）→ `I-PDU` →（路由）→ `L-PDU/L-SDU` → CAN frame。PDU 命名：I-PDU（Com/DCM/PduR/IpduM 层）、N-PDU（TP 层）、L-PDU（Driver/Interface 层）；PDU = SDU + PCI，发送时上层 PDU 被下层视作自己的 SDU（EXP p.94–95）。**Com 路由单个 signal/signal group 在 I-PDU 之间，PduR 路由 I-PDU 在抽象通信控制器与上层之间，PduR 不修改 I-PDU 内容**（EXP p.96；`SWS_PduR_00160`，PDUR p.33）。I-PDU 由静态 I-PDU ID 标识，目标由静态表决定，每个处理 I-PDU 的模块自带 ID 查找表（PDUR p.16–17、p.33）。

### 8.2 发送（TX）

| 步骤 | 函数 | 要点 / 出处 |
|---|---|---|
| 0. SWC 写 | `Rte_Write_<port>_<elem>`（RTE） | S/R 跨 ECU：**RTE 对每个原语元素调用 `Com_SendSignal`，对 signal group 调 `Com_SendSignalGroup`**；RTE 使用 ComSignal 的 symbolic name；signal group 内数据置于同一 I-PDU 以保证原子性（`SWS_Rte_04527`，RTE p.326；`SWS_Rte_05081/05173`，p.325–326；`SWS_Rte_08793`，p.325） |
| 1. Com 更新 signal | `uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)` | `SWS_Com_00197`，COM p.105，ID 0x0a，**异步**、对同一 signal 不可重入；返回 `E_OK / COM_SERVICE_NOT_AVAILABLE（I-PDU group 已停）/ COM_BUSY`；"更新 signal object"（p.105）。**若 signal 的 `ComTransferProperty` 为 TRIGGERED 且所在 I-PDU 的 `ComTxModeMode` 为 DIRECT 或 MIXED，则立即发送（最迟在下一个 main function 内）**，除非被最小延时等机制延迟（`SWS_Com_00625`，p.106）；PENDING 属性不触发发送（`SWS_Com_00630` 对 DynSignal 的同类规定，p.107）；PERIODIC I-PDU 按周期发送 |
| 2. Com 触发 PduR | `Std_ReturnType PduR_ComTransmit(PduIdType, const PduInfoType*)` | 即 `PduR_<User:Up>Transmit` 的实例（`SWS_PduR_00406`，PDUR p.86，ID 0x49，同一 PduId 不可重入）；Com 在 DIRECT/MIXED 重复发送（`ComTxModeNumberOfRepetitions`）时以 `ComTxModeRepetitionPeriod` 周期调用，直至收到足够多确认（`SWS_Com_00305`，COM p.44）；`ComRetryFailedTransmitRequests` 控制失败重试（`SWS_Com_00467/00773`，p.44）；**周期/延迟类发送在 `Com_MainFunctionTx[_<shortName>]` 中执行**（`SWS_Com_00399`，p.133，ID 0x19）；Com 未 Init 时该函数直接返回（`SWS_Com_00665`） |
| 3. PduR 路由 | PduR 静态路由表：`PduR_ComTransmit → CanIf_Transmit` | PDUR p.16（"Translating the source I-PDU ID to the destination I-PDU ID (e.g. PduR_ComTransmit to CanIf_Transmit, PduR_CanIfTxConfirmation to Com_TxConfirmation)"）；PduR 须 `PduR_Init(const PduR_PBConfigType*)`（`SWS_PduR_00334`，PDUR p.82，ID 0xf0）后才处于 `PDUR_ONLINE`（`SWS_PduR_00742`，p.82） |
| 4. CanIf | `Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)` | `SWS_CANIF_00005`，CANIF p.85，ID 0x49；**要求控制器为 `CAN_CS_STARTED` 且通道 mode 至少发送路径 online/offline-active，否则不接受**（`SWS_CANIF_00317`）；TxPduId 解析出 controller 与 HTH；调用 **`Can_Write(Hth, Can_PduType{swPduHandle, length, id, sdu})`**（`SWS_CANIF_00318`，p.85–86；CAN XL 用 `CanXL_Write`，`00939`；Can_Write 返回 CAN_BUSY 时 CanIf 缓冲，p.44–46） |
| 5. Can driver | `Can_Write` → 写 mailbox/硬件 | Can driver SWS 不在本批 R25-11 文本清单内；仓库根目录 CAN Driver SWS 为 R22-11（见 `00-writing-conventions.md`），引用请用那份并注明 release |
| 6. 发送确认 | `Can` → `CanIf_TxConfirmation(PduIdType CanTxPduId)` → 上层 → `Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)` | `SWS_CANIF_00007`，CANIF p.114，ID 0x13（调用上下文为中断级或轮询任务级）；`SWS_PduR_00365`（`PduR_<User:Lo>TxConfirmation`，PDUR p.89，ID 0x40）；`SWS_Com_00124`，COM p.127，ID 0x40 |

### 8.3 接收（RX）

| 步骤 | 函数 | 要点 / 出处 |
|---|---|---|
| 1. Can driver 收到帧 | 中断或轮询 → `CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)` | `SWS_CANIF_00006`，CANIF p.115，ID 0x14，可重入；`Mailbox` 标识 HRH 与对应 controller。CanIf 做 software filtering（BasicCAN）与 Data Length Check（失败报 runtime error `CANIF_E_INVALID_DATA_LENGTH`），通过后调用上层（流程图 CANIF p.49；序列图 p.69） |
| 2. CanIf → 上层 | 上层 callback | **R25-11 CanIf 把上层称为 L-SDU Router**，名为 `LSduR_CanIfRxIndication(PduIdType, const PduInfoType*)`（CANIF p.49、p.69）；PduR 侧泛化为 `PduR_<User:Lo>RxIndication`（`SWS_PduR_00362`，PDUR p.89，ID 0x42），实例如 `PduR_CanIfRxIndication`（PDUR 正文示例，p.16、p.33） |
| 3. PduR → Com | `void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)` | `SWS_Com_00123`，COM p.125，ID 0x42，对同一 PduId 不可重入；Com 把 I-PDU 拷入自己的缓冲并拆包（Table 7.7 说明普通 I-PDU 使用 RxIndication/TxConfirmation/TriggerTransmit，TP I-PDU 使用 TpRxIndication 等一组，COM p.80） |
| 4. signal 通知时机 | 由 `ComIPduSignalProcessing` 决定 | **IMMEDIATE**：在 `Com_RxIndication` 内调用配置的 `<ComUser_CbkRxAck>`（`SWS_Com_00300`）；**DEFERRED**：RxIndication 内先拷贝数据，**下一次 `Com_MainFunctionRx[_<shortName>]` 才拆包并调用通知**（`SWS_Com_00301`，COM p.54）；DEFERRED 下若在拆包前调 `Com_ReceiveSignal`，返回旧值（p.54）。`Com_MainFunctionRx` 为 `SWS_Com_00398`，ID 0x18，p.132 |
| 5. 取 signal | `uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)` | `SWS_Com_00198`，COM p.107，ID 0x0b，同步、可重入；把 signal object 的数据拷到 `SignalDataPtr`；调用者须保证对齐（p.108）。**RTE 侧**：先对 signal group 调用 `Com_ReceiveSignalGroup` 再逐个 `Com_ReceiveSignal`（RTE p.326）；接收通知经 `Rte_COMCbk...`（`Rte_COMCbk_<sn>` 等，RTE p.333–337；COM 图 5.1 显示 Com 对 RTE 提供 `Rte_Cbk` 回调，COM p.18） |
| 6. SWC 读 | `Rte_Read_...` / DataReceivedEvent 触发 runnable | 见第 5 节 |

### 8.4 版本差异提醒（写教程务必复核）

1. **L-SDU Router**：EXP 变更历史写明 R24-11 起新增（EXP p.2）；EXP 的 CAN 栈图在 CanIf 之上放 L-SDU Router（p.40）；CANIF SWS 全文用 `LSduR_*` 命名上调（p.41、49、69、114）。仓库里**没有** LSduR 的 SWS 文本，因此"LSduR 与 PduR 之间如何接线"不要在教程里编造；**R4.x 早期/中期 release（项目 RTA-CAR 若基于它们）通常是 CanIf 直接调用 `PduR_CanIfRxIndication/TxConfirmation/TriggerTransmit`**——这是业界已知形态，但规范文本在本仓库中仅间接出现（PDUR p.16–17 的示例用语），须以项目 release 的 CanIf/PduR SWS 复核。
2. **PduR API 是模板化的**：R25-11 SWS 写作 `PduR_<User:Up>Transmit`、`PduR_<User:Lo>RxIndication`（占位符由配置的模块名替换），"PduR_ComTransmit"、"PduR_CanIfRxIndication" 是实例化后的名字（PDUR p.86、p.89；说明 PDUR p.14 "generic approach"、p.25 按模块拆分头文件 `PduR_CanIf.h`）。
3. `Com_MainFunctionRx/Tx` 在 R25-11 中带 `<shortName>` 后缀（COM p.132–133）。
4. `Det_ReportError` 返回 `Std_ReturnType`（DET p.21）。
5. `CanIf_RxIndication` 参数为 `(const Can_HwType* Mailbox, const PduInfoType*)`（CANIF p.115），旧版（R3.x/早期 4.x）为 `(PduIdType, const uint8*…)` 等不同形式，Arctic Core 追踪时要区分（见 `03-openautosar-trace.md`）。

### 8.5 RTE ↔ Com 映射（inter-ECU S/R，高层）

- RTE 是 Com 的上层使用者（COM p.19 §5.2.1；COM 图 5.1 p.18：RTE 使用 Com 的 `Com_Types`、实现 Com 的 `Rte_Cbk` 回调）。
- 映射由 **System Description 里的 SystemSignal/ISignal 与 SWC 数据元素的 mapping（DataMapping）+ ECUC 中 Com 的 ComSignal/ComSignalGroup symbolic name** 建立：RTE generator 读取这些后生成 `Rte_Write`→`Com_SendSignal`/`Com_SendSignalGroup`、`Rte_Read`→`Com_ReceiveSignal`/`Com_ReceiveSignalGroup`（RTE p.325–326；ECUC 的 symbolic name 机制 `TPS_ECUC_02108`）。
- 复合数据类型：不做 data transformation 时 RTE 把每个原语元素映射到各自 ISignal，`COM` 负责原语类型字节序转换，且 signal 打包由 COM 配置决定（RTE p.325–326）；做 transformation（非 COM-based）时整个复合类型映射到一个 COM signal（`SWS_Rte_08793`，p.325）。
- 本地（同 ECU）通信不经 Com（COM 表 p.26–27 标 "not required, done by the RTE"；VFB p.45：RTE 实现本地连接，只把远端的 port 路由到通信栈）。
- RTE 细节（队列、Rte_Read/Write 变体、Rte_COMCbk 命名等）由 RTE 笔记负责，这里不展开。

---

## 9. 精简回答区（可直接改写进教程）

### (a) MCAL 做什么、什么架构

**MCAL（Microcontroller Abstraction Layer）= BSW 最底层、直接操作 µC 寄存器与片内外设的一组驱动。** 目的是让上层与具体 µC 解耦：实现 µC 相关，**向上接口标准化且 µC 无关**（EXP p.15）。

模块组（EXP p.29）：Microcontroller（MCU、GPT、Watchdog、Core/RAM/Flash Test）、Communication（SPI、I2C、LIN、CAN、FlexRay、Ethernet）、Memory（片内/memory-mapped 存储驱动；R25-11 EXP 已把 Fls/Eep 从文档中移除，p.2）、I/O（ADC、DIO、PORT、PWM、ICU、OCU）、Crypto、Wireless。

结构特征：层内**不允许水平调用**（仅性能原因的配置 notification 例外，EXP p.79）；**矩阵规定它只用 System Services 和硬件，不用其他 BSW 层**（EXP p.80）；调用者是 EcuM（Init）、ECU Abstraction 层（CanIf、IoHwAb、Memory HW Abstraction）与 CDD（EXP p.34、p.80、p.82；IOHWAB p.14–15）；**SWC 不能直接调**。共性 API 形态：`<Mod>_Init(const <Mod>_ConfigType*)`（Dio 除外，由 Port 初始化引脚）、`GetVersionInfo`、DET 开发错误、notification/ISR；寄存器初始化职责：多模块共享的 I/O 寄存器归 PORT，非 I/O 归 MCU，一次性可写与其余寄存器归 start-up code（MCU `SWS_Mcu_00244–00247`）。典型初始化顺序：start-up code → `EcuM_Init` 的 DriverInitZero/One（含 `Mcu_Init`→`Mcu_InitClock`→PLL lock→`Mcu_DistributePllClock`、`Port_Init` 等）→ StartOS → `EcuM_StartupTwo` → BswM 驱动其余初始化。

### (b) Classic AUTOSAR 整体架构

三大层：**Application（SWC）/ RTE / BSW**；BSW 再分 **Services / ECU Abstraction / MCAL / Complex Drivers**，并按 I/O、Memory、Crypto、Communication、Off-board Communication、System 功能组纵向切分（EXP p.12–14、p.20）。RTE 之上是 **component style**（SWC 通过 port 交互，不关心位置），RTE 之下是 **layered style**（EXP p.19）。设计方法学上，应用先在 **VFB** 上与 ECU/网络无关地建模，再部署到 ECU；**每个 ECU 单独生成 RTE（含 BSW Scheduler）**，由 RTE 把 VFB 连接实现为本地调用或经通信栈的网络通信（VFB p.13、p.44–45；EXP p.133）。通信栈：`Com / PduR / (LSduR) / CanIf / CanTrcv / Can`（EXP p.40）；诊断、NvM、WdgM 等位于 Services。配置通过 **ECUC（ECU Configuration Values）** 统一描述，由各模块 generator 生成 `_Cfg/_PBcfg` 等文件（METH p.96、p.103–104；ECUC p.18）。

### (c) SWC 如何与 RTE 交互（高层）

1. SWC = **模型（ports + port-interfaces）+ 实现（runnables 的 C 代码）+ SWC description（含 SwcInternalBehavior：runnables、RTEEvents、数据访问点、ExclusiveArea、PerInstanceMemory、ServiceNeeds…）**（VFB p.59；SWCT p.520–525）。
2. 描述向 RTE **声明需求**：哪些 runnable 周期调用，哪些在通信/mode/触发事件时调用，如何访问 port 数据，需要什么资源（VFB p.59）。
3. RTE **满足需求**：保证 runnable 在正确时机被调用、提供访问数据/调用 operation 的函数（`Rte_Read/Write/Call/...`，如 VFB p.58 例中的 `Rte_Read_SeatSwitch_PassengerDetected()`）、提供其余资源；runnable 在 task 中运行，task 与 OS 调度由 integrator 映射（VFB p.57–59；METH p.42）。
4. 两阶段：**contract phase**（SWC 开发者生成 contract/application header，使 SWC 可独立于 ECU 编译，METH p.307–308）与 **generation phase**（ECU Integrator 生成实际 RTE 源码，METH p.386–387）。
5. SWC 与 BSW 服务的交互也经 RTE：服务端口在 ECU 配置阶段连接，服务 SWC/Proxy 在本地（SWCT p.667；VFB p.89–91）。
RTE 内部实现与 API 细节见 RTE 笔记。

### (d) OEM 是否开发 Classic AUTOSAR 软件 / 谁开发什么

**[规范]**（METH 明文）：
- 文档**不按"OEM / Tier1"划分职责**，而按 role：System Engineer、SWC Designer/Developer、BSW Designer/Module Developer、ECU Integrator 等（METH p.182–189）；一个人/组织可兼多个 role（`TR_METH_01024`，p.30）。
- 唯一明确的组织性陈述：两阶段开发里 primary organization **"usually OEM"** 定义整体系统并交付 System Extract，**"usually suppliers"** 并行定义子系统（`TR_METH_01047`，p.40–41）；序列化/诊断 use case 里 OEM 定义网络表示/数据类型/诊断需求，Tier1/ECU supplier 做集成（p.71–72、p.137–138），但 OEM 在自研应用 SWC 时也可兼任 integrator（`TR_METH_01139`，p.137）。
- 软件角度：SWC 开发、BSW 模块开发、ECU 集成三条线可由不同公司并行（`TR_METH_01110–01112`，p.41–42）；VFB 文档同理强调 µC、ECU、传感器/执行器、应用可由不同专家/公司开发（VFB p.85）。

**[业界实践]**（规范无规定，请在教程中显式标注"典型做法，因项目而异"）：
- OEM 通常**不写 MCAL**，也很少自己做完整 BSW；OEM 通常持有系统/通信配置（ECU Extract、通信矩阵等），常自研或指定部分应用/功能 SWC，供 Tier1 集成；也有 OEM 自研基础软件平台或集中式 ECU 的情况。
- **Tier1** 常做 ECU Integrator：授权 BSW 栈和芯片厂 MCAL、配置/生成/编译链接、写 CDD 与 ECU 专属 SWC（含 IoHwAb 与 Sensor-Actuator SWC）。
- **BSW 供应商 / 芯片厂** 提供 BSW 模块、BSWMD、ECUC 定义、generator 与 MCAL；**工具厂商**提供配置编辑器、RTE Generator、BSW Generator Framework（METH 以 Tool 抽象描述，p.414）。
- 对本项目背景（RTA-CAR 12.9.0 + RTA-OS RH850 GHS port + Renesas P1M MCAL）：这正是上面 BSW 供应商 + 芯片厂 MCAL 的组合，ECU Integrator 做 `Configure MCAL / IO Hardware abstraction / OS / RTE …`（METH p.184）。

---

## 10. 可直接用于章节的"关键 ID 速查"

| 主题 | ID | 位置 |
|---|---|---|
| 层间规则（matrix normative） | —（EXP 无 ID） | EXP p.79–80 |
| Init 仅 EcuM/BswM 可调 | `SWS_BSW_00150`、`00152` | BSWG p.73、p.75 |
| Init 配置指针规则 | `SWS_BSW_00050`、`00212` | BSWG p.74、p.73 |
| MainFunction 命名/限制 | `SWS_BSW_00153/00154/00156/00133/00037/00210` | BSWG p.79、p.43、p.28 |
| DET 开发错误开关/上报 | `SWS_BSW_00042/00045/00243/00255` | BSWG p.56–57、p.55 |
| 运行时错误 | `SWS_BSW_00222` | BSWG p.59 |
| Published info / 版本检查 | `SWS_BSW_00059/00256/00036` | BSWG p.91–92、p.30 |
| GetVersionInfo | `SWS_BSW_00064/00052/00051/00236/00164` | BSWG p.76–77 |
| MemMap 文件/类型/机制 | `SWS_MemMap_00002/00005/00015/00038/00073` | MEMMAP p.19、p.39–40、p.21、p.34 |
| ECUC 定义/值/配置类 | `TPS_ECUC_02005/02065/02097–02100/08012/08034/08035/03016` | ECUC p.34、p.27、p.52、p.35、p.120 |
| SWC 类型/Behavior/Runnable/Event | `TPS_SWCT_01108/01075/01030/01314/01031/01359` | SWCT p.70、p.520、p.525、p.545、p.558、p.601 |
| VFB port/connector/composition | `TR_VFB_00001/00002/00003/00009/00010/00113` | VFB p.17、p.41 |
| METH 两阶段/集成/配置类/RTE contract | `TR_METH_01047/01087/01092/01093/01110–01112/01116` | METH p.40、p.94、p.103、p.107、p.41–42、p.96 |
| Mcu | `SWS_Mcu_00153/00154/00155/00156/00139/00142/00026` | MCU p.25–28 |
| Port | `SWS_Port_00140/00041/00078/00121/00141/00145` | PORT p.24–28 |
| Dio | `SWS_Dio_00001/00061/00102/00133/00134/00190` | DIO p.19、p.14、p.28–34 |
| Adc | `SWS_Adc_00365/91000/00367/00369/00374/00513/00082` | ADC p.52–81 |
| IoHwAb | `SWS_IoHwAb_00001/00025/00038/00039/00044/00078/00119/00158/00061` | IOHWAB p.24、p.20–21、p.16、p.15、p.42 |
| Com TX/RX | `SWS_Com_00197/00198/00432/00123/00124/00398/00399/00300/00301/00305/00625` | COM p.105–133、p.54、p.44、p.106 |
| PduR | `SWS_PduR_00334/00406/00362/00365/00369/00160` | PDUR p.82–90、p.33 |
| CanIf | `SWS_CANIF_00001/00005/00317/00318/00006/00007` | CANIF p.80–115 |
| EcuM 启动 | `SWS_EcuM_02411/02603/02934` | ECUM p.37–42 |

---

## 11. 已知缺口与需要在写作时处理的风险

1. **EXP 对 EcuM 驱动初始化的描述很少**（只有配置指针概念）；完整启动序列须引 ECUM（本笔记 7.5 已补）。
2. **BSWG 没有 `Mod_Cbk.h`**，且 `Mod_Cfg.h` 不是 Table 5.1 的独立类型（见 6.1）。教程提到这些文件名时应写"惯例/视具体 SWS"。
3. **Layer Interaction Matrix 的 PDF 文本列对齐不稳**（EXP p.80）：只引用其底行文字结论（MCAL 行只用 System Services 与硬件）；若要逐格复述，需对照 PDF 图像。
4. **CanIf→（LSduR）→PduR 接线**：见 8.4 第 1 条；R25-11 与 R4.2.x/R19-11/R22-11 不同，须按项目实际 release 复核。
5. **Can driver / Gpt / Wdg / Spi 等 SWS 不在本次文本清单**：写 `Can_Write` 细节要用仓库根目录 R22-11 CAN Driver SWS，并注明 release。
6. **METH 中没有 BSW vendor/tool vendor/Tier1 role**，教程必须把"Tier1=ECU Integrator"之类表述标成业界实践。
7. **P1M-E 约束**（`00-writing-conventions.md`）：讲 `Mcu_InitClock`/PLL 时先通用模型，再声明 P1M-E 无软件可编程 PLL；CAN 为 RS-CANFD 等硬件事实以 `04-rh850-hardware-notes.md` 为准。
