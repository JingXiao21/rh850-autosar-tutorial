# RH850/P1M-E 工作需求提取

整理日期：2026-10-01。来源为四张截图，已合并重叠内容、整理断行，保留原需求编号与技术参数。这是截图内容的结构化转录，不代表已完成硬件或工程核实；截图中的疑问、假设和现有配置均保持待核查状态。

## 1. 来源对应

| 来源 | 主要内容 |
| --- | --- |
| [20261001_122744.jpg](20261001_122744.jpg) | 背景、交付要求、A、B 前半部分 |
| [20261001_122804.jpg](20261001_122804.jpg) | B 后半部分、C、D、E |
| [20261001_122812.jpg](20261001_122812.jpg) | E 后半部分、F、G、H、I、J |
| [20261001_122817.jpg](20261001_122817.jpg) | G 至 J 重叠内容、优先级、计数器决策问题 |

## 2. 任务背景与目标

任务：为 RH850/P1M-E（R7F701381）的 RTA-CAR 12.9.0 + GHS + Renesas MCAL 工程查证硬件信息。

| 项目 | 截图记载 |
| --- | --- |
| 当前工程 | `C:\VMEPS\RTA-SK_VRTA_GCC_SingleCore_12.9.0_R3` |
| 工具链与目标 | RTA-CAR 12.9.0、GHS `ccrh850`、目标 `RH850GHS[P1M]` |
| MCAL | Renesas P1M MCAL，Can/Dio/Fls/Gpt/Mcu/Port/Wdg，AUTOSAR 4.2.2 API |
| MCAL 配置 | `MCAL/config/ecucValues/App_*_P1M-E_701381_Sample.arxml` |
| 已在真实 ECU 测试过的参考工程 | `C:\VMEPS\int.bbm.vm-eps.plrp`，关注 `Communication`、`ComplexDrivers`、`standalone/Dio`；其引脚表属于 RH850/U2A6，不是 P1M |
| 旧参考工程 | `C:\VMEPS\RTA-SK_RH850P1M_GH_9.1.1_INTERNAL`，RTA-OS 9.1.1，带预编译 `RTAOS.a` |
| 当前阻塞 | RTE 要求 `Rte_TickCounter` 为 HARDWARE 计数器；RTA-OS 要求提供 `Os_Cbk_Now/Set/State/Cancel_Rte_TickCounter` 四个回调，当前已到链接的最后一步 |
| 最终目标 | 生成 ELF 和可烧录镜像，上板验证 CAN 栈与诊断栈 |
| RTA-OS 端口资料 | `C:\ETAS\RTA-CAR_12.9.0\RTA-OS_12.9.0\Targets\RH850GHS_5.0.39`，查 `doc` 和 `Os_ConfigInterrupts.h` 的生成规则 |

四个回调的具体符号、函数签名和语义，后续应以当前版本生成的头文件及端口文档为准。

## 3. 原始交付要求

1. 每项给出结论值、来源（手册名称/章节/页码，或工程文件路径及行号）、置信度（已确认/推测）。
2. 查到的内容用表格列出；查不到的单独列入“未确认”。
3. 如果 `.plrp` 工程已有 P1M 或 P1x 的真实配置，优先引用。
4. 手册与工程矛盾时，两者都列出，明确矛盾点。

## 4. 完整需求清单

### A. 芯片与时钟（原文标注最优先）

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| A1 | 确认 R7F701381 的完整型号、封装、CPU 内核（原文疑问：G3M？）、有无 FPU；确认当前 GHS 参数 `-cpu=rh850g3m -fsoft` 是否正确。 |
| A2 | 确认 code flash、data flash 的大小及基地址，本地 RAM/全局 RAM 的大小及地址。`rh850ghs.ld` 当前假设：`iROM 2048K@0x0`、`PLRAM 128K-10K@0xFEDE0000`、栈 `10K@0xFEDFD800`，核查是否适用于 R7F701381。 |
| A3 | 确认外部晶振/MainOSC、PLL0/PLL1、CPUCLK、PCLK、OSTM 计数时钟、RS-CAN 的 fCAN（`clk_xincan` / `CLK_HSB` / 其他）、WDTA 时钟（LPO 或 `CLK_LSB`）。与 `MCAL/config/ecucValues/App_MCU_P1M-E_701381_Sample.arxml` 对照，指出差异。 |
| A4 | 确认 P1M 实际 fCAN；`.plrp` 暗示约 80 MHz。当前标称速率 500k：`TSeg1=31, TSeg2=8, SJW=4`、采样点 80%；FD 数据速率 1000k：`TSeg1=15, TSeg2=4, SJW=4`。用确认后的 fCAN 复算并给出 BRP/TSeg。 |

