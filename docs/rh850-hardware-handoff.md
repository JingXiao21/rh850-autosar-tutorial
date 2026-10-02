# RH850/P1M-E：供内部 agent 使用的 CAN、诊断通信与 OS 硬件交接

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**（RH850 + AUTOSAR 中文教程），不是公司真实工程；文中“内部 agent / 内部工程 / 交付”等说法只是当时的写作语境，不代表任何真实公司项目、真实配置或指令。正文以下保持原样，仅作溯源；学习请以新教程章节为准。
>
> - **已复审并迁移到**：§2 → [01-rh850/03](01-rh850/03-memory-map.md)、[01-rh850/07](01-rh850/07-clock-system.md)；§3 → [04-can-mcal/04](04-can-mcal/04-can-pin-transceiver.md)；§4–5 → [04-can-mcal/02](04-can-mcal/02-rh850-can-peripheral.md)；§6 → [04-can-mcal/03](04-can-mcal/03-can-clock-bit-timing.md)；§7–9 → [04-can-mcal/07](04-can-mcal/07-hoh-hrh-hth.md)、[10](04-can-mcal/10-can-write-implementation.md)、[11](04-can-mcal/11-can-rx-implementation.md)；§10 → [04-can-mcal/05](04-can-mcal/05-can-interrupt.md)、[01-rh850/06](01-rh850/06-interrupt-exception.md)；§11 → [04-can-mcal/06](04-can-mcal/06-can-controller-init.md)；§12 → [03-mcal/05](03-mcal/05-gpt-driver.md)、[01-rh850/04](01-rh850/04-startup-process.md)；§13 → [04-can-mcal/15](04-can-mcal/15-can-driver-debugging.md)、[08-integration/07](08-integration/07-integration-debugging.md)。
> - **已知更正 / 补充**：① 全文“供内部 agent 使用 / 内部工程须绑定”（§14 等）是旧语境，本仓库是教育项目；② :148 的 `BOM=11` 只是**有条件地**满足 `SWS_Can_00274`（软件须在 128×11 隐性位检测完成前请求 halt，HW-E p.807），无条件满足的是 01/10，见 [04-can-mcal/13](04-can-mcal/13-can-error-busoff.md) §5.3；③ OSTM0/1 归属是配置选择；④ 缺写保护机制说明——P1M-E 无 PROTCMD，时钟控制器靠 Slave Guard（p.471）、复位寄存器靠 P-Bus Guard（p.420）。详见 [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) §3 与 [05-consistency-review-log.md](reference/research/05-consistency-review-log.md)。
> - **行号说明**：本 banner 在第 2 行之后插入了 6 行。其他文档（如研究笔记 01、`analysis-and-learning-plan.md`）引用的本文件行号均指插入前版本，对应当前行号 **+6**。

审核日期：2026-10-01。目标基线为截图中的 **R7F701381，RH850/P1M-E，100 引脚 DPS**。本文件将手册信息展开为配置条件、寄存器、执行顺序和验收方法；内部 agent 不必再解析 PDF 才能理解这些步骤。

**交付状态是“硬件资料与配置方案已审核”，不是“目标板已运行”。** 当前没有内部工程、原理图、实物读回或总线记录。下面有两种标签：**手册事实**可直接用于核对；**参考配置**只有其前提成立才可用于生成配置。待绑定信息见第 14 节，不能用猜测填空。

本文件优先于此前较简略的 CAN/PORT 解说。审核差异见 [hardware-review.md](hardware-review.md)；可直接交给内部 agent 的任务指令见 [internal-agent-task.md](internal-agent-task.md)。地址索引见 [hardware-registers.json](hardware-registers.json) 和 [hardware-registers.csv](hardware-registers.csv)。它们是核对资料，不是可直接顺序执行的寄存器写入脚本。

## 1. 适用资料与器件边界

| 代号 | 文件 / 版本 | 本文使用方式 |
| --- | --- | --- |
| HW-E | [R01UH0585EJ0120 Rev.1.20，2018-03-23](../references/downloads/r01uh0585ej0120.pdf)，3121 页 | P1M-E 硬件主依据；以下页码均为此 PDF 页码，与印刷页码一致 |
| DS-E | [R01DS0505ED0100 Rev.1.00，2025-09-30](../r01ds0505ed0100-rh850p1m-e.pdf)，72 页 | R7F701381 容量、封装、引脚、电气时序 |
| HW-P1x | 用户提供的 R01UH0436EJ0140，2018-03-30 | 旧 P1x/P1M 参考，不能替代 P1M-E 的 RS-CANFD 寄存器章节 |
| DS-C | 用户提供的 R01DS0506ED0100 | P1x-C 对照资料，不能将其 M-CAN 配置移植为本芯片配置 |

HW-E 的 SHA-256：`aaea89a7f5d9b029776945868d21728465d372223c41db05cbd728a0499a6e34`。其他源文件身份见 [source-index.md](source-index.md)。本次重新核对了相关寄存器页，并对引脚复用表等复杂表格进行了页面图像检查。

