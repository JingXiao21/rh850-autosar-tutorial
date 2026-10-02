# RH850/P1M-E 首轮硬件核查报告

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**（RH850 + AUTOSAR 中文教程），不是公司真实工程；文中“内部 agent / 内部工程 / 交付”等说法只是当时的写作语境，不代表任何真实公司项目、真实配置或指令。正文以下保持原样，仅作溯源；学习请以新教程章节为准。
>
> - **已复审并迁移到**：存储/时钟 → [01-rh850/03](01-rh850/03-memory-map.md)、[01-rh850/07](01-rh850/07-clock-system.md)；中断表 → [01-rh850/06](01-rh850/06-interrupt-exception.md)、[04-can-mcal/05](04-can-mcal/05-can-interrupt.md)；CAN 位时间 → [04-can-mcal/03](04-can-mcal/03-can-clock-bit-timing.md)；OSTM → [03-mcal/05](03-mcal/05-gpt-driver.md)；寄存器总表 → [03-mcal/07](03-mcal/07-rh850-hardware-mapping.md)。
> - **已知更正**：① 原 :86 的 EI184 标签已就地补注——EI184 是 CAN0 common（Tx/Rx）FIFO 接收中断，**不是** Rx FIFO；Rx FIFO0–7 = **EI190**（HW-E p.285–286、p.792）；② 文中“已完成内部 agent 解说交付”的语境不适用于本教育项目；③ 无 SWS 映射。其余抽查事实与手册一致。详见 [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) §3、§4 与 [05-consistency-review-log.md](reference/research/05-consistency-review-log.md)。
> - **行号说明**：本 banner 在第 2 行之后插入了 6 行。其他文档（如研究笔记 01、`analysis-and-learning-plan.md`）引用的本文件行号均指插入前版本，对应当前行号 **+6**。

2026-10-01 追加复审：CAN/诊断/OS 配置请优先使用 [详细硬件交接](rh850-hardware-handoff.md) 和 [复审记录](hardware-review.md)，包括 Classic/FD 地址区分、具体 ALT/PIPC、访问宽度与初始化顺序。

本文件保留首轮硬件证据；后续补充的完整需求解答见 [agent-guide.md](agent-guide.md)，计数器方案建议见 [counter-design.md](counter-design.md)。当前已完成内部 agent 解说交付，以下“待集成”仅指未来真实工程实施。

日期：2026-10-01。资料 ID 见 [source-index.md](source-index.md)。页码为 PDF 页码，所引技术页面与印刷页码一致。下面“已确认”表示手册依据已核实，不代表已测量目标板；工程当前值来自截图，实际工程未提供。

## 1. 应优先处理的差异

