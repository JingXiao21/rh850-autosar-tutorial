# 内部 agent 解说：需求覆盖追踪

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**（RH850 + AUTOSAR 中文教程），不是公司真实工程；文中“内部 agent / 内部工程 / 交付”等说法只是当时的写作语境，不代表任何真实公司项目、真实配置或指令。正文以下保持原样，仅作溯源；学习请以新教程章节为准。
>
> - **已复审并迁移到**：需求覆盖追踪已由新教程的学习路线与总索引取代：[00-learning-roadmap.md](00-learning-roadmap.md)、[analysis-and-learning-plan.md](analysis-and-learning-plan.md)。
> - **已知更正**：表中“已解答”≠“已验证”——很多条目只是给出了核对方法，并无目标板或真实工程证据；“内部 agent 解说已完成”的说法不适用于本教育项目。详见 [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) §3 与 [05-consistency-review-log.md](reference/research/05-consistency-review-log.md)。
> - **行号说明**：本 banner 在第 2 行之后插入了 6 行。其他文档（如研究笔记 01、`analysis-and-learning-plan.md`）引用的本文件行号均指插入前版本，对应当前行号 **+6**。

更新：2026-10-01。**A～J 共 40 项、K1 和 R1～R7 的解说均已完成。** “已解答”包括有证据的答案，也包括对未知工程值给出原因、适用边界及明确核对方法；不表示真实工程参数已全部确认或已上板。

本表把“解说交付状态”与“事实/实现状态”区分。工程和板卡输入列只用于未来实施，本次无需等待这些输入。完整答案以对应链接为准；下表保留首轮证据摘要，不替代后续补充解说。