官方来源：[P1M-E 硬件手册](https://www.renesas.com/en/document/mah/rh850p1m-e-group-users-manual-hardware-rev120)、[P1M-E 数据手册](https://www.renesas.com/en/document/dst/rh850p1m-e-datasheet)、[产品资料与技术更新入口](https://www.renesas.com/en/products/rh850-p1m-e?page=1&recommended=on&tab=documentation)。已检索在线资料；没有取得能证明与本项目器件、MCAL、RTA-OS 版本完全一致且已上板的公开完整工程。不能用 F1KM/U2A/P1x-C 示例的地址补齐本芯片。

## 2. 固定硬件基线与时钟

R7F701381 是 G3M 主核与检查核锁步器件，检查核不是第二个独立 AUTOSAR 应用核；有 FPU。OS 端口、浮点 ABI 和上下文保存仍须与内部工程工具链一致。DS-E pp.1–3。

| 区域 | 起始地址 | 最后一个有效字节 | 容量 |
| --- | --- | --- | --- |
| Code Flash 用户区 | `0x00000000` | `0x000FFFFF` | 1 MiB |
| Code Flash 扩展用户区 | `0x01000000` | `0x01007FFF` | 32 KiB，不能自动并入普通代码区 |
| Data Flash | `0xFF200000` | `0xFF207FFF` | 32 KiB |
| LRAM self | `0xFEDE0000` | `0xFEDFFFFF` | 128 KiB |
| LRAM PE1 别名 | `0xFEBE0000` | `0xFEBFFFFF` | 与 self 是同一 RAM，不能重复分配 |
| GRAM A | `0xFEEF8000` | `0xFEEFFFFF` | 32 KiB |
| GRAM B | `0xFEF00000` | `0xFEF07FFF` | 32 KiB |

来源：HW-E pp.257–258。截图的 `iROM 2048K` 不适用于此 1 MiB 型号。10 KiB 栈候选区为 `[0xFEDFD800, 0xFEE00000)`，地址算术成立；实际栈深度仍需 OS 任务/中断嵌套分析。

时钟树的手册工作基线：Main OSC 16 MHz → PLL → CPU 160 MHz → HSB 80 MHz → LSB 40 MHz；IOSC 8 MHz。CAN 的接口时钟 `pclk=HSB=80 MHz`；CAN 位时钟 `fCAN` 则由 `GCFG.DCS` 决定：0 选 LSB 40 MHz，1 选 Main OSC 16 MHz。二者不能混用。CAN 数据率超过 2 Mbit/s 时不能选 16 MHz 源。来源：HW-E pp.469–471、791。

P1M-E §12 的寄存器表没有其他 RH850 示例常见的可编程 `MOSCE/PLLE/CKSC_CPU` 初始化序列。不要凭其他系列模板写那些地址。由匹配 P1M-E 的启动包与 Mcu 驱动承担芯片启动，再确认实际晶振、选项与测量结果。本文不提供无依据的 PLL 寄存器数值。

可用于板级测频的已确认时钟输出寄存器：`CLKD2DIV=0xFFF88810`、`CLKD2STAT=0xFFF88814`、`CKSC2C=0xFFF89080`、`CKSC2S=0xFFF89088`，均 32 位。CKSC2C[2:0]：3=Main OSC、4=LSB、5=CPU、6=IOSC；分频值 1…1023，0 停止输出。改变源前必须 DIV=0 且 STAT=2；改变分频前等待 STAT.bit1=1。输出应小于 20 MHz，例如已确认的 LSB/4=10 MHz。还必须有可用 EXTCLK0O 引脚及其正确 Port 配置；不能仅写寄存器就声称板上可测。HW-E pp.472–477。

## 3. CAN 引脚：实际 ALT 和 PORT 寄存器

以下是芯片候选功能，不是板卡布线断言。内部 agent 必须从原理图选择一组真实连接到收发器的引脚，并确保同一 CAN RX 功能没有同时在多个引脚使能。来源：DS-E p.23；HW-E pp.131、151–154。

| 控制器 | RX 候选 | RX ALT | TX 候选 | TX ALT |
| --- | --- | --- | --- | --- |
| CAN0 | P2_0 | 1 | P2_1 | 1 |
| CAN0 | P3_7 | 3 | P3_8 | 3 |
| CAN0 | P4_5 | 3 | P4_6 | 3 |
| CAN1 | P2_2 | 1 | P2_3 | 1 |
| CAN1 | P3_12 | 3 | P3_13 | 3 |
| CAN1 | P4_2 | 1 | P4_3 | 1 |
| CAN2 | P5_6 | 1 | P5_5 | **6** |

100 引脚 R7F701381 不能套用 144 引脚配置中的 CAN2 TX=P5_7。CAN2 的 RX/TX ALT 不对称，不能同时写成 ALT1。

PORT 基址 `P=0xFFC10000`，端口组 n 的步长 `0x40`。以下宽度是本文规定的访问宽度，不是允许用 C 位域随意读改写。HW-E pp.99–125。

| 寄存器 | 地址公式 | 宽度 | 本任务用法 |
| --- | --- | --- | --- |
| Pn | P+0x0000+0x40n | 16 | GPIO 输出数据 |
| PSRn | P+0x0004+0x40n | 32 | 高 16 位为写使能、低 16 位为写值，用于 GPIO 原子位更新 |
| PPRn | P+0x000C+0x40n | 16 | 引脚输入读回 |
| PMn | P+0x0010+0x40n | 16 | RX=1 输入；TX=0 输出 |
| PMCn | P+0x0014+0x40n | 16 | CAN 功能=1；GPIO=0 |
| PFCn / PFCEn / PFCAEn | P+0x0018 / 0x001C / 0x0028+0x40n | 16 | ALT 编码，见下文 |
| PMSRn / PMCSRn | P+0x0020 / 0x0024+0x40n | 32 | 高半字掩码、低半字数据，分别更新 PM/PMC |
| PINVn | P+0x0030+0x40n | **32** | CAN 输出不额外反相 |
| PIBCn / PBDCn / PIPCn | P+0x4000 / 0x4004 / 0x4008+0x40n | 16 | 此 CAN 路径相关位设 0 |
| PUn / PDn | P+0x400C / 0x4010+0x40n | 16 | 按收发器、板级电气需求选择 |
| PODCn / PDSCn | P+0x4014 / 0x4018+0x40n | **32** | 输出类型、驱动能力须与板级条件匹配 |
| PUCCn / PODCEn | P+0x4028 / 0x403C+0x40n | **32** | 输出缓冲电气设置 |
| PISAn | P+0x402C+0x40n | 16 | 输入缓冲选择；只对实际实现的组/位使用，不能按公式假设有 PISA5 |

ALT 的 `[PFCAE, PFCE, PFC]` 位编码：ALT1=`000`、ALT3=`010`、ALT6=`101`。每个字段只修改目标 pin 对应 bit，保留其他已分配引脚。

输出缓冲 `[PUCC,PDSC]`：00=SLOW、01=FAST、10/11=MIDDLE；PISA：0=Type1 Schmitt、1=Type2 CMOS。前者按 32 位访问，PISA 按 16 位访问，即使有效 pin 位都在低半字也不能统一使用 16 位写。各组可用位以对应引脚能力为准，保留位不作普通 pin 修改。HW-E pp.116–123。

输出电路 `[PODCE,PODC]`：00/10=push-pull、01=N-channel open-drain、11=P-channel open-drain；普通 CAN TX 到收发器 TXD 的候选是 push-pull，但仍须满足收发器输入电气要求。PU/PD 对应位1分别启用内部上拉/下拉、0关闭，不同时启用；PINV 对应位0为不反相。上述值只作用于已确认支持该功能的 pin，不能整组覆盖其他功能引脚。

**PIPC 对 CAN 必须为 0。** HW-E p.131 只允许表列的 CSIH/CSIG/TSG 等功能使用 PIPC=1；CAN 不在允许清单中。PIPC=0 时由 PM 决定方向，不是“交给外设自动控制方向”。

引脚初始化顺序（控制器尚未通信、收发器处于板级允许的安全状态）：相关 PBDC=0、PIBC=0、PM=1、PMC=0、PIPC=0 → 设置电气参数与 ALT → PMC=1 → 仅 TX 的 PM=0，RX 保持 PM=1。输入复用通过 PMC 接入外设，不需为此把 PIBC 改成 1。P2/P3/P4 的部分 RX 与 INTP 共用管脚，初始化时也要避免误使能外部中断。依据 HW-E pp.126–131。

若板卡确认采用 CAN0 P2_0/P2_1：PM2=`0xFFC10090`（bit0=1、bit1=0）；PMC2=`0xFFC10094`（bit0/1=1）；PFC2=`0xFFC10098`、PFCE2=`0xFFC1009C`、PFCAE2=`0xFFC100A8`（这两 pin 的位均为 0）；PIPC2=`0xFFC14088`（bit0/1=0）。这些是**局部位期望**，不是整寄存器覆盖值。

MCU 的 CAN TX/RX 必须经合适收发器连接 CANH/CANL。STB/EN/ERR/WAKE 的 GPIO、有效电平、供电、电平兼容和终端电阻属于原理图/收发器资料，RH850 手册不能推导。DS-E p.57 的 RS-CANFD 时序以 fast/middle 输出缓冲模式为条件，不能任意选择 Port 输出速度后继续套用该时序。

## 4. 两套 CAN 接口必须选定一套

唯一 RS-CANFD 单元基址 `B=0xFFD20000`，其内部有 CAN0/1/2。不是为每个控制器另找一个单元基址。通道号 m=0…2。

`GRMCFG=0xFFD204FC`，32 位，bit0 `RCMC`：0=Classical CAN 接口，1=CAN FD 接口；`CANFDMDR=0xFFD28000` bit0 是只读模式确认。GRMCFG 只能在 global reset 中改变，并先于其余 CAN 配置写入。已经运行后的模式切换还要恢复旧模式专属寄存器的复位值，不能套用冷启动步骤直接切换。HW-E pp.796、920、1056、1122。

| 项目 | 路线 A：Classical CAN | 路线 B：CAN FD 接口 |
| --- | --- | --- |
| RCMC | 0 | 1 |
| 总线上使用 | Classical CAN，至多 8 B | 可发送 Classical 与 FD 帧；由每帧 FDF/BRS 决定 |
| 通道位时间 | CFG | NCFG、DCFG，位域不同 |
| 过滤规则窗口 | B+0x0500 | B+0x1000 |
| Rx FIFO 0 头部 | B+0x0E00 | B+0x3000 |
| Tx buffer 0 头部 | B+0x1000 | B+0x4000 |
| 本文参考用途 | 先验证 500k Classical 诊断链路 | 截图 500k/1M、16 B FD 链路 |

典型严重错误：在 FD 模式把 B+0x0500 当过滤器写，会覆盖 CAN0 DCFG；在 Classical 模式把 B+0x1000 当过滤器写，会覆盖 Tx buffer 0。**基址相同不等于寄存器布局相同。** AUTOSAR Classic 平台名称也不等于必须选择 Classical CAN 总线格式。

## 5. CAN 核心寄存器与位定义

以下表格中除特别注明的 8 位 TMC/TMSTS 外，使用 32 位对齐访问。保留位按手册复位值写入；不同模式的额外字段不可混用。标准名称省略 `RSCFD0CFD` / `RSCAN0` 前缀，以模式和地址消除歧义。

| 寄存器 | 地址 | 关键字段 / 用法 | HW-E 页 |
| --- | --- | --- | --- |
| CmCFG / CmNCFG | B+0x0000+0x10m | 对应模式的位时间 | 803–804、921–922 |
| CmCTR | B+0x0004+0x10m | CHMDC[1:0]、CSLPR[2]、中断使能、bus-off 模式 | 805–807、923–925 |
| CmSTS | B+0x0008+0x10m | 模式、bus-off、REC/TEC | 808–809、928–929 |
| CmERFL | B+0x000C+0x10m | 错误标志，写 0 清除 | 810–813、930–934 |
| GCFG | `0xFFD20084` | DCS[4] 时钟选择；TPRI[0]；DCE[1]；DRE[2]；MME[3]；FD 的 CMPOC[5] | 949–950 |
| GCTR | `0xFFD20088` | GMDC[1:0]，GSLPR[2]；DEIE[8]、MEIE[9]、THLEIE[10]；FD 的 CMPOFIE[11] | 951–953 |
| GSTS | `0xFFD2008C` | GRSTSTS[0]、GHLTSTS[1]、GSLPSTS[2]、GRAMINIT[3] | 954–955 |
| GERFL | `0xFFD20090` | DEF[0]、MES[1]、THLES[2]；FD 的 CMPOF[3]、EEFm[16+m] | 956–957 |
| GAFLECTR | `0xFFD20098` | AFLPN[4:0]、AFLDAE[8] | 963 |
| GAFLCFG0 | `0xFFD2009C` | RNC0[31:24]、RNC1[23:16]、RNC2[15:8] | 964 |
| RMNB | `0xFFD200A4` | NRXMB；FIFO-only 参考配置写 0 | 972 |
| RFCCx | B+0x00B8+4x，x=0…7 | FIFO 配置，见第 8 节 | 979–980 |
| RFSTSx | B+0x00D8+4x | FIFO 状态、W0C 标志 | 981–982 |
| RFPCTRx | B+0x00F8+4x | 写低 8 位 0xFF 弹出当前 FIFO 头 | 983 |
| CFCCk | B+0x0118+4k，k=0…8 | 通道收发共用 FIFO；本参考配置全部关闭 | 990–994 |
| TMCp | B+0x0250+p，p=0…47 | **8 位**；TMTR[0]、TMTAR[1]、TMOM[2] | 1018–1019 |
| TMSTSp | B+0x02D0+p | **8 位**；TMTSTS[0]、TMTRF[2:1]、TMTRM[3]、TMTARM[4] | 1020–1021 |
| TMIECy | B+0x0390+4y，y=0,1 | 每 bit 对应发送 buffer；y=1 仅低 16 位有效 | 1036–1037 |
| TXQCCm | B+0x03A0+4m | 本参考配置不使用 Tx queue，写 0 | 1039–1040 |
| THLCCm | B+0x0400+4m | 本参考配置不使用 Tx history，写 0 | 1044–1045 |
| GFDCFG | `0xFFD20474` | FD 全局设置；参考保留复位设置 | 962 |
| GRMCFG | `0xFFD204FC` | RCMC[0] | 920 |
| CmDCFG | B+0x0500+0x20m | **仅 FD** 数据位时间 | 935–936 |
| CmFDCFG | B+0x0504+0x20m | **仅 FD** TDCOC[8]、TDCE[9]、ESIC[10]、TDCO[22:16]、TMME[27]、FDOE[28] 等 | 938–940 |
| CANFDMDR | `0xFFD28000` | 只读 FDMDR[0] | 1056 |

### 模式、错误和清标志约束

GCTR：GMDC=00 operating、01 reset、10 test，11 禁止；GSLPR=1 是 global stop。复位 GCTR=5，GSTS 在 RAM 初始化结束前常见低四位 D；等待 GRAMINIT=0 后，清 GSLPR 并保留 GMDC=01，等待 GSTS[2:0]=001。退出 reset 时设 GMDC=00，等待 GSTS[2:0]=000。模式写入不是同步完成，必须有读回和超时。

CmCTR：CHMDC=00 communication、01 reset、10 halt，11 禁止；CSLPR[2]=1 为 channel stop。先清 CSLPR 并保持 CHMDC=01，等待 CmSTS[2:0]=001。完成配置后设 CHMDC=00，等待低三位为 0。CmSTS.COMSTS[7] 在总线满足 11 个连续 recessive 位后反映通信状态；只看到 mode bits=0 不能证明已经正确接入总线。

CmCTR 中断位：BEIE8、EWIE9、EPIE10、BOEIE11、BORIE12、OLIE13、BLIE14、ALIE15、TAIE16；FD 还有 EOCOIE17、SOCOIE18、TDCVFIE19。BOM[22:21]：00 为手册 ISO11898-1 自动恢复模式，01 为 bus-off 进入时 halt，10 为恢复序列结束时 halt，11 为处于 bus-off 时请求 halt。**MCAL/CanSM 的 bus-off 恢复策略必须与 BOM 一致**；不得为消除报错反复重启控制器。普通通信 CTME[24]=0；环回/监听的 CTMS[26:25] 修改要遵守 halt 条件，不在正常配置中遗留测试位。

CmSTS：EPSTS[3]、BOSTS[4]、TRMSTS[5]、RECSTS[6]、COMSTS[7]；REC=[23:16]，TEC=[31:24]。FD 的 ESIF[8] 是写 0 清标志。

CmERFL 标志：BEF0、EWF1、EPF2、BOEF3、BORF4、OVLF5、BLF6、ALF7、SERR8、FERR9、AERR10、CERR11、B1ERR12、B0ERR13、ADERR14，均为写 0 清除的标志位。先记录状态，再对确需清除的标志写 0、其他已定义 W0C 标志写 1，保留位写 0。例如仅清 AERR 用 `0x00007FFF & ~(1u<<10)`。不要使用普通 `reg &= ~mask` 模板：并发到来的其他事件可能被读改写丢掉。相同事件重复发生可合并为一个硬件标志，软件不能把标志当完整计数。

GERFL 的 MES/THLES 是汇总状态，应清其 RFMLT/CFMLT/THLELT 等源标志；不是向汇总位写值就能消除中断。FD 的 EEFm 表示 CAN RAM 相关 ECC 错误，不能只清掉后继续假定数据完整。部分保留位读值未定义，读回比较必须使用有效位掩码，不能要求整个 GERFL 恒等于 0。

## 6. 位时间：物理量、编码、可复算候选

`bitrate=fCAN/[divider×(1+TSEG1+TSEG2)]`，采样点 `(1+TSEG1)/(1+TSEG1+TSEG2)`。divider、TSEG1/2、SJW 都是物理量；寄存器填物理量减 1。

| 路线 / 阶段 | fCAN | divider | TSEG1 | TSEG2 | SJW | 位率 / 采样点 | 寄存器值 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A：Classical | 40 MHz | 4 | 15 | 4 | 3 | 500 kbit/s / 80% | CFG=`0x023E0003` |
| B：FD nominal | 40 MHz | 2 | 31 | 8 | 4 | 500 kbit/s / 80% | NCFG=`0x071E1801` |
| B：FD data | 40 MHz | 2 | 15 | 4 | 3 | 1 Mbit/s / 80% | DCFG=`0x023E0001` |

路线 A CFG：BRP[9:0]，TSEG1[19:16]，TSEG2[22:20]，SJW[25:24]。TSEG1=4…16，TSEG2=2…8，SJW=1…4。旧 FD 候选的 TSEG1=31 不能塞进 Classical CFG。

路线 B NCFG：NBRP[9:0]，NSJW[15:11]，NTSEG1[22:16]，NTSEG2[28:24]。DCFG：DBRP[7:0]，DTSEG1[19:16]，DTSEG2[22:20]，DSJW[26:24]。NBRP 与 DBRP 必须相同；TDCE=1 时两者编码须 ≤1，即 divider≤2。来源：HW-E pp.803–804、921–922、935–940、1092。

手册寄存器文字允许 SJW≤TSEG2，而 Figure 17.17 要求 TSEG1>TSEG2>SJW；本参考采用更严格关系，因此 data SJW=3。不能反过来声称截图 SJW=4 一定无法工作；该边界差异需适用技术更新/厂商说明关闭。

**TDC 不能只看 prescaler 就宣布配置完成。** FDCFG.TDCE[9] 使能补偿，TDCOC[8] 选择方式，TDCO[22:16] 的字段单位是 fCAN 周期，物理 offset 为字段值+1 个周期，不是直接填写数据段 Tq 数。需结合收发器往返延时、布线、采样点与手册 p.1099 配置。本文不给出未经板级验证的“必可运行”TDCO 常数。先用路线 A 完成物理链路验证，再根据网络要求完成路线 B 的 TDC/传播延时确认。

供内部 agent 计算 TDC 的具体规则：TDCOC=0 使用硬件测得的延时加 offset；TDCOC=1 只用 offset，不能在不知道往返延时时随意选择。若以数据段正常采样位置作为 offset，按 p.940 的编码公式推导 `offset_cycles=(1+DTSEG1)×divider`，`TDCO=offset_cycles−1`，并检查 7 位范围。此例为 32 个 fCAN 周期，即 800 ns、字段31；它是单位换算示例，不是已确认的板级 SSP。p.940/p.1099 对 Tq 量化使用了不同舍入措辞，最终 SSP 及测得延时须结合供应商说明与实测确认。组合值的 TDC 部分为 `(TDCO<<16)|(TDCE<<9)|(TDCOC<<8)`；其余 FDCFG 字段另行按功能设置。修改 TDCO 时通道须处于 reset/halt，TDCE 与相关配置遵守手册模式限制。

FDCFG.FDOE[28]=1 表示 FD-only，对 Classical 帧有排斥行为；需要接收 Classical 诊断帧时必须为 0。**RCMC=1 不意味着 FDOE=1。** 普通 16 B 发送不需 TMME，设 TMME[27]=0。无网关需求时不使能网关字段。手册 p.794 说明 ISO11898-1 与可选 FD 帧，但本次资料没有给出可据以替换所有测试仪“ISO/non-ISO FD”选项的完整版本判定；不得凭名称编造一个 GFDCFG ISO 切换位。

另外，HW-E p.1122 对“FD 接口中只使用 Classical 帧”要求 DCFG 与 NCFG 同值，但两者字段布局不同；不能把该句机械实现为复制 32 位原始值。本文路线 A 避开该歧义，路线 B 面向实际 FD 网络。若内部 MCAL 必须采用 FD 接口承载纯 Classical 网络，应依据供应商实现/澄清关闭这一条件，不能直接套路线 B 的不同 nominal/data 速率。

## 7. 接收规则：地址、mask、分页、7 个 ID 示例

每条规则四个 32 位字，j=页内索引 0…15：

| 字段 | 路线 A 地址 | 路线 B 地址 |
| --- | --- | --- |
| GAFLIDj | B+0x0500+0x10j | B+0x1000+0x10j |
| GAFLMj | B+0x0504+0x10j | B+0x1004+0x10j |
| GAFLP0j | B+0x0508+0x10j | B+0x1008+0x10j |
| GAFLP1j | B+0x050C+0x10j | B+0x100C+0x10j |

来源：HW-E Classical 寄存器表 pp.797–802；FD pp.963–971；规则配置 pp.1095–1096。

GAFLID：IDE[31]，RTR[30]，GAFLLB[29]（0=外部收到，1=自身发送镜像），ID[28:0]。标准 ID 放低 11 位，不左移。GAFLM：IDEM[31]、RTRM[30]、IDM[28:0]，1=比较，0=忽略；bit29 保留。IDEM=0 时手册另要求所有 IDM=0，不可直接构造“任意格式但精确低 11 位”的掩码。

所以**标准数据帧精确匹配**：`GAFLID=id & 0x7FF`、`GAFLM=0xC00007FF`。这同时比较 IDE=0 与 RTR=0。全零 mask 是通配，不是精确匹配；MCAL 配置项是否做反相必须看生成代码。

GAFLP0：GAFLDLC[31:28] 为最小 DLC 条件，0 禁用该规则的 DLC 检查；不是“只能接收恰好这个字节数”。GAFLPTR[27:16] 是 12 位标签；RMV[15] 控制是否写 Rx buffer；RMDP[14:8] 指定 Rx buffer。FIFO-only 时 RMV=0。GAFLP1 bit0…7 对应 Rx FIFO0…7，bit8…16 对应共用 FIFO0…8；单写 `1` 将报文路由到 Rx FIFO0。

规则总数最多 192，单通道最多 128，窗口每页 16 条、共 12 页。先在 GAFLCFG0 填各通道规则数量，规则按通道顺序连续分配；第 r 条的页号 r/16、页内 j=r%16。不能把第 16 条直接写到窗口后面。全局 reset 下通过 GAFLECTR.AFLDAE=1 开启写入，用 AFLPN 选页；写完关闭 AFLDAE。

**参考配置：只有 CAN0，截图 7 个标准数据 ID 全部进 FIFO0。** ID 列表：`0x4E0, 0x7DF, 0x014, 0x0FD, 0x3F2, 0x4F0, 0x07B`。GAFLCFG0=`0x07000000`；GAFLECTR=`0x00000100` 选第 0 页并允许写；j=0…6 的 GAFLID 为上述 ID，GAFLM 全部 `0xC00007FF`，GAFLP0=`j<<16`，GAFLP1=`1`；完成后 GAFLECTR=`0`。若只想验收一个诊断 ID，可以将规则数改成 1，并同步只写该条。

这 7 个值只来自截图，不构成已确认诊断寻址规范。特别是物理请求 ID、响应 ID、功能请求是否用 0x7DF，都必须由项目网络配置确认。Rx 规则数、MCAL HOH 数、Tx mailbox 数是三个不同概念。

## 8. Rx FIFO0：容量、数据读取、弹出与中断

RFCCx：RFE[0] 使能，RFIE[1] 中断使能，FD 的 RFPLS[6:4] 为 payload 存储编码，RFDC[10:8] 为深度编码，RFIM[12] 为中断模式，RFIGCV[15:13] 为阈值。

RFPLS 编码 0…7 对应 `8,12,16,20,24,32,48,64` 字节；RFDC 编码 0…7 对应 `0,4,8,16,32,48,64,128` 条。RFIM=1 为每收到一条产生请求；RFIM=0 为阈值模式。Classical 模式的 RFPLS 位是保留位，不得写 FD payload 编码。HW-E pp.979–983。

| 配置 | global reset 阶段 RFCC0，RFE=0 | global operating 后单独使能 RFE |
| --- | --- | --- |
| A：8 B，8 条，轮询 | `0x00001200` | `0x00001201` |
| A：8 B，8 条，每帧 IRQ | `0x00001202` | `0x00001203` |
| B：16 B，8 条，轮询 | `0x00001220` | `0x00001221` |
| B：16 B，8 条，每帧 IRQ | `0x00001222` | `0x00001223` |

**RFE=1 必须在 global operating 后另一次写入**，不能把 reset 阶段配置与使能合并。参考只开 FIFO0，其他 FIFO、共用 FIFO、Rx buffer、Tx queue/history 均保持关闭。FD 16 B FIFO 若收到更长报文，容量策略必须明确：本参考 GCFG.CMPOC=0，超出时丢弃，不静默截断给 CanTp。需要收 64 B 时将 RFPLS 改为 7，并同步调整软件缓冲、CanIf/CanTp 与容量核算。

容量核算：Classical `NRXMB+ΣRF depth+ΣCF depth≤192`；FD `NRXMB×(12+Rx-buffer payload)+Σdepth×(12+FIFO payload)≤5376 bytes`。本例 FD FIFO0=8×(12+16)=224 B；Classical 8×16=128 B。48 个固定 Tx buffer 另计。不能把“13 个邮箱”直接代入总 RAM 字节数。HW-E p.1097。

RFSTS0=`0xFFD200D8`：RFEMP[0] 空、RFFLL[1] 满、RFMLT[2] 丢帧、RFIF[3] 请求，RFMC[15:8] 当前条数。RFMLT/RFIF 都是 W0C；仅清 RFIF 的 32 位写值为 `0x00000004`，仅清 RFMLT 为 `0x00000008`；先记录错误再清。读取 RFSTS 不弹出数据。

FIFO x 读取窗口：

| 数据 | 路线 A | 路线 B |
| --- | --- | --- |
| RFIDx | B+0x0E00+0x10x | B+0x3000+0x80x |
| RFPTRx | B+0x0E04+0x10x | B+0x3004+0x80x |
| RFFDSTSx | 无 | B+0x3008+0x80x |
| RFDFd_x | B+0x0E08+0x10x+4d，d=0,1 | B+0x300C+0x80x+4d，d=0…15 |

RFID 的 IDE/RTR/ID 布局与 GAFLID 的对应字段一致，但不能将接收头的其他字段当发送控制位。RFPTR：DLC[31:28]、rule label[27:16]、timestamp[15:0]。FD 状态 FDF[2]、BRS[1]、ESI[0]。数据按每个 32 位寄存器低字节在先展开：byte0=word&255，byte1=(word>>8)&255，依次类推。FD DLC 表：0…8→0…8 B，9→12，10→16，11→20，12→24，13→32，14→48，15→64；Classical DLC 大于 8 不表示可以读取 12/16 B。

接收算法：检查 RFEMP=0 → 读并保存全部头部 → 按帧格式、DLC 和已分配 payload 上限校验长度 → 复制数据到软件缓冲 → 向 `RFPCTR0=0xFFD200F8` 写 32 位 `0xFF` → 再处理下一条。RFE=1 且非空时才可弹出；DMA 消费与 CPU 弹出不能同时拥有同一 FIFO。不要在空/未分配窗口盲读。

ISR/轮询必须使用同一个受控消费路径。ISR 清 RFIF 后再次检查 FIFO 与源标志，处理“清标志瞬间到帧”的竞争；中断有处理预算时，剩余消息必须通过可靠的延后执行继续处理，不能清掉最后通知后把报文留在 FIFO。溢出时记录 RFMLT 与当时软件负载，不把丢帧伪装成成功接收。

## 9. Tx buffer：地址、16 B FD、完成判定

每通道 16 个固定 Tx buffer，全局 p=`16*m+local`。CAN0 p=0…15，CAN1 p=16…31，CAN2 p=32…47。不能把 CAN1 的 local0 当成全局 p0。

| 数据 | 路线 A | 路线 B |
| --- | --- | --- |
| TMIDp | B+0x1000+0x10p | B+0x4000+0x20p |
| TMPTRp | B+0x1004+0x10p | B+0x4004+0x20p |
| TMFDCTRp | 无 | B+0x4008+0x20p |
| TMDFd_p | B+0x1008+0x10p+4d，d=0,1 | B+0x400C+0x20p+4d，d=0…4 |

TMID：IDE[31]，RTR[30]，THLEN[29]，ID[28:0]；标准数据帧无 history 时直接填低 11 位 ID。TMPTR：DLC[31:28]、8 位 label[23:16]，不是 Rx 的 12 位 label。FD TMFDCTR：FDF[2]、BRS[1]、ESI[0]。ESI 策略还受 FDCFG.ESIC 影响，不能任意伪造节点错误状态。

**CAN0 buffer0，16 B FD+BRS 参考：** TMID=`0xFFD24000` 填网络定义的响应 ID；TMPTR=`0xFFD24004` 填 `0xA0000000`（DLC10）；TMFDCTR=`0xFFD24008` 填 `0x00000006`（FDF/BRS=1，ESI 候选0）；payload 的 4 个 32 位字写 `0xFFD2400C/4010/4014/4018`。完成写入并满足驱动所需 I/O 顺序后，对 **8 位** `TMC0=0xFFD20250` 写 `1` 请求发送。Classical 8 B 则使用另一套窗口、TMPTR DLC8=`0x80000000`，没有 TMFDCTR 写入。

发送前检查 `TMSTS0=0xFFD202D0`：TMTRM[3]=0，旧 TMTRF[2:1] 结果已消费并清除。仅在软件独占该 buffer 时填数据，禁止发送请求尚存时覆盖 payload。TMC 的 TMTR/TMTAR 是写 1 请求、写 0 不取消请求；不能通过写 TMC=0 取消发送。

TMTRF 解码：00=尚无完成结果；01=取消完成；10=发送成功且无取消请求；11=取消请求与发送竞争但发送成功。只有 10/11 才产生成功 TxConfirmation，01 走取消/失败语义。TMTRM=0 本身不等于成功。记录并交接结果后，按手册对 TMSTS **8 位写 0** 清除结果；不能把读出的活动状态位原样回写。

若启用 buffer0 完成中断，TMIEC0=`0xFFD20390` bit0=1；中断由 CAN0 TRX（EI185）路由。ISR 须处理所有已启用的发送源，不能只清 EIC 请求。软件的 PDU handle 与 p buffer 的映射必须保存到完成时再交给上层。

普通 FD Tx buffer 最多存 20 B，16 B 不需 merge。>20 B 时 merge 只允许从每通道 local0 或 local3 发出：local1/2 或 local4/5 被占用为 payload 存储，且其中 ID/PTR/FDCTR 地址也改作数据；不能任意挑三个邮箱拼接。Tx queue、共用 FIFO 所关联的发送 buffer 也会占用固定资源。64 B 的详细物理布局在 HW-E pp.1107–1108，当前方案明确不启用，因此不可自行把 DLC 改成 15 就发送 64 B。

## 10. 中断到 AUTOSAR OS 的硬件交接

EICn 地址：n=0…31 为 `0xFFFEEA00+2n`；n≥32 为 `0xFFFFB000+2n`，只访问已定义通道。EIC 是 16 位寄存器；具体设置必须使用 OS/MCAL 的受控路径。来源：HW-E pp.267–268、283、285–286。

| 硬件源 | EI 通道 | EIC 地址 | 表参考偏移 4n |
| --- | --- | --- | --- |
| OSTM0 | 74 | `0xFFFFB094` | `0x128` |
| OSTM1 | 75 | `0xFFFFB096` | `0x12C` |
| CAN0 error | 183 | `0xFFFFB16E` | `0x2DC` |
| CAN0 common Rx FIFO | 184 | `0xFFFFB170` | `0x2E0` |
| CAN0 Tx | 185 | `0xFFFFB172` | `0x2E4` |
| CAN1 error | 186 | `0xFFFFB174` | `0x2E8` |
| CAN1 common Rx FIFO | 187 | `0xFFFFB176` | `0x2EC` |
| CAN1 Tx | 188 | `0xFFFFB178` | `0x2F0` |
| CAN global error | 189 | `0xFFFFB17A` | `0x2F4` |
| **CAN 全局 Rx FIFO0…7** | **190** | **`0xFFFFB17C`** | **`0x2F8`** |
| CAN2 error | 191 | `0xFFFFB17E` | `0x2FC` |
| CAN2 common Rx FIFO | 192 | `0xFFFFB180` | `0x300` |
| CAN2 Tx | 193 | `0xFFFFB182` | `0x304` |

**本参考 FIFO0 的接收中断是 EI190，不是 EI184。** EI184 对应 CAN0 的 common Rx FIFO，是另一组寄存器/资源。若只配置 CAN0 ERR/REC/TRX 三个 ISR，本方案的 Rx FIFO 消息可能永远到不了 CanIf。

EIC 位：EIP[3:0]，0 最高、15 最低；EITB[6]，0 直接分支、1 表参考；EIMK[7]，1 屏蔽；EIRF[12]；EICT[15] 只读源类型。CAN 上表各源为高电平中断（EICT=1），EIRF 只读，必须清 CAN 外设中的相应源。OSTM0/1 为同步边沿源，可按手册处理 EIRF；不要把 CAN 清中断方法复制给 OSTM。

EIMK 只屏蔽向 CPU 的请求，不保证 EIRF 不置位。EIC 整寄存器读改写存在与硬件更新竞争的风险；bit 指令也不能自动保证不会覆盖其他同时变化的位。由已验证的 OS/MCAL 端口处理屏蔽、优先级和 pending 管理，不能在 ISR 中无差别写 EIC=0。

直接分支偏移由 RINT/优先级决定；表参考偏移 4n 是硬件表项相对 INTBP 的偏移。截图 `Interrupt_0228/022C` 不能仅凭名字改成 `Interrupt_0128/012C`：内部 agent 应核查 RTA-OS RH850GHS 5.0.39 生成的中断表、符号和端口说明。OsIsrPriority 到 EIP 的映射、Category 1/2 包装、OS 栈与寄存器/FPU 保存属于端口契约，不能根据这个地址表自行造 ABI。

上电初始保持 CPU/控制器相应中断受控；先装好合法向量和 ISR、清理已处理的外设源，再开启外设中断与 EIC，最后按 OS 启动顺序开放 CPU 中断。不能在 OS 尚未建立上下文时进入生成的 Category 2 ISR。

## 11. 可执行的冷启动配置顺序与读回点

前提：确认芯片与时钟；单一驱动拥有整个 CAN 单元；以下是从芯片复位开始的参考过程。若 MCAL 已存在，应修改其配置/生成结果并通过驱动执行；不要由应用代码与 MCAL 同时写寄存器。多通道共用 GCFG、AFL、FIFO RAM，重新初始化全局会影响其他通道。

1. 完成启动、RAM/ECC 与 OS 早期资源准备；选择板级 Port 和收发器模式。暂时屏蔽相关 CAN 中断。所有等待均使用有依据的单调超时，超时后记录寄存器并退出，不无限死循环。
2. 读取 GSTS，等待 GRAMINIT[3]=0。硬件 RAM 初始化为 3794 个 pclk 周期，80 MHz 下约 47.425 μs；此数用于理解，实际仍按标志判断。HW-E p.1090。
3. GCTR 清 GSLPR、GMDC=01，使 global reset；等待 GSTS[2:0]=1。初次冷启动可写 GCTR=1，此后不要覆盖已建立的无关配置。
4. GRMCFG 写路线 A 的 0 或 B 的 1；读 CANFDMDR.bit0 确认。然后对使用的通道清 CSLPR、CHMDC=01，等待 CmSTS[2:0]=1。未使用通道保持禁止通信状态。
5. GCFG 配置 DCS=0，使用 40 MHz。本参考 TPRI=0（ID 优先），不启用 DLC 替换/镜像，FD 溢出丢弃策略 CMPOC=0，即候选 GCFG=0。
6. 写通道 CFG 或 NCFG/DCFG，读回有效位并按第 6 节反算。不能混用模式，也不能只核对工具 GUI 显示的波特率。
7. 在 global reset 下写 GAFLCFG0、分页 AFL 规则；关闭 AFLDAE。设 RMNB=0，RFCC0 为第 8 节 RFE=0 值；未分配 RFCC/CFCC/TXQCC/THLCC 保持复位关闭。完成内存资源核算。
8. FD 路线设 FDCFG，明确 FDOE=0、TMME=0，按板级时序确定 TDC。先用轮询 bring-up 时 RFIE/TMIEC/错误 IRQ 可保持关闭，由软件检查所有错误；转中断时在合法配置模式中设置相应使能。
9. 建立/核查硬件中断映射、OS ISR、错误记录与完成回调；根据 CanSM 策略设 BOM。禁止在此直接猜测 OsIsrPriority 数值。
10. 将 GCTR.GMDC 改为 00；等待 GSTS[2:0]=0。**此后单独设置 RFCC0.RFE=1**，读回。
11. 将使用通道 CmCTR.CHMDC 改为 00；等待 CmSTS[2:0]=0。板级收发器进入 Normal，并在实际总线上检查 COMSTS、错误计数和 recessive 状态；具体 EN/STB 时序服从收发器要求。
12. 先发送一帧并等 TMTRF 成功，再用外部节点发匹配帧检查 FIFO 接收。轮询通过后再打开已配置的 IRQ 路线，证明 ISR 能完整消费源；最后接入内部工程的 CanIf/CanTp/DCM 路由。

路线 A/CAN0 的预期关键读回：GRMCFG.bit0=0；CANFDMDR.bit0=0；GCFG.DCS=0；CFG0=`0x023E0003`；GAFLCFG0=`0x07000000`；GAFLECTR.AFLDAE=0；RFCC0 为 `0x1201`（轮询）或 `0x1203`（IRQ）；GSTS[3:0]=0；C0STS[2:0]=0；正常空闲时 FIFO RFEMP=1、无丢帧。动态寄存器仅按相关位判断。

路线 B 对应关键差异：模式位=1；NCFG0=`0x071E1801`、DCFG0=`0x023E0001`；RFCC0=`0x1221` 或 `0x1223`；FDCFG 按经确认的 TDC 方案检查且 FDOE/TMME=0；AFL/Rx/Tx 使用 FD 窗口。未确认 TDC、收发器与网络条件前，此路线的算术候选不等于已验证总线配置。

## 12. OSTM、启动与看门狗：诊断栈时间基础

### OSTM0/1

OSTM0 基址 `T0=0xFFDD8000`，OSTM1 `T1=0xFFDD9000`。时钟选择 `IC0CKSEL0=0xFFDD6000`、`IC0CKSEL1=0xFFDD6004`，**16 位**；bit15 IC0TMEN=0 时直接使用 PCLK=80 MHz。设 1 才通过 TAUD/TAUJ 计数使能链路，不能把某个 CK0 名称当作已配置 20 MHz。HW-E pp.1542–1567。

| 寄存器 | 相对 T 地址 | 宽度 | 用途 |
| --- | --- | --- | --- |
| CMP | +0x00 | 32 | 比较值 |
| CNT | +0x04 | 32，只读 | 当前计数 |
| TO | +0x08 | 8 | 软件输出值 |
| TOE | +0x0C | 8 | 0=软件控制输出，**不是引脚高阻/输出禁用** |
| TE | +0x10 | 8，只读 | bit0 运行状态 |
| TS | +0x14 | 8，命令 | 写 1 启动 |
| TT | +0x18 | 8，命令 | 写 1 停止，随后轮询 TE=0 |
| CTL | +0x20 | 8 | MD1[1]：0 interval downcounter，1 free-running upcounter；MD0[0] 启动 IRQ |

独占 OSTM0 的 1 ms 周期参考：屏蔽 EI74 → TT 写1、确认 TE=0 → IC0CKSEL0 写16位0 → CTL 写8位0（interval，关闭启动 IRQ）→ CMP 写32位79999=`0x0001387F` → 按端口契约处理 pending/向量 → TS 写8位1 → 按 OS 规则启用中断。周期为 `(CMP+1)/80MHz`，不是 `CMP/80MHz`。不要假定该周期 ISR 就能替代 RTA 硬件 counter 的全部回调。

自由运行比较参考 CTL=2，运行中可更新 CMP。再次启动从 0 开始，因此 Cancel 不能用“停再启”破坏 OS 时间基准；应按 OS 回调契约取消比较事件并保持计数连续。80 MHz 32 位硬件计数在 53.6870912 s 回绕，而 16 位、1 ms OS tick 在 65.536 s 回绕，直接 `CNT/80000` 再截成16位不连续。应累计无符号 delta 与除法余数，并保证采样间隔小于一个硬件回绕；远期目标超过硬件可表示范围时需要维护/分段比较。完整算法和竞争处理见 [counter-design.md](counter-design.md)。

OSTM 实例有 0、1、3…7，没有 OSTM2；3…7 的 FEINT 路线不能直接当普通 EI timer 备用。对本工程首先固定 OSTM0/1 的 owner，避免 Gpt 和 OsCounter 双重配置同一实例。CanTp/DCM 所用周期任务必须基于实际时间证明，不能仅凭 alarm 配置字符串宣布诊断超时正确。

### 启动与 RAM/ECC

匹配器件的启动代码负责合法栈、GP/TP、异常入口、OS 所需寄存器状态和 C 运行时初始化；INTBP/RBASE/EBASE 属于 CPU 系统寄存器，不应伪造为 MMIO 地址。让 RTA/GHS 端口拥有向量和上下文机制。FPU 是否允许任务/ISR 使用必须与编译 ABI、库和 OS 保存策略相容。

HW-E p.2890 说明 LRAM、GRAM、DTS RAM、CSIH RAM 支持含 ECC 的硬件零初始化；各复位默认执行，但可由 RAM Initialization Mode Control 配置禁用。不能在未检查复位路径和初始化模式时宣称“所有复位 RAM 都已清零”，也不能为了规避错误把全部保留 RAM 无条件覆盖。CAN RAM 另按 GRAMINIT 流程。启动验收应保存 reset 原因、RAM 模式、ECC/ECM 错误并确认栈、向量、OS 数据首次读取前已合法初始化。

可直接用于启动日志的寄存器如下，均为 **32 位**（HW-E pp.420–430）：

| 寄存器 | 地址 | 解码与限制 |
| --- | --- | --- |
| RESF | `0xFFF81000`，只读 | bit0 POR、1 pin reset、2 CVM、3 software system、5 ECM system、7 software application、9 ECM application、10 Field BIST 执行；bit6 读值未定义 |
| RESFC | `0xFFF81008`，只写 | 在保存原因后，向上述对应位写1清除；与 CAN 的 W0C 不同，保留位写0 |
| STAC_DTSRAM | `0xFFF81320` | DTS RAM 的 application reset 初始化策略 |
| STAC_GRAM | `0xFFF81420` | GRAM 的 application reset 初始化策略 |
| STAC_LM0 | `0xFFF81520` | LRAM 的 system reset（CVM 除外）及 application reset 初始化策略 |
| STAC_LM10 | `0xFFF81E20` | CSIH RAM 的 application reset 初始化策略 |

STAC 的 RZEROMD[1:0]：11=初始化启用，00/10=禁用，01=禁止；复位默认3，具体寄存器的复位保持条件不同。它们配置后续复位的行为，不是“写3立即清 RAM”的命令。启动时先记录再决策，不能为了保留日志而同时把 OS 数据区置于未经设计的保留状态。RESF 可能包含累积标志，调试器复位也影响原因，不能把一个标志单独解释为完整复位历史。

本材料不提供未经逐项确认的 ECM 全清、Slave Guard 全开、option bytes 全覆盖脚本。出现访问异常时定位地址、访问宽度、权限、当前模式和异常原因；不能将关闭所有保护当作 CAN 初始化步骤。

### WDTA0

只有 WDTA0，基址 `0xFFD74000`；WDTE +0、EVAC +4、REF +8、MD +0xC，均为 8 位访问。OPBT0 只读映射 `0xFFCD0030`：OPWDRUN[31]、OPWDOVF[27:25]、OPWDVAC[22]、OPWDMDS[21]。OPWDMDS=0/1 对应 8 MHz/250 kHz；溢出周期 `2^(9+OVF)/counter_hz`。OPWDRUN=1 为自动启动，0 为软件启动。无 VAC 模式时 WDTE 写 `0xAC` 触发；VAC 模式不能沿用固定值，必须按该模式算法与 REF/EVAC 配合。来源 HW-E pp.1522–1535、2884。

WDTA 启动后不能当普通 timer 随意关闭；MD 改写有首次启动/触发前的限制。保留项目现有有效喂狗策略，尤其要覆盖启动等待、长诊断和 Flash 操作。`Wdg_SetTriggerCondition` 的上层预算不等于硬件溢出寄存器值。不要为了让长等待通过而无条件永久喂狗。

## 13. 将硬件事实交给 CAN stack / diagnosis stack 的边界

内部 agent 应把本文件映射到**已有供应商 MCAL 的配置容器与生成代码**，而不是新建第二套寄存器驱动与 MCAL 并行。仅需自行实现硬件驱动时，也必须先明确本单元唯一 owner。

| 内部模块 | 本文提供的硬件输入 | 内部 agent 必须在工程内证明 |
| --- | --- | --- |
| Mcu / Startup | 器件、内存、时钟、RAM/ECC 条件 | 选对 derivative；启动/链接/复位路径一致 |
| Port / Dio / CanTrcv | CAN ALT、方向、PIPC、寄存器地址 | 原理图实际 pins、收发器 EN/STB 电平、供电与 Normal 模式 |
| Can | 模式、fCAN、CFG/NCFG/DCFG、AFL/FIFO/Tx、IRQ | ARXML→生成代码→寄存器读回一致；buffer/HOH 分配无冲突 |
| Os / Gpt | OSTM 地址、宽度、周期、EI74/75、回绕约束 | 唯一 owner、真实 RTA 回调 ABI、ISR 包装、优先级与运行周期 |
| CanIf / CanTp / PduR / Dcm | 接收长度、帧格式、物理成功/失败事件、可靠时间基准 | 物理/功能 ID、PDU 路由、payload/DLC、流控、定时参数与任务调度一致 |
| CanSM / ComM | bus-off 状态和恢复硬件机制 | BOM 与软件恢复策略、控制器/收发器状态转换一致 |

RH850 硬件不存储 UDS DID、诊断会话或 SecurityAccess 的应用语义。本文不替内部工程决定这些内容；需要硬件层保证的是正确收发、不漏完成事件、长度不被截断、时间连续、错误可诊断。

验收按层提供证据：

1. **启动/时间**：冷启动与软件复位均进入 OS；无未解释的 ECC/访问异常；OSTM 的真实 1 ms 周期和长时间回绕正确，WDTA 不意外复位。
2. **轮询物理链路**：外部活动 CAN 节点提供 ACK；TX 波形、TMTRF 成功；匹配 ID 入 FIFO，不匹配 ID 不入；IDE/RTR/长度按规则处理。仅内部环回通过不能证明引脚和收发器。
3. **IRQ 链路**：确认 EI190 进入正确 ISR，消息不会滞留；EI185 只对真实发送成功产生确认；错误源消费后退出 ISR，无中断风暴。
4. **OS 并发**：持续接收、发送、诊断周期任务同时运行；证明 FIFO 峰值占用、最坏 ISR 延时、无双重消费，系统 tick 不受总线负载破坏。
5. **诊断链路**：项目定义的一次单帧请求/响应和一次多帧请求/响应成功；请求、流控、连续帧与响应 ID 均在真实总线上可见；负载下时序仍符合内部栈配置。
6. **故障与恢复**：总线无 ACK、断线、FIFO 压力、bus-off 后，错误日志与恢复动作符合项目策略；Classic/FD 两路线分别验收，不以其中一条代替另一条。

| 现象 | 优先查看 | 常见硬件原因 |
| --- | --- | --- |
| TX pin 完全不动 | CmSTS 模式、TMC/TMSTS、Port ALT/PM/PIPC | 未退出 reset/stop、选错窗口、引脚错误、发送请求未提交 |
| TX 重发且无确认 | ERFL.AERR、TEC、示波器与外部节点 | 无 ACK 节点、收发器待机、错误速率/时钟、线束问题 |
| 管脚有 RX，但 FIFO 空 | GAFLID/M/P1、GAFLCFG0、RFE、帧 IDE/RTR | mask 语义或规则窗口错误、未路由/使能 FIFO、长度超容量 |
| FIFO 有数据，CanIf 不收 | RFIF/RFIE、EI190、ISR 消费路径 | 错把 IRQ 绑到 EI184、OS 屏蔽/向量错误、只清 pending 不搬数据 |
| Rx 反复同一帧 | RFPCTR、FIFO owner | 未 pop，或错误地址/宽度，或 DMA/CPU 竞争 |
| 中断持续重入 | 对应外设 W0C 标志与错误源 | 高电平源只清 EIC、遗漏其他已使能源 |
| 小帧通、FD16不通 | RCMC、FDF/BRS、DCFG、RFPLS、DLC、收发器 | 模式混用、data timing、长度误编码、TDC/传播延时 |
| CAN 可收发但诊断超时 | OSTM 实测、TxConfirmation、任务运行 | tick 倍率/回绕错误、发送成功未上报、软件任务未调度 |
| 运行几十秒后 alarm 异常 | CNT delta、维护周期、Set/Cancel 竞争 | 32 位硬件回绕早于16位 OS wrap、计数器被重启 |

## 14. 内部工程须绑定的信息与交付输出

内部 agent 可以自行从工程/原理图读取下列信息；无需让本会话反复审批。只有资料确实不存在时标成待确认，不生成虚假配置。

- 完整 MCU 订货号、实物/硅版本、适用技术更新；R7F701381 基线与实际一致。
- 主时钟实物与启动包、MCAL 的确切供应商/版本/derivative；本项目是否确实采用 RTA-OS RH850GHS 5.0.39。
- CAN 通道、真实 RX/TX pins、Port 电气模式、收发器型号及 EN/STB/供电/终端接法。
- 总线 Classical/FD、速率、采样点要求、最大 payload、传播延时/TDC、测试仪 FD 协议设置。
- 诊断物理请求/功能请求/响应 ID、标准/扩展格式、HOH/PDU/Tx buffer 分配；不能从截图的 7 个 ID 自动推导完整路由。
- OSTM owner、OS callback 声明与向量生成规则、计数器单位、任务周期、ISR 优先级和上下文保存策略。

要求内部 agent 最终输出：①以上绑定表；②ARXML/生成配置差异；③两套模式中所选一套的地址/值/有效位读回表；④中断映射与唯一 owner 表；⑤启动日志、总线记录与分层验收结果。缺实物时可交付静态配置和预期读回，但必须明确尚未完成目标板验收。

这份材料提供可追溯的硬件输入和可验证的执行路径。是否“顺利跑起来”最终由上述工程绑定和板级证据确认，不能由资料审核或主机测试替代。