| 项目 | 截图现有值 / 假设 | 已核查结果 | 来源 / 置信度 |
| --- | --- | --- | --- |
| A2 Code Flash | `iROM 2048K@0x0` | R7F701381 是 1 MB 型号；用户区为 `0x00000000～0x000FFFFF`。主工程拿到后应修正容量并核查 map | DS-E §1.3 pp.2–3；HW-E §4.1 p.257 / 已确认 |
| A3/A4 fCAN | 约 80 MHz | `pclk=80 MHz` 是接口时钟；位时间源由 DCS 选择 `clkc=40 MHz` 或 `clk_xincan=16 MHz`。不能直接使用 80 MHz | HW-E §17.1.3 p.791、§17.4.4.1 p.949 / 已确认 |
| D3 掩码语义 | `mask=0` 精确匹配 | 硬件 `GAFLIDM` 位为 0 时不比较，为 1 时比较。MCAL 参数是否反相转换尚未知，需检查生成代码及寄存器值 | HW-E §17.4.5.4 p.968 / 硬件已确认；配置映射未确认 |
| A1 FPU | G3M？有无 FPU？`-fsoft`？ | G3M 与双精度 FPU 已确认；`-fsoft` 是否符合库 ABI、OS 上下文策略和编译选项，不能仅由“有 FPU”决定 | DS-E §1.1 p.1、§1.3 p.2 / 硬件已确认；工具链未确认 |
| E2 RAM 执行 | FCU 擦写时必须离开 Code Flash？ | Data Flash 擦写支持从 Code Flash 执行的 BGO；Code Flash 自编程与 Data Flash 操作要分别判断。具体 Fls 库仍需遵守其要求 | HW-E §35.1 p.2857、§35.7 p.2869 / 已确认 |
| G4 RAM/ECC | 启动时必须全部软件清零？ | LRAM/GRAM 等提供硬件初始化并设置 ECC；手册称各类复位初始化可受 RAM 初始化模式寄存器控制，应核查配置后决定软件动作 | HW-E §36.2.1.4 p.2890 / 已确认；板上控制值未确认 |
| B2/F WDTA 实例 | WDTA0/1 | P1M-E 仅有 WDTA0；不能给 WDTA1 分配真实外设资源 | HW-E §21.1 pp.1522–1523 / 已确认 |
| C4 时间换算 | 硬件计数除以频率得到 16 位 tick | 80 MHz 下硬件 32 位约 53.6870912 s 回绕，而 OS 1 ms/16 位为 65.536 s；直接除法再截断会破坏连续性 | HW-E §22；基于位宽/频率的计算 / 已确认计算，OS 实现待集成 |

## 2. 器件、内存与时钟基线

器件：R7F701381，LFQFP100（14×14 mm），DPS，G3M 主核 + 检查核锁步。这里的检查核不等于第二个可独立调度的应用核。订货温度/封装完整后缀仍需读取实物/BOM。依据 DS-E pp.1–3。

| 区域 | 容量 | 地址范围 | 依据 |
| --- | --- | --- | --- |
| Code Flash 用户区 | 1 MB | `0x00000000～0x000FFFFF` | HW-E p.257，1 MB 型号注释 |
| Code Flash 扩展用户区 | 32 KB | `0x01000000～0x01007FFF` | HW-E p.257；用途/工程分配仍待确认 |
| Data Flash | 32 KB | `0xFF200000～0xFF207FFF` | HW-E pp.257、2859 |
| 本地 RAM（self） | 128 KB | `0xFEDE0000～0xFEDFFFFF` | HW-E pp.257–258 |
| 本地 RAM（PE1 别名） | 同一块 128 KB | `0xFEBE0000～0xFEBFFFFF` | HW-E pp.257–258；不能与 self 区重复计入容量 |
| Global RAM Bank A | 32 KB | `0xFEEF8000～0xFEEFFFFF` | HW-E p.257 |
| Global RAM Bank B | 32 KB | `0xFEF00000～0xFEF07FFF` | HW-E p.257 |

截图的 `PLRAM 128K-10K@0xFEDE0000` 与 `stack 10K@0xFEDFD800` 在地址算术上首尾衔接，栈区结束于 `0xFEE00000`（上界不含）。这只确认布局边界，不确认栈容量足够或工程没有段重叠。

| 时钟 | 手册基线 | 工程核查点 |
| --- | --- | --- |
| Main OSC | 16 MHz | 实物晶振与板级时钟输入 |
| CPU | 160 MHz | 启动状态、目标配置；不可直接套用其他 P1x 的 PLL 初始化代码 |
| CLK_HSB | 80 MHz | OSTM 的 PCLK、CAN 的 pclk 等 |
| CLK_LSB | 40 MHz | CAN 的 clkc、Data Flash 等 |
| CLK_IOSC | 8 MHz | 与 WDTA 选项配合 |
| WDTA 计数时钟 | 8 MHz 或 250 kHz | OPWDMDS：0/1 分别对应两种模式 |
| OSTM0/1 计数 | PCLK 或 TAUD/TAUJ 提供的计数使能 | `IC0CKSELn` 与相应 TAUD/TAUJ 预分频；截图 CK0/20 MHz 不能由名称单独确认 |

