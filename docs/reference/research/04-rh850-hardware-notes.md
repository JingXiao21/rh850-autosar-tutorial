# 04 · RH850 硬件研究笔记（Phase 1）

> 标记：本文件全部内容属于 `[RH850 Hardware]` 类别。
> 规则：每个具体事实都带 `(资料简称 p.PDF页码)`。手册中查不到的内容一律写成 **“需根据实际芯片手册确认”**，不做推测。
> 已有文档 `docs/hardware-findings.md`、`docs/rh850-hardware-handoff.md` 只作线索；下文结论均已回到原手册页面复核。若两者不一致，以本文引用的手册页面为准。

## 0. 资料与简称

| 简称 | 文件（`artifacts/pdf-text/`） | 实际内容 |
|---|---|---|
| **HW-E** | `r01uh0585ej0120.txt` | **RH850/P1M-E Group User's Manual: Hardware** Rev.1.20 (Mar 2018)，共 3121 页 (HW-E p.1)。PDF 页码与手册印刷页码一致（如 PDF p.187 印有 “Page 187 of 3121”）。 |
| **DS-E** | `r01ds0505ed0100-rh850p1m-e.txt` | RH850/P1M-E Datasheet Rev.1.00 (DS-E p.1) |
| **HW-X** | `REN_r01uh0436ej0140-rh850p1x_MAH_20180330_1.txt` | RH850/P1x Group（P1H / P1M，**非 -E、非 -C**）User's Manual: Hardware Rev.1.40 (HW-X p.1, p.60) |
| **DS-C** | `REN_r01ds0506ed0100-rh850p1x-c_DST_20251218.txt` | RH850/P1x-C（P1H-C / P1H-CE / P1M-C）Datasheet (DS-C p.1–2) |

仓库内**没有**的资料（凡涉及以下内容，本文只讲架构，不给细节）：
- **RH850G3M User's Manual: Software**：HW-E 多处直接引用它来说明完整的异常表、异常优先级、EIINT/FEINT 的确认流程、SYSCALL/CALLT 语义和 SYNCP 要求 (HW-E p.193, p.199, p.205, p.254, p.256, p.264, p.281)。
- RH850/P1M-E Flash Memory User's Manual: Hardware Interface：这本手册讲 Code/Data Flash 自编程与 FACI 时序 (HW-E p.255, p.2888)。
- 板卡原理图、CAN 收发器数据手册、工具链（GHS/CC-RH）手册、MCAL 供应商文档。

---

## 1. 器件识别

### 1.1 R7F701381 是哪颗芯片

- DS-E Table 1.1 把 **R7F701381** 列为 **RH850/P1M-E, 100-pin (LFQFP100 14×14), DPS 电源方式, 1 MB Code Flash** (DS-E p.2)。
- 同一张表的其他型号：R7F701375–380（2 MB / eVR）和 R7F701382–386 (DS-E p.2)。运行时可以读 PRDNAME1–4 产品名寄存器来核对型号，R7F701381 对应 `3746 3752 3833 3130 2020 2031 2020 2020` (HW-E p.2880)。
- **结论**：如果项目芯片确实是 R7F701381，就以 **HW-E + DS-E** 为准。HW-X（P1H/P1M 非 E 版）和 DS-C（P1x-C）是不同的器件：
  - HW-X 的 CAN 是 **RS-CAN**（只有 classical CAN）(HW-X 目录 Section 17 “CAN Interface (RS-CAN)”, p.772)。它的时钟、端口用 PROT1PHCMD/PPCMDn 做写保护 (HW-X p.259–261)，这些都与 P1M-E 不同。
  - P1x-C 的 CAN 是 **MCAN / M_TTCAN**（Bosch M_CAN 系 IP）(DS-C p.4)，和 RS-CANFD 是完全不同的 IP。它还带 G3K 安全核 (DS-C p.3)。
  - **三个系列的寄存器不能互相套用。** 实际项目用哪颗芯片，必须看 BOM、芯片丝印或读 PRDNAME 来确认。

### 1.2 CPU 与频率

| 项目 | P1M-E 值 | 出处 |
|---|---|---|
| CPU 核 | RH850**G3M**，主核 1 个，另有 checker core 以 lock-step 运行（检查核不是第二个可调度核） | DS-E p.1–2；HW-E p.187, p.250 |
| FPU | 单/双精度，IEEE754 | DS-E p.2；HW-E p.189 |
| MPU | 16 区 | DS-E p.2；HW-E p.189 |
| I-Cache | 16 KB 4-way | DS-E p.2 |
| CLK_CPU | 160 MHz（PLL 输出） | HW-E p.469 |
| CLK_HSB（高速外设） | 80 MHz | HW-E p.469 |
| CLK_LSB（低速外设） | 40 MHz | HW-E p.469 |
| Main OSC | **只支持 16 MHz** | DS-E p.2；HW-E p.469 |
| CLK_IOSC | 8 MHz（HS IntOSC/2） | HW-E p.469–470 |
| PLL | 输入 16 MHz，输出 160 MHz | HW-E p.2905 |

> 注意：不是 G4MH。G4MH 相关特性（如多上下文、虚拟化）不适用于本芯片。

### 1.3 地址映射（P1M-E，Table 4.1）

| 地址 | 区域 | 大小 | 出处 |
|---|---|---|---|
| `0000_0000`–`000F_FFFF` | Code Flash 用户区（1 MB 型号；2 MB 型号到 `001F_FFFF`） | 1 MB / 2 MB | HW-E p.257 |
| `0100_0000`–`0100_7FFF` | Code Flash 扩展用户区 | 32 KB | HW-E p.257 |
| `0100_A000`–`0100_BFFF` | ECC test area | 8 KB | HW-E p.257, p.2887 |
| `1000_0000`–`1FFF_FFFF` | On-chip I/O（H-Bus 区） | 256 MB | HW-E p.257 |
| `FEBE_0000`–`FEBF_FFFF` | Local RAM（PE1 area，供其他总线主访问的别名） | 128 KB | HW-E p.257, p.259 |
| `FEDE_0000`–`FEDF_FFFF` | Local RAM（self，CPU 自身访问） | 128 KB（与上面是同一块 RAM） | HW-E p.257, p.259 |
| `FEEF_8000`–`FEEF_FFFF` | Global RAM Bank A | 32 KB | HW-E p.257 |
| `FEF0_0000`–`FEF0_7FFF` | Global RAM Bank B | 32 KB | HW-E p.257 |
| `FF00_0000`–`FFFD_FFFF` | On-chip I/O（P-Bus 区） | 16 MB−128 KB | HW-E p.257 |
| `FF20_0000`–`FF20_7FFF` | Data Flash（1 MB 型号 32 KB；2 MB 型号到 `FF20_FFFF`，64 KB） | 32/64 KB | HW-E p.257；DS-E p.2 |
| `FFFE_E000`–`FFFE_FFFF` | LPB self 区：SEG、PEG、IPG、INTC1 等 CPU 私有外设，只有 PE1 能访问 | 8 KB | HW-E p.257 |
| `FFFF_5000`–`FFFF_FFFF` | On-chip I/O（P-Bus 区，含 INTC2 的 EIC32–383） | 44 KB | HW-E p.257, p.265 |

- 可取指的区域只有 Code Flash、Local RAM (self) 和 Global RAM (HW-E p.258)。DMA 和 H-Bus 主设备**访问不到** Local RAM (self) 地址，只能走 PE1 别名 `FEBE_xxxx` (HW-E p.259)。
- Local RAM 128 KB、Global RAM 64 KB、ERAM 32 KB/8 KB（2 MB 与 1 MB 型号不同）(DS-E p.2；HW-E p.2889)。
- 访问保护（guard）：P-Bus 区由 PBG 保护，LPB 区由 IPG/PEG 保护，Local RAM 由 PEG 保护，Global RAM 由 GRG 保护 (HW-E p.260)。复位后，外设对 PE1 以外的主设备默认是受保护状态 (HW-E p.188)。

---

## 2. CPU（RH850G3M，按 HW-E §3.2）

### 2.1 通用寄存器与 PC

| 寄存器 | 约定用途 | 出处 |
|---|---|---|
| r0 | 恒为 0 | HW-E p.190 |
| r1 | 汇编器保留（生成地址用） | HW-E p.190–191 |
| r2 | 地址/数据变量；**某些 RTOS 会占用** | HW-E p.190–191 |
| r3 | **SP**。PREPARE、DISPOSE、PUSHSP、POPSP 会隐式使用 | HW-E p.190–191 |
| r4 | GP（global pointer） | HW-E p.190 |
| r5 | TP（text pointer） | HW-E p.190 |
| r6–r29 | 地址/数据变量 | HW-E p.190 |
| r30 | EP（element pointer），SLD/SST 的基址 | HW-E p.190–191 |
| r31 | LP（link pointer） | HW-E p.190 |
| PC | bit0 固定为 0；复位值由 reset vector 决定 | HW-E p.191 |

