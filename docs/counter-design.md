# K1：Rte_TickCounter 的方案与四回调解说

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**（RH850 + AUTOSAR 中文教程），不是公司真实工程；文中“内部 agent / 内部工程 / 交付”等说法只是当时的写作语境，不代表任何真实公司项目、真实配置或指令。正文以下保持原样，仅作溯源；学习请以新教程章节为准。
>
> - **已复审并迁移到**：Counter/Alarm、硬件计数器“四回调”概念 → [02-autosar-classic/06](02-autosar-classic/06-os-task-isr.md)；OSTM 时基与回绕 → [03-mcal/05](03-mcal/05-gpt-driver.md)；tick 累加器 → `examples/rh850_mcal_reference/integration/Tick_Accumulator.[ch]`。
> - **已知更正 / 限定**：① “OSTM0 已由 Gpt 占用、OSTM1 为 OS 候选”来自截图工程，是**配置选择而非硬件事实**；② “四回调”是 RTA-OS 硬件计数器接口的**概念名**，签名与语义须以 RTA-OS 用户手册和 RH850 port 文档确认（本仓库无 OS SWS）；③ 算术推导经复核无误。详见 [01-project-and-docs-review.md](reference/research/01-project-and-docs-review.md) §3 与 [05-consistency-review-log.md](reference/research/05-consistency-review-log.md)。
> - **行号说明**：本 banner 在第 2 行之后插入了 6 行。其他文档（如研究笔记 01、`analysis-and-learning-plan.md`）引用的本文件行号均指插入前版本，对应当前行号 **+6**。

本文件回答原 K1 以及 C2～C6、J2/J3。它是内部 agent 的设计参考，不是 RH850GHS 5.0.39 的接口声明。目标端口的签名和调度语义未取得，以下伪代码统一使用概念名，避免误当可直接链接的 OS 驱动。硬件依据见 HW-E §6、§22 及 [硬件报告](hardware-findings.md)。

## 1. 方案结论

**以 OSTM1 自由运行硬件计数器为主线；Gpt 1 ms 通知驱动软件计数器作为条件备选。** 理由是截图中的 RTE 已要求 HARDWARE，而且 OSTM0 已由 Gpt 占用，OSTM1 具有自由运行比较和 EI75。OSTM1 是否空闲、OS 接口契约和最长关中断时间仍是实现输入，不能只因硬件存在就宣布已集成可用。

| 比较项 | OSTM 硬件路线 | Gpt 软件路线 |
| --- | --- | --- |
| 与当前生成要求 | 不改变截图中的 HARDWARE 要求 | 必须证明 RTE 12.9.0 可以正式生成 SOFTWARE，或使用该版支持的其他接法 |
| 时间来源 | 持续自由运行的 CNT | 周期通知中的逻辑计数推进 |
| 主要工作 | 四回调、匹配 ISR、回绕维护、到期竞争处理 | 配置/重新生成、合法调用上下文、周期丢失与延迟分析 |
| 资源 | 独占 timer/比较寄存器/对应中断；维护任务可读时间 | 独占一个逻辑推进入口；周期 timer 可由现有 Gpt 驱动管理 |
| 易错点 | Cancel 停表、CMP 过期、长目标截断、原始计数漏圈 | 手改生成文件、重复 IncrementCounter、中断合并导致漏 tick |
| 进入正式实现的条件 | 当前端口能表达全部状态与事件语义，所选硬件满足 Cancel/Set 契约 | 生成配置能稳定保留 SOFTWARE，应用行为与 OS API 调用规则成立 |

没有证据证明软件路线不能做，也没有证据证明只改一处计数器类型就能做。旧 9.1.1 工程仅作结构参考。

## 2. 时间模型：先把三个单位分开

| 名称 | 单位 / 模数 | 用途 |
| --- | --- | --- |
| raw | 硬件 counts，32 位模数 2^32 | CNT、CMP 与硬件时序 |
| logical time | 扩展的单调时间及余数 | 在软件中识别回绕、远期目标、历史时间 |
| OS tick | 1 ms，模数 65536 | 对外呈现的计数器值 |

`counts_per_tick = counter_hz / 1000`。当前参考函数要求整除，非整除频率应拒绝或另用有明确误差界的有理数累计器，不能直接向下取整。

```text
sample(raw):                         # 所有调用串行化，计数器不重置
    delta = unsigned32(raw - last_raw)
    total = remainder + delta        # 使用足够宽的整数
    elapsed_ticks = total / counts_per_tick
    remainder = total % counts_per_tick
    logical_ticks += elapsed_ticks
    last_raw = raw
    os_value = logical_ticks modulo 65536
```