依据：HW-E §12.2 pp.469–470、§21.5.1 p.1534、§22.1.3 p.1543、§22.2.3 pp.1547–1548。尚无 MCU/Gpt ARXML，不能声称项目已配置为这些值。

## 3. CAN 位时间复算与配置约束

从物理量计算：`bitrate = fCAN / [divider × (1 + TSEG1 + TSEG2)]`；`sample_point = (1 + TSEG1)/(1 + TSEG1 + TSEG2)`。硬件 BRP、TSEG、SJW 字段的编码与这些物理值相差 1，必须区分。依据 HW-E pp.921–922、935–936、1092。

当确认 **DCS=0，fCAN=40 MHz，使用 CAN FD 寄存器接口模式** 时：

| 阶段 | 物理 divider | TSEG1 / TSEG2 / SJW（Tq） | 结果 | 候选寄存器值 |
| --- | --- | --- | --- | --- |
| 标称 | 2 | 31 / 8 / 4 | 500 kbit/s，采样点 80% | `NCFG=0x071E1801` |
| 数据 | 2 | 15 / 4 / 3 | 1 Mbit/s，采样点 80% | `DCFG=0x023E0001` |

**此表是条件成立时的参考候选，尚未写入目标工程或硬件。** TDC 使能时还需符合数据阶段传播延迟补偿的其他配置；当前计算只检查其 prescaler 限制。不能把 NCFG/DCFG 编码用于 classical CAN 接口模式下的 CmCFG。

截图数据阶段为 SJW=4、TSEG2=4。手册寄存器说明允许 SJW 不大于 TSEG2（pp.922、936），但 Figure 17.17 p.1092 写出 `TSEG1 > TSEG2 > SJW`。本报告保留该内部差异，参考校验器采取严格不等式，因此候选数据阶段 SJW 暂用 3；最终以适用勘误/厂商确认及网络时序要求关闭。不能把“原 SJW=4 一定不能工作”作为既定事实。

其他已确认约束：

- NBRP 与 DBRP 必须相同；TDC 使能时两者编码不大于 1，即物理 divider 不大于 2（HW-E pp.922、1092）。
- 若 DCS=1，实际是 16 MHz，截图 40/20 Tq 组合不能直接得到目标速率；参考测试另验证了 16 MHz 下 500k/1000k 的候选组合，采样点分别 81.25%/75%，还需网络需求确认。
- 物理 Tx/Rx buffer 索引与 MCAL “13 个邮箱”不是同一层配置。HW-E p.789 列出索引 0～47，p.1078 说明普通 Tx buffer 支持至 20 字节，merge 模式可用 3 个 buffer 组合到 64 字节。`CanPayloadStorageSize=16` 本身不足以证明映射正确，需检查实际 buffer 分配、DLC 和模式。
- 硬件 ID mask 全零是通配语义；标准帧精确 ID 比较需核查 ID mask、IDE/RTR mask 和 MCAL 参数到寄存器的转换，不能只看一个 `mask` 字段。

## 4. 中断映射

依据 HW-E §6.2.2 pp.267–268、Table 6.11 pp.282–290、§17.1.4 p.792、§22.1.4 p.1544。

