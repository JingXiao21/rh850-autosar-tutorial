# 硬件交付复审记录

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**（RH850 + AUTOSAR 中文教程），不是公司真实工程；文中“内部 agent / 内部工程 / 交付”等说法只是当时的写作语境，不代表任何真实公司项目、真实配置或指令。正文以下保持原样，仅作溯源；学习请以新教程章节为准。
>
> - **已复审并迁移到**：本文的 15 项修正（pclk≠fCAN、EI190 vs EI184、GAFLM 1=比较、RFE 单独写、TMC/TMSTS 8 位访问、Classical/FD 窗口重叠等）已作为“常见错误”并入 [04-can-mcal/15](04-can-mcal/15-can-driver-debugging.md)，以及 [04-can-mcal/02](04-can-mcal/02-rh850-can-peripheral.md)、[03](04-can-mcal/03-can-clock-bit-timing.md)、[05](04-can-mcal/05-can-interrupt.md) 各章。
> - **已知更正**：内容抽查与手册一致；“内部 agent 无法自行解析手册”等表述是旧语境，本仓库是教育项目。详见 [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) §3 与 [05-consistency-review-log.md](reference/research/05-consistency-review-log.md)。
> - **行号说明**：本 banner 在第 2 行之后插入了 6 行。其他文档（如研究笔记 01、`analysis-and-learning-plan.md`）引用的本文件行号均指插入前版本，对应当前行号 **+6**。

日期：2026-10-01。范围：审核前次解说中影响 RH850/P1M-E CAN、诊断通信、AUTOSAR OS 启动的硬件信息，并补齐内部 agent 无法自行解析手册的部分。

结论：前次资料可以作为需求解说，但不足以独立指导 CAN 硬件配置。现以 [硬件交接主文档](rh850-hardware-handoff.md) 为该范围的优先入口；旧文档不是已验证驱动或已运行系统的证据。

| 项目 | 审核结论 / 本次处理 | 依据 |
| --- | --- | --- |
| 器件容量 | 保持原核查：R7F701381 为 1 MiB Code Flash；不得用系列最大 2 MiB 代替型号容量 | DS-E pp.2–3；HW-E p.257 |
| CAN 时钟 | 保持原核查：pclk=80 MHz 不等于 fCAN；DCS=0/1 对应 40/16 MHz | HW-E p.791 |
| 位时间候选 | 原 NCFG/DCFG 候选已限定 FD 接口，并非算错；本次补 Classical CFG=0x023E0003，禁止混用 TSEG1=31 | HW-E pp.803–804、921–936 |
| 地址布局 | 以前只给基址/时序不够；补 GRMCFG、两套过滤/Rx/Tx 窗口及重叠风险 | HW-E pp.797–802、914–920 |
| 接收中断 | 原中断表区分了两种 FIFO；本次明确参考 Rx FIFO0 必须绑定 EI190，EI184 是 CAN0 common FIFO | HW-E pp.285–286、792 |
| ID 过滤 | 保持 mask 1 比较、0 忽略；补 IDE/RTR 和标准帧精确 mask=0xC00007FF、分页和路由 | HW-E pp.963–971、1096 |
| 引脚复用 | 补具体 ALT：CAN2 RX P5_6 ALT1、TX P5_5 ALT6；100 pin 不用 P5_7 | DS-E p.23；HW-E pp.151–154 |
| PIPC | 补 CAN 必须为 0 的器件特定限制，不能套其他外设的直接 I/O 模板 | HW-E p.131 |
| PORT 宽度 | 逐项核查：PINV/PODC/PODCE/PDSC/PUCC 为32位；PM/PMC/PFC/PFCE/PFCAE/PIPC 为16位；按有效位数量推断访问宽度会出错 | HW-E pp.101–123 |
| FIFO 使能时机 | 补配置 RFE=0、global operating 后单独 RFE=1 | HW-E p.980 |
| Tx 完成 | 补 8 位 TMC/TMSTS、TMTRF 的取消/成功四种状态；发请求不是成功确认 | HW-E pp.1018–1028 |
| FD payload | 收紧原“3 buffer 合并”说法：仅 local0+1+2 或 local3+4+5，不能任意选择；16 B 本例不合并 | HW-E pp.1107–1108 |
| W0C/中断 | 补 clear-zero 语义、禁止普通读改写模板；CAN 电平源须清外设，EIRF 不是软件清源 | HW-E pp.267–268、981–982 |
| OSTM | 保持地址/宽度/回绕核查；强调 TOE=0 是软件控制而非高阻，Cancel 不能重启自由运行基准 | HW-E pp.1542–1567 |
| RAM/ECC | 保留硬件初始化事实，同时明确复位模式可禁用初始化，不将所有 warm reset 当 POR | HW-E p.2890 |
| TDC | 既有位时间校验器只检查 prescaler 限制；不能据此宣布 TDC 已配置，补 offset 单位及板级条件 | HW-E pp.938–940、1099 |
| OS ABI | 未取得真实内部 RTA 头文件，仍不生成猜测回调/ISR 符号；交付硬件 EI 编号、地址、时基条件 | HW-E §6、§22；内部工程需绑定 |

保留的资料差异：SJW≤TSEG2 与 Figure 17.17 严格不等式不一致；FD 接口只用 Classical 帧时 p.1122 的 DCFG/NCFG“同值”描述不能直接解释为原始寄存器复制；p.972 RMNB 表的保留位文字与其 RMPLS 字段行有重叠，本参考 NRXMB=0、不分配 Rx buffer，不依赖该冲突。本文选择明确、保守的使用范围并公开这些差异，没有把推断伪装为厂商结论。

公开资料检索使用 Renesas 官方文档、产品页及技术支持资料作为来源。产品页存在后续技术更新；本次不声称已覆盖所有硅版本/所有勘误，内部 agent 要绑定实际订货号和适用通知。在线项目/其他系列代码只可辅助理解，不能作为本器件地址来源。

验证分为：手册逐页复核；候选数值/地址/访问宽度与输出文件的静态一致性检查；已有 C 参考代码的先前主机测试。后两者不能证明真实芯片、电气链路、供应商生成器或 OS ABI 正确。当前未执行目标板烧写、RH850 编译、CAN 抓包或完整诊断会话。

本次静态检查通过：1185 条按模式展开的地址索引（不是1185个互不重叠物理寄存器）、两套模式内地址无重复、访问对齐、独立列出的地址样例、位时间编码和 FIFO 参数、JSON/CSV 与生成源一致；15 个 Markdown 文件的150个本地文件链接有效；主硬件 PDF 哈希一致；19 项关键访问宽度与手册提取文本匹配。结果见 [validation.txt](../artifacts/hardware-review/validation.txt)。这些自动检查用于发现抄写/算术/文件一致性错误，不替代手册语义审核。