- 复位后 r1–r31 的值**未定义** (HW-E p.190)，所以启动代码必须自己设置 SP/GP/TP/EP。r1/r3–r5/r31 的具体 ABI 用法“见各开发环境说明” (HW-E p.190)，需根据实际编译器手册确认。

### 2.2 PSW（SR5,0；复位值 `0x0000_0020`）

| 位 | 名称 | 含义 | 出处 |
|---|---|---|---|
| 30 | UM | 0=Supervisor，1=User | HW-E p.197 |
| 18–16 | CU2–0 | CU0=FPU 使用许可。为 0 时执行 FPU 指令会产生 coprocessor unusable 异常 | HW-E p.197 |
| 15 | EBV | 0：异常向量基址用 RBASE；1：用 EBASE | HW-E p.198, p.205 |
| 11–9 | Debug | 调试用，正常运行时写 0 | HW-E p.198 |
| 7 | NP | 受理 FE 级异常时自动置 1，屏蔽 FE/EI 级异常 | HW-E p.198 |
| 6 | EP | 正在处理一个“非 INTC 中断”的异常 | HW-E p.198 |
| 5 | ID | 受理 EI 或 FE 级异常时自动置 1，屏蔽 EI 级中断；DI 指令置 1，EI 指令清 0 | HW-E p.198 |
| 4 | SAT | 饱和运算累积标志 | HW-E p.198 |
| 3/2/1/0 | CY/OV/S/Z | 进位/溢出/负/零 | HW-E p.198 |

- 在 UM 下用 LDSR 写受 SV 保护的位，写入会被忽略，**不产生 PIE 异常** (HW-E p.197 Note 1)。
- MCTL.UIC=1 时，用户模式也能执行 EI/DI (HW-E p.208)。MCTL.MA 决定 misaligned 访问是否产生异常 (HW-E p.208)。

### 2.3 系统寄存器（LDSR/STSR，regID, selID）

| 寄存器 | 编号 | 作用 | 出处 |
|---|---|---|---|
| EIPC / EIPSW | SR0,0 / SR1,0 | EI 级异常受理时保存 PC/PSW。**只有一组**，多重异常时必须由软件保存 | HW-E p.192–194 |
| FEPC / FEPSW | SR2,0 / SR3,0 | FE 级异常时保存 PC/PSW。只有一组 | HW-E p.192, p.195–196 |
| EIIC / FEIC | SR13,0 / SR14,0 | 异常原因码；中断的原因码见 Table 6.11 | HW-E p.199 |
| EIWR / FEWR | SR28,0 / SR29,0 | 异常处理中可自由使用的工作寄存器 | HW-E p.192, p.201–202 |
| CTPC / CTPSW / CTBP | SR16,0 / SR17,0 / SR20,0 | CALLT 用 | HW-E p.200–201 |
| RBASE | SR2,1 | reset vector 基址（只读），bit0=RINT | HW-E p.205 |
| EBASE | SR3,1 | EBV=1 时的异常向量基址，bit0=RINT | HW-E p.205 |
| INTBP | SR4,1 | 表引用方式中断的地址表基址（低 9 位为 0） | HW-E p.206 |
| SCCFG / SCBP | SR11,1 / SR12,1 | SYSCALL 表大小与基址 | HW-E p.207 |
| MCFG0 / MCTL / PID | SR0,1 / SR5,1 / SR6,1 | 配置 / 控制 / ID（PID=`0x0580_0714`，其中 bit10/9/8 分别表示双精度 FPU、单精度 FPU、MPU） | HW-E p.206–208 |
| FPIPR / ISPR / PMR / ICSR / INTCFG | SR7,1 / SR10,2 / SR11,2 / SR12,2 / SR13,2 | 中断优先级控制，见 §2.5 | HW-E p.209–212 |
| FPSR, FPEPC, FPST, FPCC, FPCFG, FPEC | SR6–11,0 | FPU。浮点运算使用通用寄存器（双精度用寄存器对） | HW-E p.192, p.213 |
| MPM, MPRC, MPLAn/MPUAn/MPATn … | SR*,5/6/7 | MPU | HW-E p.214–215 |
| MEA / MEI | SR6,2 / SR8,2 | MAE/MDP 异常的地址与指令信息 | HW-E p.202–204 |

### 2.4 异常级别与向量

**手册中能确认的内容：**
- 异常分为 **FE 级** 和 **EI 级**。FENMI 来自 ECM（不可屏蔽），FEINT 来自 NMI 引脚和 OSTM3–7，EIINT 共 384 路 (HW-E p.264)。
- SYSERR 是 **FE 级异步异常**，**不能返回或恢复**，由 SEG 产生（来源包括取指或数据访问错误、PBG/IPG 违规、RAM ECC 等）(HW-E p.244–245)。
- 发生 MDP/MIP 时，MEA/MEI 记录现场信息 (HW-E p.202–203, p.215)。
- 异常处理程序地址 = 基址（PSW.EBV=0 时用 **RBASE**，=1 时用 **EBASE**）+ 偏移 (HW-E p.205, p.281)：
  - FENMI `+0E0H`，FEINT `+0F0H` (HW-E p.282)。
  - EIINT **直接分支方式**（EICn.EITB=0）：RINT=0 时，偏移按该通道优先级 0–15 落在 `+100H`–`+1F0H`；RINT=1 时，所有中断都用 `+100H` (HW-E p.281–282)。
  - EIINT **表引用方式**（EICn.EITB=1）：从 `INTBP + 通道号×4` 读取处理程序地址 (HW-E p.281, p.268)。
- FENMI、FEINT、EIINT（直接向量方式）、SYSERR、FPI 的处理程序入口前需要插入 **SYNCP** (HW-E p.281, p.256)。
- 同时发生多个异常时，保存现场的系统寄存器会被覆盖，能否正确返回取决于异常种类 (HW-E p.255)。

**仓库内资料不足，需根据 RH850G3M User's Manual: Software 确认：**
- RESET 之外各异常（SYSERR、FETRAP/TRAP、RIE、UCPOP、PIE、MAE、MIP/MDP、FPP/FPI、SYSCALL 等）的向量偏移、优先级、可恢复性。
- 受理异常时 PSW.UM、EBV、CU 等位如何变化；EIRET/FERET 的完整语义。
- 本文不给出这些偏移的数值。

### 2.5 中断优先级与屏蔽（CPU 侧）

- 优先级 0（最高）到 15（最低）。同一优先级时，通道号小的先受理 (HW-E p.268)。
- **ISPR**：受理 EIINT 时，对应优先级位由硬件**自动置 1**。执行 EIRET 时，如果 PSW.EP=0，硬件清除其中最高优先级的位。ISPR 中有位为 1 时，同级和更低优先级的中断被屏蔽 (HW-E p.210)。
- **PMR**：软件屏蔽某些优先级，必须从最低优先级开始连续置位（例如 `FF00H` 可以，`F0F0H` 不行）(HW-E p.211)。
- **INTCFG.ISPC**：一般保持 0（ISPR 自动更新）。只有在用 PMR 做软件优先级控制时才置 1 (HW-E p.212)。
- ICSR.PMEI/PMFP：表示存在被 PMR 屏蔽的挂起中断 (HW-E p.211)。

### 2.6 中断时硬件保存什么，软件保存什么

- **硬件保存**：PC→EIPC，PSW→EIPSW，原因码→EIIC；PSW.ID 置 1；ISPR 对应位置 1 (HW-E p.193–194, p.198–199, p.210)。FE 级用 FEPC/FEPSW/FEIC，并把 NP 置 1 (HW-E p.195–196, p.198)。
- **软件负责**：通用寄存器（r1–r31）、多重中断时的 EIPC/EIPSW（只有一组，必须由程序保存）(HW-E p.193–194)。FPU 系统寄存器是否需要保存，取决于 ISR 是否使用 FPU，属于编译器/OS 策略，需按工具链手册确认。
- CPU 提供 **PUSHSP/POPSP** 指令，用于快速保存/恢复上下文 (HW-E p.189)。
- **清中断源后再开中断的同步要求**（对 CAN ISR 很关键）：先 store 写控制寄存器 → 对该寄存器做一次 dummy read → 执行 SYNCP → 再执行 EI 或访问下一个外设 (HW-E p.254)。

### 2.7 特权级

- PSW.UM：0=SV，1=UM (HW-E p.197)。多数系统寄存器（EIPC、RBASE、INTBP、MPU 寄存器等）只能在 SV 下访问 (HW-E p.192, p.209, p.214)。
- 只有 PE1 在 SV 模式（UM=0）下才能写 EICn、IMRn、EIBDn (HW-E p.265)。SEG 寄存器在 UM 下写入会被忽略 (HW-E p.244)。

---

## 3. 启动与存储器

### 3.1 复位时 CPU 做什么

