# Part X — 启动失败与早期 Trap 调试

> 适用器件: R7F701381 = RH850/P1M-E（core RH850G3M，lock-step）；工具链 GHS；AUTOSAR 栈按 RTA-CAR / RTA-OS 风格描述；调试器以 TRACE32 为主，兼顾 GHS MULTI
> 硬件依据: HW-E = R01UH0585EJ0120 Rev.1.20（页码为 PDF 页码）。**本仓库没有 RH850G3M Software Manual、TRACE32 / MULTI 手册、RTA-OS 手册**：凡涉及工具命令、异常向量偏移（FENMI/FEINT 以外）、OS hook 细节之处，各章都标注了“需确认”
> 前置阅读: [RH850 启动过程](../01-rh850/04-startup-process.md)、[中断与异常](../01-rh850/06-interrupt-exception.md)、[ECU 启动流程](../02-autosar-classic/03-ecu-startup.md)、[MCU 驱动](../03-mcal/02-mcu-driver.md)、[P1M-E 内存布局与 GHS MemMap 速查](../reference/p1me-memory-layout-ghs-memmap.md)
> 姊妹手册: [调试手册：22 F1 90 没有响应](../debugging-autosar-diagnostics.md)（启动成功之后的通信/诊断层排查）

---

## 1. 这个系列解决什么问题

这个系列针对一句很常见的话：

> “flash 之后启动时 ECU 直接进入 trap，我都没办法 debug。”

“没办法 debug”通常不是因为问题本身多难，而是因为：

1. **现场一碰就没**：FEPC/FEIC 只在 trap 当时有效，调试器复位会冲掉它们并改写 RESF；
2. **调试器自己会改变行为**：连上之前代码已经在跑（HW-E p.2856），连上之后复位可能被屏蔽（p.434）；
3. **镜像里一次塞了太多东西**：启动汇编、CRT、MCAL、OS、Can、NvM、Dcm 任何一处出错，看起来都是“进 trap 了”；
4. **trap handler 什么也不记录**：默认 handler 只是一个死循环。

系列的目标是让你做到三件事：**先分类**（是 trap、复位循环、卡死还是连不上），**让失败留下证据**（阶段码、复位原因、trap 现场），**按固定顺序排查**（决策树 + 症状表）。

## 2. 阅读顺序

```mermaid
flowchart LR
    C01["01 启动失败总览<br/>分类 + 证据原则"] --> C02["02 异常与 trap handler"]
    C02 --> C03["03 复位原因与复位循环"]
    C03 --> C04["04 调试器连接与恢复"]
    C04 --> C05["05 启动代码失败点"]
    C05 --> C06["06 增量式上板策略<br/>BootStatus 记录"]
    C06 --> C07["07 排查手册<br/>决策树 + 症状表 + 案例"]
```

按 01 → 07 顺序阅读。**如果你现在手上就有一块“上电即 trap”的板子**：先看本页 §5 的“出事后 5 分钟检查清单”，再直接跳到 [07](07-boot-trap-troubleshooting-playbook.md) 的决策树；问题解决后再回头补 01–06。

## 3. 各章摘要

| 章 | 文件 | 一段话摘要 |
|---|---|---|
| 01 | [01-boot-failure-overview.md](01-boot-failure-overview.md) | 为什么启动阶段的失败特别难调；把“上电后不对劲”分成几类可区分的失败（停在异常、复位循环、卡在循环、调试器连不上等），说明每类在外部和片上分别会留下什么痕迹；介绍 P1M-E 上天然存在的证据（RESF、ECM 错误源、SEG、BRAMDAT），以及 ECM 默认反应如何决定“故障长什么样”；提出贯穿全系列的原则——让失败留下证据。 |
| 02 | [02-exception-and-trap-handlers.md](02-exception-and-trap-handlers.md) | RH850G3M 的 EI/FE 两级异常、各寄存器（FEPC/FEPSW/FEIC、EIPC/EIPSW/EIIC、MEA/MEI）与 SYSERR/SEG 的关系；如何设计一个不依赖栈、能区分向量、能在坏栈下保住现场的 trap handler；默认 handler 为什么会让你“没办法 debug”。 |
| 03 | [03-reset-causes-and-reset-loops.md](03-reset-causes-and-reset-loops.md) | 四类复位（POR、System Reset 1/2、Application Reset 1）的差异；RESF 的累积语义与两级解码（RESF → ECMMESSTR）；看门狗默认表现为 ARESF2 的原因；复位循环如何形成、如何用 BRAMDAT 跨复位留下阶段与原因。 |
| 04 | [04-debugger-attach-and-recovery.md](04-debugger-attach-and-recovery.md) | 调试器如何拿到控制权（正常连接、under-reset、热连接等概念）；什么会让调试器连不上或保持不住连接（JP0 被重配、OPJTAG、WDTA 复位循环、Reset Mask）；“救砖”路径（含 FLMD0 串行编程模式）；safe image 与上板前安全检查。 |
| 05 | [05-startup-code-failure-points.md](05-startup-code-failure-points.md) | 沿启动汇编与 CRT 逐条列出失败点：GPR 初始化与 lock-step、SP/GP/TP/EP、EBASE/INTBP 对齐、FPU（CU0）、`.data`/`.sdata` 复制表、`.bss` 清零与栈重叠、noinit 与 STAC、RAM 函数同步与 48 B 预取、系统寄存器 hazard。 |
| 06 | [06-incremental-bring-up-strategy.md](06-incremental-bring-up-strategy.md) | 9 个上板阶段（Stage 0 死循环 → 1 CRT → 2 trap 记录 → 3 GPIO → 4 OSTM → 5 Mcu/Port → 6 OS → 7 Can 回环 → 8 完整 BSW/DCM），每阶段的进入条件、观察点、通过标准与典型失败；`BootStatus` 观察结构（BRAMDAT + noinit 双通道）、`BOOT_STAGE` 宏、带超时的等待循环、DET/OS hook 路由、bring-up 构建开关。 |
| 07 | [07-boot-trap-troubleshooting-playbook.md](07-boot-trap-troubleshooting-playbook.md) | 现场手册：三条铁律、证据读取顺序、从“能否连接”到“阶段码”的决策树、38 行症状→原因→检查表（A–H 组），以及 3 个逐步叙述的教学案例（跳到地址 0 的伪复位、脱机才出现的看门狗复位、Port_Init 重配 JP0 导致调试器掉线）。 |