### B. 中断（配置 RTA-OS ISR 与 OSTM）

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| B1 | P1M 使用直接向量还是 EIINTTBL 表参考机制；向量偏移与 EI 通道号如何对应。 |
| B2 | 列出外设中断通道号、向量偏移、EIC 寄存器地址、默认优先级位宽。覆盖 OSTM0/OSTM1 及其他 OSTM（如有）、RS-CAN 全局 RX FIFO/全局错误/CAN 通道 0 的 TX/RX FIFO/错误、FCU、WDTA0/1、TAUJ0 各通道。 |
| B3 | 核对参考工程复制来的向量：`OSTM0 → Interrupt_0228`（RTA-OS 内部通道 74）、`OSTM1 → Interrupt_022C`（内部通道 75）、WDG 触发 ISR、CAN ISR（`Os_EcucValues.arxml` 当前有 3 个参考 ISR）。给出 P1M 正确值，解释 RH850GHS 5.0.39 的 `Interrupt_XXXX` 命名规则；查端口 `doc` 和 `Os_ConfigInterrupts.h` 生成规则。 |
| B4 | 确认 EIC 的 EIP 取值范围（原文疑问：0～15？）及与 `OsIsrPriority`（`IPL_x`）的映射。 |

### C. OSTM（决定 Rte_TickCounter 四个回调的实现）

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| C1 | OSTM 基地址，以及 `OSTMnCMP/CNT/TO/TOE/TE/TS/TT/CTL` 的偏移和位定义。 |
| C2 | 间隔定时器模式与自由运行比较模式：哪种能实现 free-running 计数 + 匹配中断；匹配值已过时能否通过软件置位中断挂起。截图指出 RTA-OS 要求 `Os_Cbk_Set` 处理“匹配值已过”的情况。 |
| C3 | 读取当前计数值、设置/取消匹配、清除/置位中断请求的方法，涉及 `EIC.EIRFn`、`EIMKn` 屏蔽位。 |
| C4 | 硬件计数器 32 位；`Rte_TickCounter.OsCounterMaxAllowedValue=65535`，tick 周期 1 ms。明确如何把 OSTM 硬件计数折算为 1 ms OS tick，是否需要分频、回调除法或软件计数。 |
| C5 | Gpt 通道 0 已占用 OSTM0（`Gpt_Cbk_Notification`、CK0、20 MHz）；确认 OSTM1 是否空闲，以及通道是否受特定核/时钟域限制。 |
| C6 | 核查当前 Gpt 配置 `GptChannelConfiguration0/1`、CK0、`2.0E7` 的实际 tick 频率与分频是否一致。 |

### D. RS-CAN 与收发器引脚

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| D1 | 板上实际使用的 CAN 通道（CAN0/CAN1 等），RX/TX 引脚的端口号、位、复用功能 ALT。 |
| D2 | 收发器 STB/EN/ERR/WAKE 的端口和位、有效电平、上电默认状态、开机时序；`.plrp` 使用 STB1/EN1，查 U2A6 引脚并确认 P1M 板实际对应引脚。 |
| D3 | 核查 MCAL Can 配置与 P1M RS-CAN FD 硬件限制：13 个邮箱、7 个 Rx 接收规则，ID 为 `1248, 2015, 20, 253, 1010, 1264, 123`；原文称 `mask=0` 表示精确匹配，此语义也需核实。Tx 缓冲 `CanPayloadStorageSize=16`、`CanFdPaddingValue`、轮询模式；核查 Tx buffer-mode、数据长度及 FD BRP 是否必须与标称阶段相同等限制。 |
| D4 | 是否需要 CAN 唤醒/睡眠，以及 EcuM 唤醒源配置。 |

### E. Flash 与 Fee/NvM

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| E1 | data flash 地址范围、擦除块大小、最小写入单元、擦写寿命。 |
| E2 | 当前为 Fls MCAL（FCU）+ Fee（FS1x）+ NvM。核查 `Fls_Cfg` 扇区布局是否对应 P1M 实际 data flash；是否需要将 Fls 驱动搬到 RAM 执行，以及 FCU 擦写时 code flash 执行限制；检查 MemMap 与链接脚本是否具备对应 section。 |
| E3 | 如板上有外部 EEPROM/Flash，列出相关器件。 |

### F. 看门狗

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| F1 | WDTA 基地址、是否由 option byte 默认开启、启动后首次喂狗期限。 |
| F2 | `Wdg_59_DriverA` 的 Wdg 超时/模式配置是否合理，涉及 `SetTriggerCondition(1000)`。 |
| F3 | OS 中 `WDG_59_DRIVERA_TRIGGERFUNCTION_CAT2_ISR` 的中断来源，原文疑问为 WDTA 的 75% 中断。 |