- 复位分四类：Power On Reset、System Reset 1（Pin/CVM/调试器断开）、System Reset 2（SWSRESA0 软件复位、ECM）、Application Reset 1（SWARESA0、ECM）(HW-E p.418, p.431)。复位原因可以读 RESF `0xFFF8_1000` (HW-E p.420–421)。
- reset vector 等于 RBASE 的初值。启动区（startup mat）为 user mat 时，RBASE 初值为 **`0x0000_0000`** (HW-E p.258)。PC 复位值也随 reset vector 设置而变 (HW-E p.191)。
- 存在 “**variable reset vector**” 功能，可通过 Flash 保护设置改写 reset vector，用于安全更新 boot 程序。GREG8 `0xFFCD_0020` 可以读出 Reset Vector 0 (HW-E p.2865, p.2880, p.205 Note 1)。实际项目的 reset vector 值需根据实际 Flash 配置确认。
- PSW 复位值 `0x20`，即 ID=1，复位后 EI 级中断处于屏蔽状态 (HW-E p.197)。
- 复位解除后执行 Field BIST 和 RAM 初始化 (HW-E p.432, p.434)。Power On Reset、System Reset 1、System Reset 2 会从 Flash 重新读取 option bytes；Application Reset 1 不读 (HW-E p.431, p.434)。

### 3.2 工作模式 / Boot mode

- 复位时锁存 FLMD0 和 FLMD1(P3_14) 决定模式：FLMD0=0 为 Normal 模式；FLMD0=1 且 FLMD1=0 为 Serial programming 模式（运行片上 boot 程序）(HW-E p.261)。
- MODE 寄存器 `0xFFF8_0104` 可读出锁存值 (HW-E p.263)。
- FLMD0=1 时允许改写 Code Flash，=0 时禁止 (HW-E p.262)。

### 3.3 Option bytes

- OPBT0 `0xFFCD_0030`（只读映射）(HW-E p.2884)：
  - OPWDRUN：WDTA0 是 default start（复位后自动运行）还是软件触发启动。
  - OPWDOVF[2:0]：溢出时间 2^(9..16)/WDTATCKI。
  - OPWDVAC、OPWDMDS（8 MHz / 250 kHz）。
  - OPEVTO/OPEVTI、ERROUTSEL。
- OPBT2 `0xFFCD_0038`：OPJTAG[1:0] 选择 GPIO / LPD / Nexus (HW-E p.2886)。
- 写 Flash 程序前必须先设好 option bytes (HW-E p.2881)。
- 项目实际值需读芯片或编程器配置确认。如果 OPWDRUN=1，启动代码要及时喂狗。

### 3.4 RAM 与 ECC 初始化

- 除 I-Cache RAM 和 ERAM 外，所有 RAM 都带 ECC (HW-E p.2889–2890)。
- **LRAM、GRAM、DTS RAM、CSIH RAM 在复位时由硬件清零，并正确写入 ECC**，所以不需要软件逐字清零 (HW-E p.2890)。
- 这种硬件清零可以通过 STAC_LM0 `0xFFF8_1520`（Local RAM）、STAC_GRAM `0xFFF8_1420`、STAC_DTSRAM `0xFFF8_1320`、STAC_LM10 `0xFFF8_1E20` 关闭。RZEROMD：`11`=执行，`x0`=不执行，`01`=禁止使用 (HW-E p.420, p.427–430)。
  - Local RAM：在 Pin Reset、System Reset 2、Application Reset 1 下可以关闭硬件清零。
  - GRAM/DTS RAM/CSIH RAM：只在 Application Reset 1 下可以关闭 (HW-E p.434)。
- **注意**：如果从 RAM 执行代码，要把代码末尾之后 48 字节也初始化，否则预取可能读到未初始化区域，触发 ECC 错误 (HW-E p.256)。往 RAM 写入代码后再跳转执行，要按 store → dummy read → SYNCP → SYNCI 的顺序 (HW-E p.255)。
- RS-CANFD RAM 由 CAN 模块在复位后自行初始化，需要 3794 个 pclk 周期；GSTS.GRAMINIT 为 0 之后才能配置 CAN (HW-E p.1090, p.821)。

### 3.5 栈初始化

- 手册只说明 r3 是 SP，并被 PREPARE/DISPOSE/PUSHSP/POPSP 隐式使用 (HW-E p.190–191)。
- 栈放在哪里、多大、是否 8 字节对齐，由工具链 ABI 和链接脚本决定，需根据实际工具链手册与 linker 文件确认。可以放栈的位置是 Local RAM（self）`FEDE_0000`–`FEDF_FFFF` (HW-E p.257)。

---

## 4. 时钟

### 4.1 P1M-E 的时钟结构（重要：和其他 RH850 不同）

- 时钟源：Main OSC（X1/X2，只支持 16 MHz）、HS IntOSC、PLL (HW-E p.468；DS-E p.2)。
- CLK_CPU=160 MHz 来自 PLL。CLK_HSB=80 MHz，CLK_LSB=40 MHz（由框图中的 1/2 分频得到）(HW-E p.469–470)。
- **HW-E §12 的寄存器表只有 CLKD2DIV/CLKD2STAT、CLKD3DIV/CLKD3STAT（外部时钟输出分频）、CKSC2C/S、CKSC3C/S（EXTCLK0O/1O 源选择）和 CKSC8C/S（ADC 时钟）** (HW-E p.471)。
- 全文检索 `PLLE`、`PLLS`、`MOSCE`、`CKSC_CPUCLK` 都**没有结果**。因此在 P1M-E 上，**软件没有可配置的 PLL 使能或 CPU 时钟选择寄存器**，频率按 Table 12.2 固定。
- 不要把 RH850/F1x 等系列的 MOSC/PLL 启动序列搬过来。启动时 PLL 由谁、在什么时候锁定，手册没有给出软件步骤，需根据实际芯片手册/启动代码确认。

| 时钟 | 用户模块（Table 12.3） | 出处 |
|---|---|---|
| CLK_CPU | PE1、Local RAM、INTC1、Code Flash（1 wait）、GRAM（1 wait） | HW-E p.469 |
| CLK_HSB 80 MHz | INTC2、DMA、TAUD/TAUJ、OSTM、RLIN、CSIH、FlexRay 等 | HW-E p.469 |
| CLK_LSB 40 MHz | Data Flash、**RS-CANFD（clkc）**、时钟控制器 | HW-E p.469 |
| Main OSC 16 MHz | **RS-CANFD（clk_xincan）** | HW-E p.469 |
| CLK_ADC | CKSC8C 选择 40 或 20 MHz | HW-E p.469, p.480 |

### 4.2 CAN 时钟

- RS-CANFD 有三个时钟：**pclk = CLK_HSB（80 MHz）**、**clkc = CLK_LSB（40 MHz）**、**clk_xincan = CLK_MOSC（16 MHz）** (HW-E p.791)。
- 位时间时钟 fCAN 由 GCFG.DCS 选择：0=clkc，1=clk_xincan (HW-E p.817)。
- 传输速率 > 2 Mbps（CAN FD 数据段）时**不要选 clk_xincan** (HW-E p.791 Table 17.7)。
- 不能把 80 MHz 当作 fCAN 来算波特率 (HW-E p.791, p.797)。

### 4.3 时钟输出（板级测频用）

- 外部时钟输出：CKSCnC 选源（3=MainOSC，4=LSB，5=CPU，6=IOSC），CLKDnDIV 设分频 1–1023（0 表示停止输出）(HW-E p.476, p.472)。
- 切换时钟源前必须满足 DIV=0 且 STAT=`0x2`；改分频前要等 SYNC=1；输出频率须小于 20 MHz (HW-E p.472, p.476, p.482)。

### 4.4 保护写入（PROTCMD 问题）

- **P1M-E 没有 PROTCMDn / PROTSn 寄存器**：在 HW-E 全文中检索 `PROTCMD`、`PROTS[0-9]` 都是 0 结果。
- 时钟控制器寄存器在 P1M-E 上由 **Slave Guard** 防误写 (HW-E p.471)；复位寄存器由 **P-Bus Guard（PBG）** 保护 (HW-E p.420)。（勘误：初稿把两者都写成 PBG，见 05-consistency-review-log.md）
- P1M-E 上**确实使用 `0xA5` 解锁序列**的寄存器：
  - **CLMAnCTL0**：向 CLMAnPCMD 写 `A5H` → 写设定值 → 写设定值的按位取反 → 再写设定值 → 读 CLMAnPS.PRERR 确认 (HW-E p.2757, p.2761, p.2764)。
  - **ECM** 寄存器：ECMPCMD1 / ECMmPCMD0 写 `0000_00A5H`，接着同样的写入三步，最后读 ECMPS.ECMPRERR 确认 (HW-E p.2795)。
  - **FLMDCNT** `0xFFA0_0000`：通过 FLMDPCMD/FLMDPS 解锁 (HW-E p.2870–2871)。
  - 在解锁序列中，如果中断或其他代码访问了**同一模块**的其他寄存器，这次写入会失败 (HW-E p.2795–2796)。