| 中断源 | EI 通道 | 表参考偏移（4×通道） | EIC 地址 |
| --- | --- | --- | --- |
| WDTA0 75% | 9 | `0x024` | `0xFFFEEA12` |
| OSTM0 | 74 | `0x128` | `0xFFFFB094` |
| OSTM1 | 75 | `0x12C` | `0xFFFFB096` |
| TAUJ0 ch0 | 133 | `0x214` | `0xFFFFB10A` |
| TAUJ0 ch1 | 134 | `0x218` | `0xFFFFB10C` |
| TAUJ0 ch2 | 135 | `0x21C` | `0xFFFFB10E` |
| TAUJ0 ch3 | 136 | `0x220` | `0xFFFFB110` |
| CAN0 error | 183 | `0x2DC` | `0xFFFFB16E` |
| CAN0 Tx/Rx FIFO（common FIFO）receive completion —— **不是 Rx FIFO**，Rx FIFO0–7 见 EI190 | 184 | `0x2E0` | `0xFFFFB170` |
| CAN0 transmit | 185 | `0x2E4` | `0xFFFFB172` |
| CAN global error | 189 | `0x2F4` | `0xFFFFB17A` |
| CAN receive FIFO（Rx FIFO0–7 共用，INTRCANGRECC） | 190 | `0x2F8` | `0xFFFFB17C` |
| Flash sequencer end | 379 | `0x5EC` | `0xFFFFB2F6` |
| Flash sequencer end error | 383 | `0x5FC` | `0xFFFFB2FE` |

EIC 地址由手册的连续 16 位寄存器布局计算：0～31 通道使用 `0xFFFEEA00 + 2×n`，32～383 使用 `0xFFFFB000 + 2×n`，仅可访问已定义通道。表中地址属于已确认手册规则下的计算结果。

EITB 位选择直接分支或表参考；直接分支的偏移由优先级及 RINT 决定，不能混同于上表的表项偏移。EIP 为 4 位，0 最高、15 最低，复位优先级 15。`OsIsrPriority/IPL_x` 映射仍需 RH850GHS 5.0.39 文档。

截图的 `Interrupt_0228/022C` 与硬件表项偏移 `0x128/0x12C` 不同，但**不能因此直接改 ISR 名称**。ETAS 符号可能使用自身命名规则，必须检查 `Os_ConfigInterrupts.h` 和该版本端口说明。

EIRF（bit12）对同步边沿源允许软件置位/清零，高电平源只读；EIMK（bit7）屏蔽 CPU 请求但不阻止请求标志产生。手册明确提示 EIC 读改写可能丢失或重复中断，后续实现必须评审访问宽度、临界区及外设/CPU 接收时序。本次底层示例未实现 EIC 操作。

## 5. OSTM 与 K1 决策进展

| 项目 | 已确认内容 | 依据 |
| --- | --- | --- |
| 实例 | OSTM0、1、3～7，共 7 个；没有 OSTM2 | HW-E p.1542 |
| 基地址 | 0=`0xFFDD8000`，1=`0xFFDD9000`；3～7 从 `0xFFD70000` 起按 `0x40` 递增 | HW-E p.1543 |
| 中断区别 | 0/1 是 EI 74/75；3～7 接 FEINT，不能当普通备用 EI timer 使用 | HW-E p.1544 |
| 寄存器 | CMP +0x00、CNT +0x04；TO +0x08、TOE +0x0C、TE +0x10、TS +0x14、TT +0x18、CTL +0x20 | HW-E pp.1551–1556 |
| 访问宽度 | CMP/CNT 32 位；TO/TOE/TE/TS/TT/CTL 8 位；IC0CKSEL0/1 为 16 位 | HW-E pp.1552–1559 |
| 间隔模式 | MD1=0，向下计数；重复周期为 `(CMP+1)/counter_hz` | HW-E pp.1553、1562 |
| 自由运行比较 | MD1=1，从 0 向上计数，可运行中更新 CMP；再次启动会重置起点 | HW-E pp.1556、1567 |
| 启动中断 | MD0 控制；本次参考实现置 0 | HW-E p.1556 |
| 时钟选择 | IC0TMEN=0 时选 PCLK；置 1 时选 TAUD/TAUJ 的计数使能链路 | HW-E pp.1547–1548、1557–1560 |

本次 `Ostm_InitPclk` 仅支持 OSTM0/1，要求通道已停止、调用方独占资源并屏蔽相应中断，使用手册 PCLK 路线。80 MHz / 1 ms 的重复周期比较值为 79999；若另行确认计数源为 20 MHz，则是 19999。本次驱动不会自动配置 TAUD/TAUJ，也不能维持截图里未知的 CK0 配置。