## 4. 上电前检查清单（Pre-flash checklist，一页）

> 每一项都对应系列中的一个具体章节。打印出来，每次**新板 / 新工程 / 新链接脚本 / 新工具链**第一次上板前逐项勾选。更完整的 safe image 检查见 [04 §8](04-debugger-attach-and-recovery.md)。

**A. 板与调试接口**

- [ ] 已读出并记录 **OPBT0**（`FFCD_0030H`）：OPWDRUN / OPWDOVF / OPWDMDS / OPWDVAC，并算出 WDTA 首次触发期限 `2^(9+OVF)/fWDT`（HW-E p.2884）
- [ ] 已读出并记录 **OPBT2**（`FFCD_0038H`）：OPJTAG 与所用调试接口一致（00 GPIO / 01 LPD 4-pin / 11 Nexus，10 禁止，p.2886）
- [ ] FLMD0/FLMD1 引脚电平符合预期的启动模式（Normal vs Serial programming，p.261）
- [ ] 确认“救砖”路径可用：under-reset 连接与串行编程模式擦除至少在这块板上演练过一次（[04](04-debugger-attach-and-recovery.md)）

**B. 镜像与链接**

- [ ] 复位入口在 `0000_0000`（或项目约定的入口），镜像中确实存在复位向量指令（p.258）
- [ ] Code Flash 范围按 R7F701381 的 1 MB（`0000_0000`–`000F_FFFF`）配置，不照搬其他型号（[MemMap 速查 §1](../reference/p1me-memory-layout-ghs-memmap.md)）
- [ ] `.stack` 与 `.bss`/`.data` 不重叠；SP 初值符号与链接脚本一致（[07 案例 A](07-boot-trap-troubleshooting-playbook.md)）
- [ ] 所有带初值的段（`.data`、`.sdata`、`.tdata`、RAM 函数）都有 ROM 镜像并在复制表中；`.bss` 类段在清零表中；noinit 段不在清零表中
- [ ] 异常向量表与 INTBP 表 512 B 对齐（p.205–206）
- [ ] RAM 函数段末尾预留并初始化 48 B（p.256）
- [ ] 若编译为硬件浮点：启动代码置 PSW.CU0 并初始化 FPSR（p.197）

**C. 启动代码**

- [ ] 在任何对外 store 之前初始化全部 GPR（lock-step，p.250）
- [ ] 尽早保存 RESF、ECMMESSTR0–2、OPBT0（[06 §5.4](06-incremental-bring-up-strategy.md)）；明确“谁唯一负责清除 RESF”
- [ ] trap handler 记录向量编号、FEPC/FEIC、EIPC/EIIC、MEA/MEI、SEGFLAG，先不用栈；bring-up 构建中停在已知标签（[06 §6.3](06-incremental-bring-up-strategy.md)）
- [ ] `BOOT_STAGE` 阶段码已插入每个 init 调用之间，BRAMDAT0 有魔术高位

**D. 配置**

- [ ] **Port 配置中没有任何 JP0 引脚**（JP0_0–JP0_5 为调试接口，p.72、p.87）
- [ ] Mcu 配置/EcuM callout 中**没有**“等待 PLL LOCKED”的无超时循环（P1M-E 无软件 PLL；`SWS_Mcu_00206`）
- [ ] 启动期所有等待循环（GRAMINIT、CAN 模式、OSTM 停止、NvM ReadAll）都有超时并记录循环 ID
- [ ] WDTA 策略已决定并记录：bring-up 用 OPWDRUN=0，或已证明“复位释放 → 首次触发”在期限内；WDTAnMD 只写一次（p.1531、p.1535）
- [ ] OSTM 通道归属唯一（OS 与 Gpt 不共用）
- [ ] DET、ErrorHook、ProtectionHook、ShutdownHook 在 bring-up 构建中全部打开并写入记录