- 对照：P1x（非 E）用 PROT1PHCMD/PROT1PS 保护 CKSC0CTL、CKSC1CTL 等，用 PPCMDn/PPROTSn 保护 PODC/PDSC/PUCC/PINV 等端口寄存器 (HW-E 无此机制；HW-X p.257–261)。
- 因此教程里“Mcu_InitClock 内部要走 PROTCMD 0xA5 序列”的说法，**对 R7F701381 不成立**，只能作为其他 RH850 系列的例子提及。

---

## 5. 中断控制器（INTC1 / INTC2）

### 5.1 结构与寄存器

- **INTC1** 是 CPU 私有的，管理 EIC0–31、IMR0、EIBD0–31、FNC、FIC，地址在 `FFFE_Exxx`，只有本 CPU 能访问。
- **INTC2** 管理 EIC32–383、IMR1–11、EIBD32–383，地址在 `FFFF_Bxxx`。
- 两者都只能在 SV 下写 (HW-E p.265)。

| 寄存器 | 地址 | 宽度 | 要点 | 出处 |
|---|---|---|---|---|
| EIC0–31 | `FFFE_EA00` + 2n | 16（L/H 可 8 位或 1 位访问） | — | HW-E p.265, p.267 |
| EIC32–383 | `FFFF_B040`–`FFFF_B2FE`，即 `FFFF_B000` + 2n | 16 | — | HW-E p.265, p.267 |
| IMR0 / IMR1–11 | `FFFE_EAF0` / `FFFF_B404`–`B42C` | 32/16/8 | 每位与 EICn.EIMK 联动 | HW-E p.269 |
| EIBD0–31 / 32–383 | `FFFE_EB00` / `FFFF_B880`–`BDFC` | 32 | PEID **必须为 001**，GPID 必须为 00；处理 EIINT 期间禁止修改 | HW-E p.271 |
| FNC / FIC | `FFFE_EA78` / `FFFE_EA7A` | 16 | FE 级 NMI / FEINT 请求标志 | HW-E p.272 |
| FEINTF / FEINTFC | `FFD6_7000` / `FFD6_7008` | 32 | 区分 FEINT 来源：NMI 引脚还是 OSTM3–7，并清除 | HW-E p.278–279 |
| SINTR0–4 | `FFC0_0000` + 4n | 8 | 软件中断计数器 | HW-E p.266, p.273 |

**EICn 各位** (HW-E p.267–268)：

| 位 | 名称 | 说明 |
|---|---|---|
| 15 | EICT | 只读：0=同步边沿检测，1=高电平检测 |
| 12 | EIRF | 请求标志。只有边沿检测型可由软件写 |
| 7 | EIMK | 1=屏蔽，复位值为 1 |
| 6 | EITB | 0=直接分支方式，1=表引用方式 |
| 3–0 | EIP | 优先级 |

- 复位值：边沿型为 `008FH`，电平型为 `808FH`。
- **注意**：对 EIC 做读-改-写（包括 set1/clr1 指令）可能丢失或重复中断。必须在外设不产生请求、CPU 也没有正在受理该中断时才写 EIC (HW-E p.267)。

### 5.2 关键中断通道（Table 6.11 / 17.8 / 22.5）

“表偏移”是表引用方式下相对 INTBP 的偏移，即 4×通道号。EIC 地址按 `FFFF_B000 + 2n` 计算得到，规则见 HW-E p.265。

| 源 | 名称 | EI 通道 | 表偏移 | EIC 地址 | 出处 |
|---|---|---|---|---|---|
| CAN0 error | INTRCAN0ERR | 183 | +2DCH | `FFFF_B16E` | HW-E p.285, p.792 |
| CAN0 TX/RX FIFO 接收完成 | INTRCAN0REC | 184 | +2E0H | `FFFF_B170` | 同上 |
| CAN0 transmit | INTRCAN0TRX | 185 | +2E4H | `FFFF_B172` | 同上 |
| CAN1 error / REC / TRX | INTRCAN1ERR/REC/TRX | 186 / 187 / 188 | +2E8 / +2EC / +2F0H | `FFFF_B174` / `B176` / `B178` | HW-E p.286, p.792 |
| CAN global error | INTRCANGERR | 189 | +2F4H | `FFFF_B17A` | 同上 |
| CAN receive FIFO（全局，8 个 RX FIFO 共用） | INTRCANGRECC | 190 | +2F8H | `FFFF_B17C` | 同上 |
| CAN2 error / REC / TRX | INTRCAN2ERR/REC/TRX | 191 / 192 / 193 | +2FC / +300 / +304H | `FFFF_B17E` / `B180` / `B182` | 同上 |
| OSTM0 / OSTM1 | INTOSTM0/1 | 74 / 75 | +128 / +12CH | `FFFF_B094` / `B096` | HW-E p.283, p.1544 |
| OSTM3–7 | INTOSTM3–7 | **FEINT**（不是 EI 通道） | +0F0H（FEINT） | FIC | HW-E p.264, p.1544 |
| WDTA0 75% | INTWDTA0 | 9 | +024H | `FFFE_EA12` | HW-E p.282 |
| TAUJ0 ch0–3 | INTTAUJ0I0–3 | 133–136 | +214…+220H | `FFFF_B10A`… | HW-E p.284 |
| TAUD0 ch0–15 | INTTAUD0I0–15 | 141–156 | +234…+270H | `FFFF_B11A`… | HW-E p.285, p.1572 |

- **CAN 中断的检测方式**：Table 6.11 的 CAN 行在 “Level Interrupt” 列有标记（PDF 抽取出来是一个特殊符号），OSTM 行没有。按表注 1，有标记的源是**高电平检测**，必须在 ISR 里清除模块内的状态标志（EIRF 不能由软件清）(HW-E p.285–290 Note 1/2)。
  - 这一点是从抽取文本推断的。上电后可以读 EICn.EICT 位直接确认 (HW-E p.267)。
- **FE 级中断源**：FENMI 来自 ECM；FEINT 来自 NMI 引脚和 OSTM3–7 (HW-E p.264, p.278)。

### 5.3 CAN 中断与标志的对应关系（Table 17.175）

| 中断 | 请求标志 → 使能位 | 出处 |
|---|---|---|
| 全局 RX FIFO 中断 | RFSTSx.RFIF → RFCCx.RFIE | HW-E p.1058 |
| 全局错误中断 | GERFL.DEF / MES / THLES → GCTR.DEIE / MEIE / THLEIE | HW-E p.1058 |
| CANm 发送中断 | TMSTSp.TMTRF → TMIECy.TMIEp（发送完成）；TAIE（发送中止）；CFTXIF/TXQIF/THLIF | HW-E p.1058 |
| CANm TX/RX FIFO 接收中断 | CFSTSk.CFRXIF → CFCCk.CFRXIE | HW-E p.1058 |
| CANm 错误中断 | CmERFL.BEF / EWF / EPF / BOEF / BORF / OVLF / BLF / ALF → CmCTR 中对应的 *IE 位 | HW-E p.1058 |

- 在中断标志清除之前，模块会一直输出中断请求 (HW-E p.1057)。

---

## 6. Port / GPIO

### 6.1 寄存器角色

- 端口组：P0–P5、JP0。`PORT_base = 0xFFC1_0000`，`JPORT0_base = 0xFFC2_0000` (HW-E p.91)。

| 寄存器 | 偏移（+n×40H） | 作用 | 出处 |
|---|---|---|---|
| Pn | +0000H | 输出数据 | HW-E p.99 |
| PSRn | +0004H | 置位/复位：高 16 位是写使能，低 16 位是值 | HW-E p.96, p.99 |
| PNOTn | +0008H | 取反 | HW-E p.99 |
| PPRn | +000CH | 读引脚（来源见 Table 2.7） | HW-E p.95, p.99 |
| PMn | +0010H | 1=输入，0=输出 | HW-E p.96, p.99 |
| PMCn | +0014H | 0=port，1=alternative | HW-E p.101 |
| PFCn / PFCEn / PFCAEn | +0018H / +001CH / +0028H | 选择 ALT1–6 | HW-E p.94, p.99 |
| PMSRn / PMCSRn | +0020H / +0024H | PM/PMC 的置位/复位寄存器 | HW-E p.99, p.102 |
| PINVn | +0030H | 输出电平反相 | HW-E p.99 |
| PIBCn | +4000H | 端口输入模式下的输入缓冲使能 | HW-E p.96, p.99 |
| PBDCn | +4004H | 双向读回 | HW-E p.96, p.99 |
| PIPCn | +4008H | direct I/O 控制 | HW-E p.93, p.99 |
| PUn / PDn | +400CH / +4010H | 上拉 / 下拉 | HW-E p.100 |
| PODCn / PDSCn / PUCCn / PODCEn / PISAn | +4014 / +4018 / +4028 / +403C / +402CH | 输出类型、驱动能力、输入缓冲类型 | HW-E p.100 |

**ALT 编码 `[PFCAE, PFCE, PFC]`** (HW-E p.94)：