| 编号 / 回答入口 | 解说状态 | 已有证据摘要 | 未来实施时需核对的输入 |
| --- | --- | --- | --- |
| [A1](agent-guide.md#A1) | 已解答 | DS-E pp.1–3：G3M、FPU、LFQFP100/DPS | 实物完整后缀；GHS `-fsoft` 与库 ABI |
| [A2](agent-guide.md#A2) | 已解答 | DS-E pp.2–3、HW-E p.257：1 MB Code Flash、32 KB Data Flash、128+64 KB RAM，地址已列出 | 修改真实链接脚本并审查 map/段布局 |
| [A3](agent-guide.md#A3) | 已解答 | HW-E pp.469–471、791、1543：CPU/HSB/LSB/CAN/OSTM 时钟链 | 板卡晶振、启动配置、MCU ARXML 对照 |
| [A4](agent-guide.md#A4) | 已解答 | HW-E pp.921–922、935–936、1092：40 MHz 下候选时序和编码已计算/主机测试 | 确认 DCS、物理值/配置编码关系、SJW 文档差异及实测 |
| [B1](agent-guide.md#B1) | 已解答 | HW-E pp.267–268、282：EITB 两种机制及偏移区别 | 当前 OS 使用哪种向量方式 |
| [B2](agent-guide.md#B2) | 已解答 | HW-E pp.282–290、792、1544：所需 EI/FEINT、偏移、EIC 及优先级表 | 逐项对照生成 ISR；WDTA1 记为目标不适用 |
| [B3](agent-guide.md#B3) | 已解答 | OSTM0/1 EI74/75 确认；硬件偏移与 ETAS 符号必须区分 | 5.0.39 `Interrupt_XXXX` 命名和当前 ISR 配置 |
| [B4](agent-guide.md#B4) | 已解答 | HW-E p.268：EIP 0～15，0 最高 | OsIsrPriority/IPL 映射 |
| [C1](agent-guide.md#C1) | 已解答 | HW-E pp.1543、1551–1559；基础 OSTM 访问代码完成主机测试 | 目标编译和寄存器读写实测 |
| [C2](agent-guide.md#C2) | 已解答 | HW-E pp.267、1562、1567：自由运行比较、软件挂起的硬件条件 | 当前 OS Set 契约、过期匹配处理和竞态验证 |
| [C3](agent-guide.md#C3) | 已解答 | 计数读取/比较值写入已实现；EIC 读改写限制已记录 | State/Cancel/挂起标志实现与并发验证 |
| [C4](agent-guide.md#C4) | 已解答 | 时间换算、回绕限制和累计辅助代码已验证 | 持续采样/溢出维护保证及四个回调集成 |
| [C5](agent-guide.md#C5) | 已解答 | HW-E pp.1542–1544：0/1 使用 EI，3～7 使用 FEINT | OSTM1 在工程中的占用情况 |
| [C6](agent-guide.md#C6) | 已解答 | HW-E pp.1547–1560：PCLK 与 TAUD/TAUJ 计数使能关系 | 现有 CK0/2.0E7 配置及预分频一致性 |
| [D1](agent-guide.md#D1) | 已解答 | DS-E p.23：100 引脚 DPS 的 CAN 候选引脚 | 板卡实际 CAN 通道、布线与 ALT 配置 |
| [D2](agent-guide.md#D2) | 已解答 | 已识别 U2A6 引脚不可直接沿用 | 原理图、收发器型号/控制引脚/有效电平/时序 |
| [D3](agent-guide.md#D3) | 已解答 | HW-E pp.789、968、1078：buffer/过滤 mask/Tx merge 规则 | 13 邮箱、7 ID、16 字节及 Padding 的实际映射与生成代码 |
| [D4](agent-guide.md#D4) | 已解答 | 保留为功能范围决策 | 是否要求睡眠/唤醒，EcuM 配置及测试 |
| [E1](agent-guide.md#E1) | 已解答 | HW-E pp.257、2859；DS-E p.63：地址/64 B 擦除/4 B 编程规格及带条件寿命 | 用专用 Flash 手册核对操作限制 |
| [E2](agent-guide.md#E2) | 已解答 | HW-E pp.2857、2869：Data Flash BGO，不能一概要求 RAM 执行 | 实际 Fls/Fee FS1x 兼容性、Fls_Cfg、MemMap、专用 Flash 手册 |
| [E3](agent-guide.md#E3) | 已解答 | 无板级资料 | 外部 EEPROM/Flash 器件及连接 |
| [F1](agent-guide.md#F1) | 已解答 | HW-E pp.1522、1531–1535、2884：WDTA0 基址、启动选项、计时公式 | 板上 OPBT0 值与首次喂狗期限 |
| [F2](agent-guide.md#F2) | 已解答 | 已区分 API 毫秒触发额度与硬件超时；补充 AUTOSAR R23-11 §8.3.3 概念依据 | 项目 4.2.2/厂商 Wdg 配置、当前实现和调度周期 |
| [F3](agent-guide.md#F3) | 已解答 | HW-E p.282：WDTA0 75% 中断 EI9 | OS 中命名 ISR 是否确实绑定该来源 |
| [G1](agent-guide.md#G1) | 已解答 | 时钟、内存、硬件 RAM 初始化规则已提取 | reset.850/crt0 的真实初始化顺序和目标编译 |
| [G2](agent-guide.md#G2) | 已解答 | 记录现用编译参数清单 | GHS 文档、库 ABI、gp/ep/tp 与链接符号 |
| [G3](agent-guide.md#G3) | 已解答 | HW-E p.282：FENMI/FEINT 硬件入口线索 | MCU_FENMI_ENTRY 等宏和实际处理函数 |
| [G4](agent-guide.md#G4) | 已解答 | HW-E p.2890：硬件清零与 ECC 初始化及可配置条件 | 复位配置；工程 PORST/TRAPRST 段用途 |
| [G5](agent-guide.md#G5) | 已解答 | HW-E p.2884：OPBT0 只读映射与字段 | 实物选项、Security ID、烧录/调试访问设置 |
| [H1](agent-guide.md#H1) | 已解答 | 无板卡和调试器信息 | 确认工具与连接 |
| [H2](agent-guide.md#H2) | 已解答 | GHS 官方确认 gsrec/S-record 与 ghexfile 工具用途；未取得本地工具 | 安装版本的具体转换命令、烧录器格式要求 |
| [H3](agent-guide.md#H3) | 已解答 | 目标内存边界已核查 | 烧录映射、起始地址及 CRC/校验规则 |
| [H4](agent-guide.md#H4) | 已解答 | 无板级日志接口信息 | UART/其他日志引脚与配置 |
| [I1](agent-guide.md#I1) | 已解答 | 无原理图 | LED/按键的 port/bit/极性 |
| [I2](agent-guide.md#I2) | 已解答 | HW-E pp.101–109 的 PMC/PM/PIBC/PIPC 与复用编码已补充 | 真实 Port 配置和板级映射 |
| [I3](agent-guide.md#I3) | 已解答 | HW-E §2.8 pp.180–186 给出 GPIO/调试/模拟等不同处理规则 | 按实际封装、占用及电气要求选择逐 pin 配置 |
| [J1](agent-guide.md#J1) | 已解答 | 主核 G3M 已确认 | 5.0.39 的 P1M/P1M-E variant 支持、内核模式/栈模型 |
| [J2](agent-guide.md#J2) | 已解答 | 硬件能力与原型辅助逻辑已准备 | 当前端口回调签名/语义、官方 OSTM 示例 |
| [J3](agent-guide.md#J3) | 已解答 | 硬件 EIC/向量规则已确认 | ETAS 命名规则、Cat1/Cat2 对 EIC 的处理 |
| [J4](agent-guide.md#J4) | 已解答 | 0x2800=10 KB，布局算术已检查 | 任务/ISR 栈峰值与实际栈模型 |
| [K1](agent-guide.md#K1) | 已解答 | 已给出以 OSTM1 硬件路线为主线的建议、两路线条件和四回调算法解说 | 正式实施前绑定当前 OS/RTE 契约、资源与边界行为 |

K1 的完整方案见 [计数器设计解说](counter-design.md)。新增模块案例覆盖如下：

| 编号 / 回答入口 | 解说状态 | 已交付内容 | 实际 C 实现状态 |
| --- | --- | --- | --- |
| [R1 Mcu](mcal-reference-guide.md#R1) | 已解答 | 时钟/复位/RAM 责任、初始化和失败处理 | 无完整驱动 |
| [R2 Port](mcal-reference-guide.md#R2) | 已解答 | pin/ALT/电气配置、初始化顺序与占用检查 | 无完整驱动 |
| [R3 Dio](mcal-reference-guide.md#R3) | 已解答 | channel/port/group、极性、读写和并发 | 无完整驱动 |
| [R4 Gpt](mcal-reference-guide.md#R4) | 已解答 | 模式、周期、通知、elapsed/remaining、OS 边界 | OSTM 基础函数已通过主机测试 |
| [R5 Can](mcal-reference-guide.md#R5) | 已解答 | 时序、Tx/Rx、过滤、错误/状态与轮询 | 位时间计算已通过主机测试 |
| [R6 Fls](mcal-reference-guide.md#R6) | 已解答 | 范围/对齐、异步作业、RAM/BGO、存储示例 | 无完整驱动 |
| [R7 Wdg](mcal-reference-guide.md#R7) | 已解答 | 首次期限、窗口、预算、固定/VAC 触发 | 无完整驱动 |

已存在的代码与测试范围见 [参考代码 README](../examples/rh850_mcal_reference/README.md)。本次不再以完整驱动开发、目标构建和上板结果作为解说任务完成条件。