### G. 启动、复位、内存布局（GHS）

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| G1 | 复位向量和 `reset.850` / `crt0` 在 P1M 上需要的步骤：栈指针、SDA/TDA 基址、gp/ep/tp、ECC 初始化、RAM 清零、时钟初始化先后顺序。 |
| G2 | `-large_sda -sda=0 -registermode=32 -reserve_r2 -no_callt` 是否适合 P1M-E；gp/tp/ep 寄存器值如何设置，是否与 `rh850ghs.ld` 一致。 |
| G3 | FENMI、FEINT、SYSERR、TRAP 的入口；MCAL 是否提供 FENMI 处理函数，工程引用了 `MCU_FENMI_ENTRY` 等宏。 |
| G4 | 是否需要在启动时清零 PLRAM 以避免 ECC 错误；`.bss.PORST.*` / `.bss.TRAPRST.*` 段的用途。 |
| G5 | Option bytes、Security ID、调试访问保护：板子出厂默认值及烧录需要的 ID。 |

### H. 烧录与调试

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| H1 | 板子实际使用的调试器/烧录工具：E1/E2/E2 Lite、GHS Probe/MULTI、Renesas Flash Programmer 等。 |
| H2 | 所需镜像格式 `.mot/.s37/.hex/.elf`；GHS 链接后使用哪个工具转换（`gsrec` / `gmemfile`）。 |
| H3 | 烧录/调试的内存映射选项、写入起始地址、是否需要校验和/CRC。 |
| H4 | 板上验证的串口或其他日志输出方式，UART 引脚及波特率。 |

### I. 其他外设引脚（MCAL Port/Dio）

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| I1 | LED、按键等调试 IO 的 port/bit/极性。 |
| I2 | Port 配置当前启用的引脚和复用功能，PMC/PFC/PFCE/PM 寄存器值是否与原理图一致。 |
| I3 | 未使用引脚的推荐配置，避免悬空。 |

### J. RTA-OS 端口

| 编号 | 待核查内容及截图中的现有值 |
| --- | --- |
| J1 | RH850GHS 5.0.39 支持的 variant 名称（本工程用 P1M）、内核模式、栈模型。 |
| J2 | 硬件计数器回调 `Os_Cbk_*` 的要求与示例，是否附带 OSTM 参考实现。 |
| J3 | `Interrupt_XXXX` 中 XXXX 与向量偏移的关系；Cat1/Cat2 ISR 对 EIC 的处理。 |
| J4 | 任务栈、OS 栈、ISR 栈大小是否合适，当前 `.stack` 填充 `0x2800`。 |

## 5. 原始优先级

| 顺序 | 原文指定事项 |
| --- | --- |
| 最高 | A3、A4、B2、B3、C：直接决定能否链接并让 OS 运行 |
| 其次 | D1、D2：CAN 能否通信；E2：Fee/NvM 能否启动；F |
| 之后 | G、H、I、J |

## 6. 额外决策问题（整理编号 K1）

需要回答：在 **RTA-OS RH850GHS 5.0.39 + RTA-RTE 12.9.0** 下，以下哪条路线可行，有哪些问题？

| 方案 | 原文内容 | 必须确认 |
| --- | --- | --- |
| 方案 1 | 用 OSTM 做硬件计数器，手写 `Os_Cbk_Now/Set/State/Cancel` 四个回调 | 硬件行为与当前端口回调要求是否吻合 |
| 方案 2 | 改成软件计数器，参考旧工程，在 `Gpt_Cbk_Notification` 中调用 `IncrementCounter` | RTE 12.9.0 是否有配置让 `osNeeds` 不再生成 HARDWARE 计数器 |

## 7. 转录与待确认说明

- 四张截图重叠部分已合并；A1～J4 共 40 项，另有 K1 一项决策。
- 型号中的 `P1M` 为数字 1；`R7F701381` 尚未包含经查证的完整订货后缀。
- `80 MHz`、`20 MHz`、内存地址、ISR 名称、`mask=0` 的语义等均为截图中的现有值/假设，不能直接当成正确硬件结论。
- `CanFdPaddingValue` 在截图中未给出具体取值；板卡版本、晶振实物值、收发器型号、实际调试工具、诊断验收用例和项目截止日期也未给出。
- 本文件记录最初的截图转录阶段，当时尚未核查三份 PDF。后续查证和完整回答已写入 [agent-guide.md](docs/agent-guide.md)，原截图中的假设在此保持原样以便对照。