| ALT | 编码 |
|---|---|
| ALT1 | 000 |
| ALT2 | 001 |
| ALT3 | 010 |
| ALT4 | 011 |
| ALT5 | 100 |
| ALT6 | 101 |

S/W I/O 控制模式下，由 PMn 决定方向（0=输出，1=输入）。

- **PIPC=1 只允许用于 Table 2.33 列出的 CSIH/CSIG/TSG3 引脚**，CAN 不在其中，所以 CAN 引脚必须用 PIPC=0 (HW-E p.131)。
- 同一个外设输入功能（例如 RSCAN0RX0）同一时间只能在一个引脚上使能 (HW-E p.131)。
- P1M-E 端口寄存器**没有** PPCMD 写保护（HW-E 全文检索 `PPCMD`、`PPROTS` 为 0 结果）。这与 P1x 不同 (HW-X p.259)。

### 6.2 推荐配置顺序（Figure 2.7 / 2.8，alternative mode）

1. PBDC=0、PIBC=0、PM=1、PMC=0、PIPC=0，先让引脚处于安全的输入状态。
2. 配置端口滤波器。
3. 配置 PU/PD/PISA（输入相关）和 PDSC/PUCC/PODC/PODCE/PINV（输出相关）。
4. 配置 PFC/PFCE/PFCAE（选择 ALT）。
5. 配置 PIPC。
6. 设 PMC=1。
7. 设 PM（输出引脚写 0）。
8. 配置 PIBC/PBDC。

依据 HW-E p.127–130。

- **注意**：PIPC=0 时，从 PMC 置 1 到 PM 清 0 之间，引脚会短暂处于 alternative 输入状态。如果该引脚复用了中断功能，需要先屏蔽中断 (HW-E p.126)。

### 6.3 CAN 引脚（P1M-E）

**Table 17.10 Combinations of Pins and Ports** (HW-E p.793)：

| 通道 | RX 可选引脚 | TX 可选引脚 | 100-pin 可用？ | ALT 号 |
|---|---|---|---|---|
| CAN0 | P2_0 | P2_1 | 是 | RX/TX 都是 ALT1 |
| CAN0 | P3_7 | P3_8 | 是 | 都是 ALT3 |
| CAN0 | P4_5 | P4_6 | 是 | 都是 ALT3 |
| CAN1 | P2_2 | P2_3 | 是 | 都是 ALT1 |
| CAN1 | P3_12 | P3_13 | 是 | 都是 ALT3 |
| CAN1 | P4_2 | P4_3 | 是 | 都是 ALT1 |
| CAN1 | P4_7 | — | 仅 144-pin | — |
| CAN2 | P5_6 | P5_5 | 是 | RX ALT1；**TX ALT6** |
| CAN2 | — | P5_7 | 仅 144-pin | ALT1 |

- 100-pin 型号的 CAN2 TX 只有 P5_5 (DS-E p.23；HW-E p.793)。
- **ALT 号的来源**：DS-E/HW-E Pin Assignment 表中每个引脚的功能按 ALT 顺序列出 (DS-E p.8–12；HW-E p.70–74)，再与 HW-E p.151–154 的端口功能表核对得到。例如 `P5_5 / SENT0RX / SENT0SPCO / … / SCI31RX / INTP1 / RSCAN0TX2` 中，RSCAN0TX2 位于第 6 列，因此是 ALT6；P3_7/P3_8 的 RSCAN 位于第 3 列，因此是 ALT3。
  - **但 p.151–154 是旋转排版的表格，文本抽取后列对齐不可靠**。写入代码前，请在 PDF 原表中逐格核对 ALT 号。
- **RX 引脚与中断/滤波器共用**：RSCAN0RX0/1/2 分别与 INTP5/INTP6/INTP10 共用数字噪声滤波器（FCLA/DNFA 系列寄存器）(HW-E p.158)。CAN RX 是否需要配置这个滤波器，需根据实际芯片手册 §2.6 确认。
- **板卡实际用哪组引脚、收发器 STB/EN 引脚是哪个**，手册无法给出，需看原理图。

---

## 7. 定时器（GPT / ICU 相关）

### 7.1 OSTM

- 共 7 个单元：OSTM0、OSTM1、OSTM3–7（**没有 OSTM2**），都是 32 位定时器，有 interval 和 free-running compare 两种模式 (HW-E p.1542)。
- 基址：OSTM0 `FFDD_8000`，OSTM1 `FFDD_9000`，OSTM3–7 从 `FFD7_0000` 起按 `+40H` 递增 (HW-E p.1543)。
- 寄存器偏移：CMP+00、CNT+04、TO+08、TOE+0C、TE+10、TS+14、TT+18、CTL+20；时钟选择 IC0CKSEL0/1 在 `FFDD_6000/6004` (HW-E p.1551)。
- CTL.MD1：0=interval，1=free-run compare；CTL.MD0：计数开始时是否产生中断。只有 TE=0 时才能写 CTL (HW-E p.1556)。
- 计数时钟是 PCLK=CLK_HSB (HW-E p.1543)。OSTM0/1 可以改用 TAUD/TAUJ 提供的计数使能信号（IC0CKSELn），切换必须在定时器停止时进行 (HW-E p.1547–1548)。
- OSTM0/1 用 EI 中断 74/75；OSTM3–7 只能用 **FEINT**，用于 timing protection 监视 (HW-E p.1544–1545)。所以做 OS tick 或 Gpt 一般选 OSTM0/1。

### 7.2 TAUD / TAUJ / ENCA（ICU 可用的硬件）

| 定时器 | 规格 | 基址 | 出处 |
|---|---|---|---|
| TAUD0–2 | 3 个单元，每个 16 通道，16 位 | `FFE2_0000` / `FFE2_1000` / `FFE2_2000` | HW-E p.1571–1572；DS-E p.3 |
| TAUJ0–2 | 3 个单元，每个 4 通道，32 位 | `FFE5_0000` / `FFE5_1000` / `FFE5_2000` | HW-E p.1878–1879；DS-E p.3 |
| ENCA | 2 个单元（编码器） | — | DS-E p.3；HW-E p.2241 |

- 时钟都是 CLK_HSB (HW-E p.1572, p.1879)。
- TAUD 可用于 **ICU** 的功能：
  - TTIN Input Interval Timer (p.1651)
  - External Event Count (p.1664)
  - Input Pulse Interval Measurement (p.1679)
  - Input Signal Width Measurement (p.1687)
  - Input Position Detection (p.1696)
  - Input Period Count Detection (p.1701)
- TAUJ 有类似的输入测量功能 (p.1933, p.1939, p.1948, p.1957, p.1962)。
- 这些功能都要求为通道输入引脚配置噪声滤波器 (HW-E p.1574 Note 2)。
- 外部中断 **INTP0–12**（边沿可选）也可用于 ICU 的边沿检测，检测方式在 FCLAnCTLm 中设置 (HW-E p.75, p.280, p.293)。

---

## 8. CAN 外设：RS-CANFD（重点）

### 8.1 概况

- 名称为 **RS-CANFD**。P1M-E 只有 1 个单元 RSCFD0，含 3 个通道 CAN0–2，基址 **`0xFFD2_0000`** (HW-E p.788, p.791)。
- 有两种接口模式，**寄存器映射不同**，由 GRMCFG.RCMC 选择 (HW-E p.789, p.796, p.802)：
  - **Classical CAN mode**：寄存器名为 RSCANnXXX。
  - **CAN FD mode**：寄存器名为 RSCFDnCFDXXX。
- GRMCFG `+04FCH` 只能在 global reset 模式下修改，而且必须先于其他 CAN 寄存器设置 (HW-E p.802)。
- 规格 (HW-E p.794–795)：
  - Classical 最高 1 Mbps；FD 模式仲裁段最高 1 Mbps、数据段最高 8 Mbps。
  - 每通道 16 个 TX buffer（共 48 个）。
  - 0–48 个 RX buffer（全部通道共享）。
  - 8 个 RX FIFO（共享），每个最多 128 级。
  - 每通道 3 个 TX/RX FIFO（全模块 k=0–8）。
  - 每通道 1 个 transmit queue。
  - 全模块 192 条接收规则，每通道最多 128 条。
  - 共 11 个中断源。
- 各索引的取值范围 (HW-E p.789–790)：

| 索引 | 范围 | 每通道分配 |
|---|---|---|
| j（规则页内序号） | 0–15 | — |
| k（TX/RX FIFO） | 0–8 | 通道 m 使用 3m 到 3m+2 |
| x（RX FIFO） | 0–7 | 全模块共享 |
| q（RX buffer） | 0–47 | — |
| p（TX buffer） | 0–47 | 通道 m 使用 16m 到 16m+15 (p.800) |

### 8.2 主要寄存器偏移（Classical CAN mode，相对 `0xFFD2_0000`）

