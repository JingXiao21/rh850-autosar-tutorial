> **[当前状态说明 — 2026-10-02]** 本文件是上一阶段（“内部 agent 技术解说交付”）的计划，保留作历史记录。**当前计划是 [claude_plan.md](claude_plan.md)**（RH850 + AUTOSAR Classic 中文教学项目），章节完成状态在 [教程总目录（Master Index）](docs/rh850-autosar-tutorial-content.md) 中跟踪。

# RH850/P1M-E 内部 agent 技术解说交付计划

更新：2026-10-01。**本计划按用户澄清后的范围完成：提取需求、查证资料、回答技术问题，并解释 RH850 MCAL 参考实现。** 本次不以真实 RTA 工程修改、完整驱动开发、目标编译、烧录或上板测试作为完成条件。

原始问题保留在 [requirements-extracted.md](requirements-extracted.md)。范围调整前的实施计划保留在 [plan-implementation-history.md](plan-implementation-history.md)，供未来实际开发参考，不作为当前未完成任务。

## 1. 交付目标

让内部 agent 能理解 R7F701381 / RH850/P1M-E 与 RTA-CAR 12.9.0、GHS、Renesas MCAL 的关联，知道哪些硬件事实已有依据，哪些截图假设有误，计数器和七个 MCAL 模块应如何设计，以及哪些参数必须从真实工程取得。

对缺少资料的问题，回答必须包含：已知边界、不能确定的具体值、不能据此推出的结论，以及后续可执行的核对方法。未知值不编造；工程资源缺失不阻塞本次解说交付。

## 2. 已完成的工作

| 工作包 | 覆盖范围 | 交付文件 | 状态 |
| --- | --- | --- | --- |
| P0 需求整理 | 四张截图，A～J 共 40 项，K1，用户新增 R1～R7 | [需求提取](requirements-extracted.md) | 完成 |
| P1 资料查证 | 三份用户 PDF；补充适用的 P1M-E 硬件手册；来源/版本/哈希 | [资料索引](docs/source-index.md)、[硬件报告](docs/hardware-findings.md) | 完成 |
| P2 逐项回答 | A1～J4，每项结论、依据、未知边界与核对方法 | [agent 技术解答](docs/agent-guide.md)、[覆盖追踪](docs/requirements-status.md) | 完成 |
| P3 计数器方案 | K1；硬件/软件路线、四回调职责、回绕/晚匹配/取消/并发 | [计数器设计解说](docs/counter-design.md) | 完成 |
| P4 MCAL 参考案例 | R1 Mcu、R2 Port、R3 Dio、R4 Gpt、R5 Can、R6 Fls、R7 Wdg | [七模块案例](docs/mcal-reference-guide.md) | 完成 |
| P5 网上资料评估 | 官方 MCAL/示例、ETAS/GHS 文档、公开 RH850 项目及适用性 | [参考资源](docs/online-references.md) | 完成 |
| P6 交付核对 | 编号覆盖、内部链接、事实/推导/未知区分、实现范围一致性 | README、上述追踪与解说文档 | 完成 |
| P7 用户追加硬件复审 | Classic/FD 两套地址、位域/宽度、Port ALT、AFL/FIFO/Tx、IRQ/OS、OSTM、复位、读回和验收 | [硬件交接](docs/rh850-hardware-handoff.md)、[复审记录](docs/hardware-review.md)、[内部 agent 指令](docs/internal-agent-task.md)、[JSON](docs/hardware-registers.json) / [CSV](docs/hardware-registers.csv) | 资料与指导完成，目标板验证未执行 |

## 3. agent 阅读顺序

当前用户关注 CAN/diagnose/OS 硬件配置时，优先从 [内部 agent 执行指令](docs/internal-agent-task.md) 与 [详细硬件交接](docs/rh850-hardware-handoff.md) 开始；其中内容优先于早期简略解说。

1. 阅读 [agent-guide.md](docs/agent-guide.md) 的结论，再按 A～J 查对应问题。
2. 处理 Rte_TickCounter 时阅读 [counter-design.md](docs/counter-design.md)，先确认单位、时间模型和接口前提。
3. 理解或开发 MCAL 时阅读 [mcal-reference-guide.md](docs/mcal-reference-guide.md)，按模块查配置、状态、算法和示例。
4. 需要核对寄存器和页码时使用 [hardware-findings.md](docs/hardware-findings.md) 与 [source-index.md](docs/source-index.md)。
5. 阅读现有 C 组件时从 [参考代码 README](examples/rh850_mcal_reference/README.md) 开始，注意它与完整设计的差距。

## 4. 核心问题的收口结论

| 问题 | 解说交付结论 |
| --- | --- |
| 器件资料适用性 | R7F701381 使用 P1M-E 依据；P1x-C、旧 P1M 和 U2A6 资料只作对照 |
| 内存 | Code Flash 1 MB；原 2 MB 假设需纠正；LRAM/GRAM 和 stack 边界已解释 |
| 时钟/CAN | 80 MHz CAN 接口时钟不能直接当位时间源；给出 40/16 MHz 下候选与编码边界 |
| 中断 | 硬件 EI/EIC 表已明确；ETAS 名字、IPL 映射保留版本限定，给出核对链 |
| K1 路线 | 建议 OSTM1 自由运行硬件路线为主线；软件路线以正式生成支持为条件备选 |
| OS 四回调 | 给出角色、状态模型、伪代码、晚匹配/取消/长期空闲等边界；不伪造当前版 ABI |
| 启动/编译/镜像 | 给出 RAM/ECC、基址、栈、异常入口、GHS 参数及镜像转换审核方法 |
| 板级值 | 引脚/收发器/日志/烧录器/ID 等未知值逐项解释，并给出所需证据和读取位置 |
| MCAL 七模块 | 全部具备实现解说；现有可执行基础组件的范围与缺口明确 |

## 5. 完成条件与证据边界

- [x] 原始 40 项问题无遗漏，K1 与新增 R1～R7 有明确回答入口。
- [x] 硬件结论有适用来源；计算说明条件；无法确定的工程/板级值明确标为未知。
- [x] K1 两条路线都有前提、实现逻辑、失败边界和验证思路。
- [x] 七模块案例包含配置、职责、步骤、错误处理、示例和接口边界。
- [x] 网上项目/资料给出链接与适用性，不声称能直接替换目标工程。
- [x] 现有代码与主机测试如实记录，未将伪代码或文档建议当作已实现驱动。
- [x] 计划、首页、需求追踪和解说入口按本次交付范围统一。

已有代码通过 5 组主机测试，其中包括 100000 次 tick 采样。此结果来自之前已执行的测试，本次文档整理未修改 C 代码，不把它扩展为目标硬件验证结论。

**本次解说交付完成。** 未提供的真实工程、GHS/ETAS 环境和板卡信息是未来实施输入，已在对应回答中解释，不再作为本次计划的阻塞项。