上述推导要求相邻实际采样间隔严格小于一次硬件回绕。只看两次 CNT，无法检测中间是否多绕了一整圈。已有 [Tick_Accumulator.c](../examples/rh850_mcal_reference/integration/Tick_Accumulator.c)只保存对外 tick 与余数，足够演示连续换算；如果要实现这里的扩展绝对 deadline，还需在集成层增加相应宽度的 epoch/单调时间状态，不能假装该文件已实现完整 scheduler。

| 频率 | 每 ms counts | 32 位回绕 | 半圈时间 |
| --- | --- | --- | --- |
| 80 MHz | 80000 | 53.6870912 s | 26.8435456 s |
| 20 MHz | 20000 | 214.7483648 s | 107.3741824 s |

OS 的一个完整周期为 65.536 s。80 MHz 下一个 65535 tick 的未来目标对应 5242800000 counts，超过 32 位；不能把乘积强转 uint32 后装入 CMP。20 MHz 下同样距离为 1310700000 counts，低于半圈，但仍必须保证跨长时间空闲时的采样维护。

不要对所有 OS tick 差使用 int16：该判据只在已证明距离小于 32768 tick 时有意义。单独一个相同的 16 位值，也不能区分“现在到期”与“下一圈同值”；必须由 Set 参数契约及保留的时间上下文确定。

将已经确定为未来的 tick 边界换算成 CMP 时，还要减去当前 tick 内已走过的余数。若一致快照为 `(raw_now, logical_ticks, remainder)`，目标比当前整数 tick 大 Δ，则剩余 counts 为 `Δ×counts_per_tick−remainder`，先用宽整数计算并检查范围，再与 raw_now 相加。例：20 MHz、当前 tick=42、余数=5000、目标 tick=45，剩余为 55000 counts，而不是 60000。到期/过去目标先走补发分支，不能让减法下溢。若 Set 契约定义的是相对当前时刻的延时而非 tick 边界，则转换公式必须相应调整。

## 3. 连续采样与长目标的维护方案

建议模型采用扩展时间，并将一次硬件比较安排限制在一个明确的短 horizon 内，例如 **1 s 仅作设计示例**。80 MHz 下 1 s=80000000 counts，小于半圈，便于到期判定。真实 horizon 应根据系统最大中断屏蔽、调试停机、节能模式和服务预算确定。

可选维护方式：

1. 已存在且保证运行的周期服务只读取 OSTM1，更新累计器；它不写 OSTM1 配置，不成为第二个驱动所有者。若系统可能关中断超过一圈，此保证不成立。
2. 所有者用 OSTM1 比较事件同时安排维护检查点和真正到期事件。检查点到达只更新时间并重装下一检查点，**不得误报 OS alarm 到期**。
3. 选择经过证实的较低计数时钟以扩大范围，同时继续证明采样周期和分辨率满足要求。不能只把配置数字从 80 MHz 改成 20 MHz 而不配置时钟链。

第二种方式涉及 Cancel：若 OS 契约要求对应中断在 Cancel 后绝不能再次挂起，就不能继续通过同一请求源发维护中断。此时使用第一种独立维护路径，或重新设计硬件事件门控；不能靠 ISR 中忽略事件来冒充硬件从未挂起。若具体硬件和端口无法共同满足契约，应记录硬件路线不成立，而不是弱化 Cancel。

## 4. 回调与内部状态

建议由适配层独占以下状态（概念字段，不是 ETAS 结构体）：

| 字段 | 意义 |
| --- | --- |
| last_raw / remainder / logical_ticks | 连续时间基准 |
| armed | 是否有有效的 OS 目标 |
| target_deadline | 已转换成单调时间的目标，不是裸 16 位值 |
| compare_deadline | 当前实际装入 CMP 的短期比较点 |
| generation | Set/Cancel 每次改变，识别过期的软件事件 |
| pending_reason | 无、维护检查点、真正到期；硬件标志另行核对 |
| owner / initialized | 防止重复初始化、重启或并发接管 |

| 回调角色 | 应做什么 | 不应从名字猜测什么 |
| --- | --- | --- |
| Now | 采样并返回当前 OS 模数下的时间 | 可调用核心/上下文、返回类型、是否允许内部临界区 |
| Set | 解码目标、装载比较点、处理写入过程中已到期 | 参数是绝对 tick、相对延时还是别的编码 |
| State | 一致地描述当前目标/运行/挂起信息 | 真实结构体字段及 Running 指 timer 还是事件 |
| Cancel | 撤销本代目标、处理残留请求、保持时间连续 | 仅 EIMK=1 就满足全部取消语义 |

必须从生成头文件/端口说明填写接口绑定表后才写真实 `Os_Cbk_*_Rte_TickCounter`。还需取得硬件 ISR 应调用的 OS 推进入口以及它是否会同步调用 Set/Cancel；后者决定锁的边界和重入策略。

## 5. Set、ISR、Cancel 的伪代码