| 类别 | 寄存器 | 偏移 | 出处 |
|---|---|---|---|
| 通道 | CmCFG / CmCTR / CmSTS / CmERFL | +0000H / +0004H / +0008H / +000CH，再加 10H×m | HW-E p.798 |
| 全局 | GCFG / GCTR / GSTS / GERFL / GTSC | +0084H / +0088H / +008CH / +0090H / +0094H | HW-E p.798 |
| 接收规则 | GAFLECTR / GAFLCFG0 | +0098H / +009CH | HW-E p.798 |
| 接收规则 | GAFLIDj / GAFLMj / GAFLP0_j / GAFLP1_j | +0500H / +0504H / +0508H / +050CH，再加 10H×j | HW-E p.798 |
| RX buffer | RMNB / RMNDy | +00A4H / +00A8H + 4y | HW-E p.798 |
| RX buffer | RMIDq / RMPTRq / RMDF0_q / RMDF1_q | +0600H 起，每个 10H×q | HW-E p.798 |
| RX FIFO | RFCCx / RFSTSx / RFPCTRx | +00B8H / +00D8H / +00F8H，再加 4x | HW-E p.798 |
| RX FIFO | RFIDx / RFPTRx / RFDF0_x / RFDF1_x | +0E00H 起，每个 10H×x | HW-E p.798–799 |
| TX/RX FIFO | CFCCk / CFSTSk / CFPCTRk | +0118H / +0178H / +01D8H，再加 4k | HW-E p.799 |
| FIFO 状态 | FESTS / FFSTS / FMSTS / RFISTS / CFRISTS / CFTISTS | +0238H / +023CH / +0240H / +0244H / +0248H / +024CH | HW-E p.799 |
| TX buffer | TMCp（**8 位**） / TMSTSp（**8 位**） | +0250H + p / +02D0H + p | HW-E p.799, p.878, p.880 |
| TX buffer | TMIDp / TMPTRp / TMDF0_p / TMDF1_p | +1000H 起，每个 10H×p | HW-E p.799 |
| TX 状态 | TMIECy / TMTRSTSy / TMTCSTSy | +0390H / +0350H / +0370H，再加 4y | HW-E p.799 |
| 其他 | TXQCCm / THLCCm / GTINTSTS0 / GLOCKK / CANFDMDR | +03A0H / +0400H / +0460H / +047CH / +8000H | HW-E p.799 |

- **CAN FD mode 下部分偏移不同** (HW-E p.916–919)：
  - CmNCFG 在 +0000H + 10H×m；CmDCFG 在 +0500H + 20H×m；CmFDCFG 在 +0504H + 20H×m。
  - GAFL 系列移到 +1000H 起。
  - TMIDp 移到 +4000H + 20H×p。
  - RFIDx 移到 +3000H + 80H×x。
  - GCTR、GSTS 等全局寄存器的偏移不变。
  - **两套偏移不能混用。**

### 8.3 模式：global / channel

- **Global 模式**：stop、reset、test、operating。
  - 由 GCTR.GSLPR 和 GMDC[1:0] 控制，GMDC：00=operating，01=reset，10=test，11=禁止。
  - GCTR 复位值 `0x5`，即复位后处于 **global stop** (HW-E p.819, p.1062, p.1064)。
- **Channel 模式**：stop、reset、halt、communication。
  - 由 CmCTR.CSLPR 和 CHMDC[1:0] 控制，CHMDC：00=communication，01=reset，10=halt，11=禁止。
  - CmCTR 复位值 `0x5`，即复位后处于 **channel stop** (HW-E p.805–806, p.1065–1066)。
- **全局模式会强制改变通道模式**：
  - 进入 global reset 时，所有通道被强制进入 channel reset。
  - 进入 global test 时，处于 communication 的通道变为 halt (HW-E p.1063–1064)。
- **状态确认**：
  - 全局：GSTS 的 GRAMINIT(b3)、GSLPSTS(b2)、GHLTSTS(b1)、GRSTSTS(b0)，复位值 `0xD` (HW-E p.821)。
  - 通道：CmSTS 的 CSLPSTS(b2)、CHLTSTS(b1)、CRSTSTS(b0)，复位值 `0x5`。
  - CmSTS.COMSTS(b7) 在检测到 **11 个连续隐性位** 后才置 1，此时节点才真正可以收发 (HW-E p.810–811, p.1068)。
  - 手册要求每次模式切换后都检查这些状态位 (HW-E p.1122)。
- 最长切换时间（用于设置超时）(HW-E p.1063, p.1066)：

| 切换 | 最长时间 |
|---|---|
| global reset → operating | 10 个 pclk |
| global operating → reset | 2 个 CAN bit time |
| channel reset → communication | 4 个 bit time |
| channel communication → reset | 2 个 bit time |
| channel communication → halt | 2 个 CAN frame |

- **只在 global reset 中才能写的内容**：GCFG (p.817)、GAFLCFG0 和接收规则表 (p.831–837)、RMNB (p.838)、RFCCx 中除 RFE/RFIE 以外的位 (p.845)、GCTR 的中断使能位 (p.820)。
- **只在 channel reset（或 halt）中才能写的内容**：CmCFG（在 reset 或 halt 中修改，首次必须在 reset 中设置，p.804）、CmCTR 中的 BOM 和各中断使能位（只能在 channel reset 中改，p.807–808）。
- **Bus-off 处理**：CmCTR.BOM 选择恢复策略 (HW-E p.805, p.807, p.1069)：
  - 00：按 ISO11898-1 自动恢复（检测到 128 次 11 个连续隐性位后恢复）。
  - 01：进入 bus-off 时自动转 halt。
  - 10：bus-off 恢复结束时自动转 halt。
  - 11：由程序请求转 halt。
  - RTBO=1 可以强制从 bus-off 返回，只允许在 BOM=00 时使用 (HW-E p.809)。
- **错误计数器**：CmSTS.TEC[31:24]、REC[23:16]；EPSTS 表示 error passive，BOSTS 表示 bus-off（TEC>255）(HW-E p.810–811)。
- **错误标志**：CmERFL 中写 0 清除对应标志，其余位要写 1（不能直接写全 0）(HW-E p.812–815)。

### 8.4 位时间（Classical mode，CmCFG）

| 字段 | 位置 | 编码 | 出处 |
|---|---|---|---|
| SJW[1:0] | b25:24 | 值+1 Tq（1–4） | HW-E p.803–804 |
| TSEG2[2:0] | b22:20 | 值+1 Tq（2–8，000 禁止） | HW-E p.803–804 |
| TSEG1[3:0] | b19:16 | 值+1 Tq（4–16，0–2 禁止） | HW-E p.803–804 |
| BRP[9:0] | b9:0 | 分频 P+1 | HW-E p.803–804 |

- 约束：SS=1 Tq；总长 8–25 Tq；TSEG1 > TSEG2 ≥ SJW（Figure 17.17 写成 TSEG1 > TSEG2 > SJW）(HW-E p.804, p.1092)。
- 公式：通信速率 = fCAN / ((BRP+1) × 每位 Tq 数) (HW-E p.794, p.1094)。
- 手册示例：fCAN=40 MHz 时，500 kbps 可取 8 Tq（分频 10）或 20 Tq（分频 4）(HW-E p.1095)。
- **按上述编码规则算出的示例（不是项目配置，只演示编码）**：
  - 条件：DCS=0，fCAN=40 MHz，500 kbps，20 Tq，TSEG1=15 Tq，TSEG2=4 Tq，SJW=1 Tq，采样点 80%。
  - 字段值：BRP=3，TSEG1 字段=14，TSEG2 字段=3，SJW 字段=0。
  - 结果：`CmCFG = 0x003E_0003`。
- FD 模式改用 NCFG 和 DCFG：
  - NCFG：NTSEG2[28:24]、NTSEG1[22:16]、NSJW[15:11]、NBRP[9:0] (HW-E p.921)。
  - DCFG：DSJW[26:24]、DTSEG2[22:20]、DTSEG1[19:16]、DBRP[7:0] (HW-E p.935)。
  - NBRP 必须等于 DBRP；使能 TDC 时两者都必须 ≤1 (HW-E p.1092, p.1099)。
  - 只收发 classical 帧时，把 DCFG 设成与 NCFG 相同 (HW-E p.1122)。

### 8.5 接收：规则表 + buffer / FIFO

- **规则表 (AFL)**：
  - GAFLCFG0.RNC0/1/2（b31:24、b23:16、b15:8）设置各通道的规则数，每通道 ≤ `80H`，全模块合计 ≤ 192 (HW-E p.831, p.1072)。
  - 各通道的规则必须连续存放，**不能跨通道共享** (HW-E p.1072)。
  - **没有设置任何规则时，收不到任何报文** (HW-E p.1072)。
  - GAFLECTR.AFLDAE=1 打开写入，AFLPN 选择页 0–11，每页 16 条 (HW-E p.830)。