K1 当前结论：**硬件支持自由运行比较，硬件路线值得继续；尚未满足选定最终 RTA 实现的全部证据。**

1. `Now` 必须处理硬件回绕和 tick 余数。已实现的 `Tick_Accumulator` 用相邻计数差累计，要求两次采样间隔严格小于一次硬件回绕：80 MHz 下约 53.69 s，20 MHz 下约 214.75 s；并要求调用串行化及计数器重启后重新初始化。
2. 该采样约束不能靠“OS 通常会调用”来假定满足。后续须设计周期维护/溢出处理或经确认的较低计数时钟，保证最长空闲或远期 alarm 时仍连续。
3. `Set` 需要处理写 CMP 时目标已经过期的竞争；同步边沿 EIRF 软件置位能力只是硬件条件，还需按 OS 契约设计临界区和重检查，当前未完成。
4. `Cancel` 不能简单停止自由运行计数器；需要取消请求并维护状态，而保留时间基准。`State` 的返回类型与状态语义尚无当前版头文件依据。
5. 软件计数器路线仍需 RTE 12.9.0 的真实配置/生成结果证明。当前没有擅自提供同名 `Os_Cbk_*` 空函数来掩盖链接错误。

## 6. Flash、看门狗、引脚与启动的补充结论

- Data Flash 32 KB、64 B 擦除块；DS-E §3.16 p.63 给出 4 B 编程操作规格。其寿命条件为平均 Ta=85°C，125000 次/20 年保持或 250000 次/3 年保持，不应脱离条件复用。实际 FACI/驱动序列还需要专用 Flash Hardware Interface 手册。
- WDTA0 基址 `0xFFD74000`，75% 中断为 EI9。OPBT0（只读映射 `0xFFCD0030`）的 OPWDRUN/OPWDMDS/OPWDOVF 决定启动和初始计时；溢出周期为 `2^(9+OVF)/WDTATCKI`，不能把 `SetTriggerCondition(1000)` 直接解释成硬件 1000 ms。WDTAnMD 的改变需在首次触发前完成。来源 HW-E pp.1522、1531–1535、2884。
- 100 引脚 DPS 的 CAN 候选复用：CAN0 RX=`P3_7/P2_0/P4_5`，TX=`P3_8/P2_1/P4_6`；CAN1 RX=`P3_12/P2_2/P4_2`，TX=`P3_13/P2_3/P4_3`；CAN2 RX=`P5_6`，TX=`P5_5`。这是 DS-E p.23 的芯片能力，不代表板卡已连接这些引脚；尤其不能从旧 P1x 的 CAN2 TX=`P5_7` 直接迁移。
- 板上 STB/EN/ERR/WAKE、UART/LED、Security ID、调试访问设置、任务栈用量，以及当前 `.bss.PORST.*`/`.bss.TRAPRST.*` 的工程含义仍未确认。

## 7. 本次实现与验证范围

参考代码入口：[examples/rh850_mcal_reference/README.md](../examples/rh850_mcal_reference/README.md)。

已实现 OSTM0/1 底层、CAN FD 位时间校验/寄存器编码、1 ms tick 累计辅助逻辑。`python tools/run_host_tests.py` 已通过 5 组测试，其中 100000 次 tick 采样跨越多次硬件及 OS 回绕；测试也检查了访问宽度、忙状态不改配置、停止超时和非法时序。日志保存在 `artifacts/host-build/results.txt`。

验证仅为主机 C 编译与 MMIO 行为模型/算术测试。尚未生成 RH850 ELF，没有验证真实总线时序、GHS ABI、RTA 回调或板卡通信，也没有七模块完整驱动。七模块的设计解说已在 [MCAL 参考案例](mcal-reference-guide.md) 全部交付。