**E. 交付记录**

- [ ] 同一次构建的 ELF、HEX、map、构建日志与哈希一起保存
- [ ] 记录 bring-up 构建与量产构建的差异（option bytes、WDTA 策略、DET、trap 行为、调试器 reset mask 设置）
- [ ] 计划中包含至少一轮**不接调试器的冷启动**验证

## 5. 出事后 5 分钟检查清单（Post-failure, first 5 minutes）

> 详细的时间线和分支见 [07 §11](07-boot-trap-troubleshooting-playbook.md) 与 [07 §5 决策树](07-boot-trap-troubleshooting-playbook.md)；调试器侧的连接细节见 [04 §9](04-debugger-attach-and-recovery.md)。

**第 0 分钟：先别动**

- [ ] **不要复位，不要重新下载**。记录 LED/GPIO、总线、电流等外部现象
- [ ] 记录：是“一上电就坏”，还是“运行一段时间后坏”；复位是否有固定周期

**第 1 分钟：连上并停住**

- [ ] 不复位 attach → Break。连不上 → 试 under-reset 连接；仍连不上 → 转 [04](04-debugger-attach-and-recovery.md)
- [ ] 能 under-reset 连接但正常连接不行 → 怀疑 JP0 重配或复位循环（[07 案例 C](07-boot-trap-troubleshooting-playbook.md)）

**第 2 分钟：读证据（只读，不清除）**

- [ ] BRAMDAT0–3（`FFC0_A000H`–`FFC0_A00CH`）：阶段码、FEPC 摘要、启动次数、超时循环 ID
- [ ] `BootStatus`（若有）：trap 记录、首个 DET、OS hook、计数器
- [ ] RESF（`FFF8_1000H`）与 ECMMESSTR0–2（`FFD6_0008H` 起）：原值照抄
- [ ] FEPC / FEPSW / FEIC、EIPC / EIPSW / EIIC、MEA / MEI、SEGFLAG / SEGADDR、PC / SP / LP / PSW

**第 3 分钟：初步分类**

- [ ] PC 在异常向量 / `_trap_halt` → 看 vectorId + FEIC/EIIC + MEA + SEGFLAG（[07 §6 A 组](07-boot-trap-troubleshooting-playbook.md)）
- [ ] PC 反复回到 0 → **清 RESF 后再观察**：有新标志 = 真复位（看 ECMMESSTR，源 0 = WDTA）；无新标志 = 软件跳到 0（栈/LP 被破坏）
- [ ] PC 在某个循环 → 看 `timeoutLoopId` 和被等待的寄存器
- [ ] 已到 main → 看 `BootStatus.stage`，按阶段码进入 [07 §6](07-boot-trap-troubleshooting-playbook.md) 对应组

**第 4 分钟：映射到代码**

- [ ] 用 map 把 FEPC、LP 映射到函数与源码行
- [ ] 在症状表中找到对应行，写下“下一个具体动作”和一个可验证的假设

**第 5 分钟：才开始重现**

- [ ] 现在才可以清 RESF、复位、设断点、重跑
- [ ] 一次只验证一个假设；修复后从“最后通过的阶段”重新走（[06 §6](06-incremental-bring-up-strategy.md)）

## 6. 与其他章节的边界

| 内容 | 本系列 | 其他位置 |
|---|---|---|
| 启动顺序本身（复位 → main → EcuM → OS → BswM → RTE） | 只引用 | [RH850 启动过程](../01-rh850/04-startup-process.md)、[ECU 启动流程](../02-autosar-classic/03-ecu-startup.md) |
| 链接脚本与 MemMap 写法 | 只列检查项 | [链接脚本](../01-rh850/05-linker-script.md)、[MemMap 速查](../reference/p1me-memory-layout-ghs-memmap.md) |
| 中断控制器、向量方式、EIC | 只在排查中引用 | [中断与异常](../01-rh850/06-interrupt-exception.md) |
| 启动后 CAN/诊断不通 | 不涉及 | [调试手册：22 F1 90 没有响应](../debugging-autosar-diagnostics.md)、[集成调试](../08-integration/07-integration-debugging.md) |
| 主机可运行的启动模型 | 引用 | [examples/can_irq_demo/startup/](../../examples/can_irq_demo/startup/README.md) |

## 7. 对未来真实项目的意义

进入真实 RTA-CAR + RH850 项目后，这个系列应当转化为三份项目资产：

1. **一页上电前检查清单**（本页 §4，补上项目真实的文件名、段名、option byte 值）；
2. **一份 BootStatus 规范**（阶段码表、等待循环 ID 表、trap vectorId 表、字段版本号），代码放在项目自己的集成目录，由编译开关控制；
3. **一份排查索引**（[07](07-boot-trap-troubleshooting-playbook.md) 的症状表逐行映射到项目中的启动汇编、链接脚本、EcuM callout、Port 配置、RTA-OS 配置位置），以及配套的 TRACE32 只读观察脚本。

有了这三样，“flash 之后直接进 trap”就不再是“没办法 debug”，而是一次按清单进行的、可以在几分钟内收敛到具体阶段和机制的排查。