- **每条规则由 4 个寄存器组成**：
  - **GAFLIDj** (HW-E p.832)：IDE(b31)、RTR(b30)、LB(b29，镜像功能)、ID[28:0]。标准帧 ID 写在 b10–0。
  - **GAFLMj** (HW-E p.834)：IDEM(b31)、RTRM(b30)、IDM[28:0]。**掩码位为 1 表示比较，为 0 表示不比较**，因此 IDEM=0 时要求 IDM 全 0（即通配）。
  - **GAFLP0_j** (HW-E p.835)：DLC[31:28]（DLC 过滤阈值，0 表示不检查）、PTR[27:16]（12 位 label）、RMV(b15)（是否存入 RX buffer）、RMDP[14:8]（RX buffer 号）。
  - **GAFLP1_j** (HW-E p.837)：FDP[16:8] 选择 TX/RX FIFO k，FDP[7:0] 选择 RX FIFO x。最多可路由到 8 个目的地。
- **匹配规则**：从小号规则开始依次比较，命中第一条就停止。如果随后的 DLC 检查失败，报文直接丢弃，并置 GERFL.DEF (HW-E p.1073–1074, p.1122)。
- **RX buffer** (HW-E p.1072, p.838–839)：
  - 收到新报文时直接覆盖，因此总能读到最新值。
  - RMNB.NRXMB 设定数量 0–48。
  - 新数据标志在 RMNDy.RMNSq。
- **RX FIFO** (HW-E p.844–846)：
  - RFCCx：RFIGCV[15:13]（中断触发水位）、RFIM(b12)（1=每收一帧就中断）、RFDC[10:8]（深度：001=4，010=8，011=16，100=32，101=48，110=64，111=128）、RFIE(b1)、RFE(b0)。
  - RFSTSx：RFMC[15:8] 未读条数、RFIF(b3)、RFMLT(b2) 丢帧、RFFLL(b1) 满、RFEMP(b0) 空。
- **RAM 容量约束**：Classical 模式下，RX buffer 数 + 所有 RX FIFO 深度之和 + 所有 TX/RX FIFO 深度之和 ≤ 192（共 3072 字节）(HW-E p.1097)。

### 8.6 发送：TX buffer / TX/RX FIFO / TX queue

- **TX buffer**：每通道 16 个，编号 16m 到 16m+15 (HW-E p.800, p.1076)。
  - **TMCp**（8 位）：TMOM(b2) 单次发送、TMTAR(b1) 中止请求、TMTR(b0) 发送请求（只能写 1，由硬件清 0）。只能在 channel communication 或 halt 模式下修改 (HW-E p.878–879)。
  - **TMSTSp**（8 位）：TMTARM(b4)、TMTRM(b3)、TMTRF[2:1]（00=发送中或无请求，01=中止完成，10=发送完成，11=带中止请求的发送完成）、TMTSTS(b0)。TMTRF 只能写 00 来清除 (HW-E p.880–881)。
  - **TMIDp**：IDE(b31)、RTR(b30)、THLEN(b29)、ID[28:0]。**TMPTRp**：DLC[31:28]、label[23:16]。**TMDF0/1_p**：数据字节 0–7。这几个寄存器只能在 TMTRM=0 时写 (HW-E p.882–887)。
- **TX/RX FIFO（transmit 模式）**：链接到某个 TX buffer（CFTML 字段选择）。被链接的 TX buffer，其 TMCp 必须保持 00H，对应 TMIE 置 0 (HW-E p.800, p.878, p.1122)。
- **TX queue**：占用每通道最高编号的若干 TX buffer。使用 queue 时 GCFG.TPRI 必须为 0（按 ID 优先级）(HW-E p.801, p.818, p.1077)。
- **发送优先级**：GCFG.TPRI=0 时按 ID 仲裁优先；=1 时按 buffer 号小者优先。该设置对所有通道生效 (HW-E p.1077)。

### 8.7 寄存器级初始化序列（手册 Figure 17.16 原顺序，Classical mode）

依据 HW-E p.1090–1091。其中“确认”步骤来自 p.1122 的要求；每一步的写入时机限制见括号内页码。

0. （前置）按 §6.2 配置 CAN TX/RX 引脚；确认 CLK_LSB/MainOSC 正常 (HW-E p.791)。
1. 轮询 **GSTS.GRAMINIT == 0**，即 CAN RAM 初始化完成，最长约 3794 个 pclk (HW-E p.1090, p.821)。
2. **GCTR.GSLPR = 0**：global stop → global reset；确认 GSTS.GSLPSTS=0 且 GRSTSTS=1 (HW-E p.1091, p.819, p.1122)。
3. **GRMCFG.RCMC**：0=classical，1=FD（只能在 global reset 中写，并先于其他寄存器）(HW-E p.802)。
4. **CmCTR.CSLPR = 0**：channel stop → channel reset；确认 CmSTS.CSLPSTS=0 且 CRSTSTS=1 (HW-E p.1091, p.809)。
5. **GCFG**：DCS（时钟源）、TPRI、DCE/DRE、MME、时间戳、EEFE（global reset 中写）(HW-E p.816–818)。
6. **CmCFG**：位时间（channel reset 中写）(HW-E p.804)。FD 模式改写 NCFG/DCFG。
7. **接收规则**：GAFLCFG0 设置 RNCm → GAFLECTR.AFLDAE=1 → 设 AFLPN → 写 GAFLIDj/Mj/P0_j/P1_j → 所有页写完后 AFLDAE=0 (HW-E p.1096)。
8. **Buffer 设置**：RMNB → RFCCx（RFIGCV/RFIM/RFDC，此时**不要**置 RFE）→ CFCCk → TXQCCm → THLCCm → 各中断使能（RFIE、CFTXIE/CFRXIE、TAIE、TMIEC、TXQIE、THLIE）(HW-E p.1098)。
9. **GCTR**：全局错误中断 DEIE/MEIE/THLEIE (HW-E p.1091, p.820)。
10. **CmCTR**：通道错误中断使能、BOM、ERRD（channel reset 中写）(HW-E p.1091, p.807–808)。FD 模式另外设置 CmFDCFG。
11. **INTC**：设置 EIC183–193 中实际使用的通道（优先级、EITB、EIMK=0），EIBD 保持 PEID=001 (HW-E p.1091, p.267, p.271)。
12. **GCTR.GMDC = 00**：进入 global operating；确认 GSTS.GRSTSTS=0 (HW-E p.1091, p.1064)。
13. 对要使用的 RX FIFO **单独执行一次写 RFCCx.RFE=1**（必须在 global operating 或 test 中，且在其他位设置完成后用另一条指令写）(HW-E p.845)。
14. **CmCTR.CHMDC = 00**：进入 channel communication；确认 CmSTS 的 CRSTSTS 和 CHLTSTS 都为 0。之后等待 **COMSTS=1**，才算可以通信 (HW-E p.1091, p.811, p.1068)。

注：步骤 13 的位置是根据 p.845 的写入时机限制补充的，Figure 17.16 本身没有画出这一步。

### 8.8 发送序列（TX buffer，Figure 17.27 及相关寄存器说明）

1. 前提：通道处于 communication 模式且 COMSTS=1 (HW-E p.878, p.811)。
2. 确认 **TMSTSp.TMTRM=0**（没有挂起的请求）且 **TMTRF=00B** (HW-E p.879, p.882, p.1110)。
3. 写 TMIDp（ID/IDE/RTR）、TMPTRp（DLC）、TMDF0_p、TMDF1_p (HW-E p.1107)。
4. 用 8 位写 **TMCp = 0x01**（TMTR=1；需要单次发送时同时置 TMOM）(HW-E p.878, p.1107)。
5. 完成判定：TMSTSp.TMTRF=10B。如果 TMIEp=1，会产生 INTRCANmTRX，GTINTSTS0.TSIFm 置 1 (HW-E p.1109, p.827)。
6. 在 ISR 或轮询中，**把 TMSTSp.TMTRF 写回 00B**，这样才清除中断请求，也才能再次发送 (HW-E p.1109–1110)。
7. 中止发送：置 TMCp.TMTAR=1，等待 TMTRF=01B（若 TAIE=1 会产生中断）。正在发送中的帧无法中止 (HW-E p.879, p.1111)。

### 8.9 接收序列

**RX FIFO**（对应中断 INTRCANGRECC，通道 190）：
1. 由 RFISTS 或 RFSTSx.RFIF 判断是哪个 FIFO (HW-E p.799, p.846)。
2. 循环执行：当 RFSTSx.RFEMP=0 时，读 RFIDx、RFPTRx（DLC/时间戳/label）、RFDF0_x、RFDF1_x，然后写 **RFPCTRx = 0xFF** 让读指针前进 (HW-E p.848, p.1102)。
3. 清除 RFIF：对 RFSTSx 写值时，RFIF 位写 0，其余可写标志位写 1 (HW-E p.847)。
4. 写完 store 后按 dummy read → SYNCP 的顺序再执行 EIRET (HW-E p.254)。

**TX/RX FIFO（receive 模式）**：流程同上，判断 CFEMP，读 CFIDk 等寄存器，写 CFPCTRk=0xFF（Figure 17.25）(HW-E p.1102)。

