# 直接交给内部项目 agent 的指令

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**（RH850 + AUTOSAR 中文教程），不是公司真实工程；文中“内部 agent / 内部工程 / 交付”等说法只是当时的写作语境，不代表任何真实公司项目、真实配置或指令。正文以下保持原样，仅作溯源；学习请以新教程章节为准。
>
> - **特别说明**：本文件写成“直接交给公司内部项目 agent 的指令”，**这只是上一阶段的写作框架**。本仓库没有、也不代表任何公司内部工程，读者不应把下列步骤当作对真实项目的指令；其中可复用的部分已改写为“将来进入真实项目时的自查清单”：[09-real-project-preparation/02](09-real-project-preparation/02-how-to-read-mcal.md)、[09-real-project-preparation/07](09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md)、[08-integration/01](08-integration/01-ecu-configuration-checklist.md)。
> - **已知更正**：语境违反当前项目的 Information Boundary（见 [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) §1.1、§3）；文中的硬件约束（EI190 vs EI184、GAFLM 1=比较、fCAN 40/16 MHz 等）与手册一致，已并入上述章节。变更记录见 [05-consistency-review-log.md](reference/research/05-consistency-review-log.md)。
> - **行号说明**：本 banner 在第 2 行之后插入了 6 行。其他文档（如研究笔记 01、`analysis-and-learning-plan.md`）引用的本文件行号均指插入前版本，对应当前行号 **+6**。

你负责在内部 AUTOSAR Classic 工程中完成 RH850/P1M-E CAN、诊断通信及 OS 所需的硬件配置。本包提供手册核实结果，你无需再解析 RH850 PDF。先阅读 [rh850-hardware-handoff.md](rh850-hardware-handoff.md)，查阅 [hardware-registers.json](hardware-registers.json) / [CSV](hardware-registers.csv)，以 [hardware-review.md](hardware-review.md) 理解审核后的约束。

目标基线是截图中的 R7F701381、100 pin DPS。若项目实际器件不同，先停止套用具体地址/引脚参数并记录差异；不要从其他 RH850 系列样例填空。目标是做出可审核配置并按环境条件验证，不允许仅用空 ISR、空 counter callback 消除编译错误。

请依次执行：

1. 从工程、生成器和原理图读取完整器件号、时钟、MCAL 版本、RTA-OS 端口版本、CAN channel/pins、收发器、诊断 ID 与帧格式、OSTM owner。将结果写成 `HardwareBinding.md`，每项注明文件/符号依据；未知项明确保留，不能猜值。
2. 找到现有 Mcu/Port/Dio/Can/CanTrcv/Os/Gpt 配置与生成输出、链接脚本和中断表。优先通过供应商支持的配置源改动，不在应用层叠加一套并发的裸寄存器初始化。
3. 明确选择手册交接的路线 A（Classical 500k）或路线 B（FD nominal500k/data1M、16 B）。若网络需求不同则用手册字段重新计算，不硬套候选常数。RCMC、CFG/NCFG/DCFG、AFL/Rx/Tx 窗口必须作为同一套方案核对。
4. 核对 Port 的实际 ALT、PM、PMC、PIPC=0 和电气配置。尤其 CAN2 RX=ALT1、TX=ALT6。收发器 EN/STB 按其资料设置，不能从 MCU 手册推导。
5. 核对 GAFL 精确过滤、分页、ID 格式和 FIFO 路由；本包 7 个 ID 仅为截图示例，真实物理诊断请求/功能请求/响应必须以网络配置为准。生成 buffer/HOH/PDU 分配表，证明容量和所有权无冲突。
6. 核对 CAN 初始化模式转换与超时、RAM 初始化、RFE 启用时机、Tx 完成确认、W0C、bus-off 策略。读取地址时使用索引中的访问宽度，保留位不能当配置位。
7. 对照生成的中断向量绑定 EI190 Rx FIFO、各通道 TRX/ERR 和需要的 global error；使用真实 OS ISR 包装与优先级映射。EI184 仅适用于 CAN0 common Rx FIFO。不得仅凭 `Interrupt_0228` 等名字反推硬件编号。
8. 核查 OSTM0/1 唯一 owner、真实计数频率、1 ms 换算、硬件回绕、Set/Cancel 竞争与 OS callback ABI。不得让 Gpt 和 OS 同时初始化相同 OSTM。
9. 输出 `HardwareExpectedReadback.csv`：阶段、寄存器、模式、地址、访问宽度、有效位 mask、期望值、超时/失败动作、实际读回。只列本配置实际分配资源；不要遍历整个寄存器目录读写保留/空窗口。JSON/CSV 地址索引不是执行顺序。
10. 有构建环境就运行项目规定的生成/编译检查；有目标板就按交接文档第13节逐层验证时间基准、轮询CAN、IRQ、OS并发、诊断多帧与错误恢复。保存 map、启动日志、寄存器快照、总线记录。缺少环境时输出已完成的静态配置与未验证项，禁止标“上板成功”。

最终给项目维护者：绑定表、配置差异及生成结果、内存/中断/buffer 分配表、预期与实际读回、验证结果、仍需项目资料确认的项。本文授权范围是硬件配置指导；UDS 服务、DID、安全算法和业务需求沿用内部项目要求。

不得绕过的硬件约束：fCAN 不用80MHz；Classic/FD 窗口不混用；CAN PIPC不置1；TMC/TMSTS用8位；PORT宽度逐项区分；不能清EIC代替清CAN电平源；未完成传播延时/TDC确认不宣布FD链路已通过；不得禁用全部ECC/ECM/Guard来掩盖错误。