以下 `critical_*`、`request_delivery`、`ack_source` 等都是待适配的概念操作，不对应未核验的具体 EIC 指令或 OS API。临界区需防止任务与 ISR 竞态；它不会让硬件停止计时。

```text
set_target(os_argument):
    critical_enter()
    now = sample(CNT)
    deadline = decode_using_verified_os_contract(os_argument, now)
    generation += 1
    armed = true
    target_deadline = deadline
    retire_previous_request_using_verified_source_protocol()
    if deadline <= now:
        request_delivery(generation)
    else:
        compare_deadline = min(deadline, now + allowed_horizon)
        write_CMP(convert_extended_deadline_to_raw(compare_deadline))
        enforce_required_access_order()
        now_after_write = sample(CNT)
        if now_after_write >= compare_deadline:
            request_one_compare_service(generation)
    critical_exit()

compare_service():
    critical_enter()
    acknowledge_and_sample_using_verified_source_protocol()
    if no_valid_event_for_current_generation:
        finish_without_os_notification()
    else if armed and now >= target_deadline:
        mark_this_generation_as_delivered()
        delivery = true
    else:
        maintain_timebase_and_rearm_next_short_compare()
    critical_exit()
    if delivery:
        call_verified_os_advance_entry()   # OS 可能同步重设/取消，勿持有不允许重入的锁

cancel_target():
    critical_enter()
    sample(CNT)
    armed = false
    generation += 1
    cancel_software_delivery()
    suppress_and_clear_hardware_event_as_required_by_os_contract()
    preserve_continuous_CNT_and_timebase()
    critical_exit()
```

`generation` 只能识别软件记录；硬件 EIRF 不带代次。先退休旧请求、后装新目标的顺序仍须精确设计，不能仅靠代次解决所有竞态。用于补发的请求必须与硬件匹配合并为一次有效分发；回调不应擅自从错误上下文同步执行 OS ISR。

硬件 EIMK 阻止 CPU 接收，不阻止产生请求。EIRF 在允许的软件置位/清除场景下也必须遵守访问和同步规则；随意对 EIC 整体读改写会改变其他位或丢事件。HW-E pp.267–268 提供硬件限制，具体汇编/访问序列由端口设计落实。

## 6. 软件计数器备选怎么讲解

前提是 RTE/OS 正式配置可以生成所需 SOFTWARE counter；重新生成之后不再出现矛盾的 HARDWARE 需求，且 alarm/schedule table 的引用和时间单位一致。

```text
on_valid_1ms_gpt_notification():
    if this_callback_is_the_only_owner_of_software_counter_progress:
        invoke_IncrementCounter_in_a_permitted_context()
```

原理简单，但不能忽略以下约束：

- Gpt 通知周期来自实际时钟和 period，不是配置文本写了 1 ms 就成立。
- 若屏蔽中断导致多个周期合并成一个请求，一次调用只能推进一次，不能假装没有丢失。补偿累计 tick 是否允许、会不会触发大量到期动作，应按 OS 语义另定。
- 不能同时在任务、Gpt callback、另一个 ISR 和旧硬件回调里推进同一计数器。
- 若通知上下文不允许调用所需 OS API，必须采用该端口允许的分发方式，并计入延迟；不能只改变函数名字。
- 不以永久手改 RTE/OS 生成文件作为解决方案；应保存可重新生成的输入配置。

## 7. 供 agent 评审的行为用例

| 情景 | 期望推理结果 | 当前证据 |
| --- | --- | --- |
| 多次 Now 读取同一 CNT | 时间不增加 | 已有主机测试 |
| CNT 从大值回绕到小值，采样间隔小于一圈 | 时间连续，余数不丢失 | 已有主机测试 |
| 16 位 OS tick 从 65535 到 0 | 合法模数回绕，不重置原始时间基准 | 已有主机测试 |
| 采样间隔达到或超过硬件一圈 | 保证失效，不能从两个读数恢复真实 elapsed | 设计限制，不能由 mock 证明环境满足 |
| 80 MHz 下安排 65 s 远期目标 | 用扩展时间和检查点，不截断乘积 | 设计解说，未实现完整 ISR |
| 写 CMP 前/中/后目标到期 | 到期事件最终可见且只分发一次 | 待真实接口实现时检查 |
| Cancel 与 ISR 交错、紧接新 Set | 旧事件不误触发新目标；时间不中断 | 待真实接口实现时检查 |
| OS advance 同步调用 Set | 不死锁、不覆盖新 generation | 待真实接口实现时检查 |
| 无 alarm 的长时间空闲 | 时间仍按保证的维护路径连续 | 设计必须证明，而非假定 |
| Stop 再 Start 自由计数器 | raw 原点重置，应重新初始化时间模型 | OSTM 基础行为已有主机模型测试 |

本文件已完成方案问题的回答；以上“待真实接口实现时检查”是后续实施的验证条件，不是本次文档任务的未答问题。