**RX buffer**：如果 RMNDy.RMNSq=1，**先把 RMNSq 清 0**，再读 RMIDq、RMPTRq、RMDF0/1_q；全部 RMNS 为 0 后结束。这个顺序保证读到的数据一致 (HW-E p.1100)。

### 8.10 其他注意事项

- 从 FD 模式切回 classical 模式前，必须先把只属于 FD 映射的寄存器恢复为复位值 (HW-E p.802, p.1122)。
- 指向 RAM 的寄存器（规则表、各 buffer）在 CAN RAM 初始化完成前的值未定义 (HW-E p.1123)。
- FIFO 满时新到的报文被丢弃（RFMLT/CFMLT 置位）(HW-E p.1122)。
- 进入 channel reset 会清除 TEC/REC、错误标志和 TMC/TMSTS 等（Table 17.180）(HW-E p.1070)。
- 测试功能：listen-only、自测试 0/1（外部/内部回环）、RAM test。CTMS/CTME 只能在 halt 模式中修改 (HW-E p.805, p.807, p.1085–1087)。

---

## 9. 硬件概念 → AUTOSAR MCAL 映射提示

| RH850/P1M-E 硬件 | AUTOSAR 概念 | 说明 / 依据 |
|---|---|---|
| RS-CANFD 单元 RSCFD0 | Can Driver 实例 | 3 个通道共用一套 global 配置（GCFG、规则表、RX FIFO）(HW-E p.788, p.817) |
| 通道 CANm | CanController | 每个 controller 对应一组 CmCFG/CmCTR/CmSTS (HW-E p.798) |
| TX buffer p（16m..16m+15） | **CanHardwareObject (TRANSMIT) → HTH** | 一个 buffer 对应一个 HTH 是常见做法。用 TX queue 或 TX/RX FIFO 实现 HTH 的 FIFO 语义，取决于 MCAL 实现，需看供应商文档 (HW-E p.1076) |
| 接收规则（GAFLID/M/P0/P1）+ RX FIFO 或 RX buffer | **CanHardwareObject (RECEIVE) → HRH**；CanHwFilter（ID/Mask） | 规则表是硬件过滤器。**硬件 Mask=1 表示比较**，与某些工具里 “mask=0 精确匹配” 的语义相反，需要检查 MCAL 生成代码 (HW-E p.834) |
| RX buffer（覆盖式） vs RX FIFO（排队式） | HRH 的 Basic/Full CAN 风格 | 前者只保留最新值，后者按 FIFO 排队 (HW-E p.1072) |
| channel communication / halt / reset / stop | Can_SetControllerMode：STARTED / STOPPED / SLEEP | 一种可能的映射：STARTED=communication，STOPPED=reset 或 halt，SLEEP≈stop（RS-CANFD 没有 CAN 唤醒机制的描述，SLEEP 语义需按 MCAL 确认）(HW-E p.1062, p.1065) |
| global reset → operating | Can_Init 内部步骤 | 规则表、FIFO 配置只能在 global reset 中写，所以必须在 Can_Init 中完成 (HW-E p.817, p.831) |
| CmSTS.BOSTS / BOEF + BOM | CanIf_ControllerBusOff → CanSM 恢复 | BOM 的设置要与 CanSM 的恢复策略一致 (HW-E p.807, p.1069) |
| CmSTS.TEC/REC、EPSTS | Can_GetControllerErrorState / Rx/TxErrorCounter | (HW-E p.810) |
| TMSTSp.TMTRF=10B + INTRCANmTRX | CanIf_TxConfirmation | (HW-E p.1109) |
| RFSTSx / INTRCANGRECC | CanIf_RxIndication（中断或轮询方式） | (HW-E p.1102) |
| EICn（EIP、EITB、EIMK）+ 向量 | OS ISR Category 2 配置（优先级、向量表、使能） | OS 端口负责生成向量表/INTBP 和 EIC 配置；EIP 0 为最高优先级。OS 的优先级编号与 EIP 的换算需看 OS 端口文档 (HW-E p.268, p.281) |
| PSW.ID、PMR、ISPR | SuspendAllInterrupts / SuspendOSInterrupts | OS 用 DI/EI 或 PMR 实现临界区，具体由 OS 端口决定 (HW-E p.198, p.211) |
| （P1M-E 无软件可配 PLL） | Mcu_InitClock / Mcu_DistributePllClock | P1M-E 的 PLL 设置项可能为空，或仅做状态检查。“PROTCMD 0xA5”在 P1M-E 上只适用于 CLMA、ECM、FLMD（§4.4）(HW-E p.471, p.2764) |
| CLMAn（时钟监视） | Mcu/WdgM 安全监控 | 用 CLMAnPCMD 的 0xA5 序列使能 (HW-E p.2764) |
| RESF / SWSRESA0 / SWARESA0 | Mcu_GetResetReason / Mcu_PerformReset | (HW-E p.420–422) |
| STAC_* RAM 初始化 | Mcu_InitRamSection / 启动代码 | 硬件已在复位时清零并写 ECC (HW-E p.2890) |
| PMC/PFC/PFCE/PFCAE/PM/PIPC | Port_Init（PortPinMode = CAN_TX/RX） | (HW-E p.94, p.127–131) |
| Pn/PSRn/PPRn | Dio_WriteChannel / Dio_ReadChannel | 用 PSRn 可以原子地修改单个位 (HW-E p.96) |
| OSTM0/1 | Gpt channel / OS Counter 硬件 | OSTM3–7 只能接 FEINT，不适合做 Gpt 通知 (HW-E p.1544) |
| TAUD/TAUJ 输入测量、INTP | Icu（edge detect、timestamp、signal measurement） | (HW-E p.1679, p.1687, p.280) |
| WDTA0 + OPBT0 | Wdg driver | 复位后是否自动运行由 option byte 决定 (HW-E p.2884) |

---

## 10. 待确认清单（真实项目必须核实）

1. **芯片型号**：是否就是 R7F701381（P1M-E，100-pin，DPS，1 MB）？读 PRDNAME1–4 或核对 BOM (HW-E p.2880)。如果是 P1x-C，CAN 为 MCAN，本文 §8 全部不适用 (DS-C p.4)。
2. **PLL/CPU 时钟**：由谁、何时完成锁定，是否需要软件等待；Main OSC 晶振的实际规格。需根据实际芯片手册/启动代码确认（HW-E §12 没有 PLL 软件寄存器）。
3. **RH850G3M Software Manual 中的内容**：异常向量偏移（SYSERR/TRAP/FPE/MIP/MDP 等）、异常受理时 PSW 各位的变化、SYNCP/SYNCI 的精确要求、EIRET/FERET 的语义。
4. **工具链 ABI**：r2/r4/r5/r30 的实际用法、栈对齐、ISR 是否保存 FPU 上下文（`-fsoft` 等编译选项）。
5. **Option bytes 实际值**：OPWDRUN/OPWDOVF/OPWDMDS、OPJTAG (HW-E p.2884–2886)，以及 reset vector / variable reset vector 的设置 (HW-E p.2865)。
6. **RAM 初始化配置**：STAC_* 的实际设置，以及 Application Reset 后哪些 RAM 区需要保留 (HW-E p.427–430)。
7. **CAN 引脚**：板卡实际使用哪组 RX/TX、各自的 ALT 号（需对照 PDF 原表 p.151–154 逐格核对），RX 噪声滤波器设置 (HW-E p.158)，收发器 STB/EN/ERR 引脚及有效电平。
8. **CAN 接口模式**：classical（RCMC=0）还是 FD（RCMC=1）？MCAL 选了哪一套？两者寄存器偏移不同 (HW-E p.916–919)。
9. **fCAN 选择**：DCS=0（40 MHz）还是 1（16 MHz），以及实际的 BRP/TSEG/SJW、采样点。Figure 17.17 写的是严格不等式 “TSEG1>TSEG2>SJW”，而寄存器说明允许 SJW≤TSEG2 (HW-E p.804, p.1092)，两处不一致，需结合网络规范确认。
10. **HTH/HRH 到硬件资源的映射**：哪些 TX buffer、哪个 RX FIFO、规则的数量与顺序；MCAL 中 mask 语义与硬件 GAFLM 的对应关系 (HW-E p.834)。
11. **CAN 中断检测方式**：读 EIC183–193.EICT 确认是否为电平型 (HW-E p.267)；OS 中 ISR 的类别、优先级、向量方式（direct 还是 table/INTBP）。
12. **BOM 策略**：与 CanSM bus-off 恢复（时间、次数）是否一致 (HW-E p.807)。
13. **PBG/IPG/PEG/GRG 等 guard 配置**：是否会阻止 MCAL 写时钟、复位或 CAN 寄存器 (HW-E p.260, p.471)。
14. **Flash 自编程/FACI 细节**（Fls/Fee/Bootloader）：需要 P1M-E Flash Memory User's Manual: Hardware Interface，本仓库中没有 (HW-E p.2888)。
15. **ECM 配置**：哪些错误映射为 FENMI/EI 中断或复位 (HW-E p.432；Section 32)。
