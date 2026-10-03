# docs/ — RH850 + AUTOSAR Classic 中文教程

这里是一套**教学 / 准备项目**的中文教程：以 RH850/P1M-E（R7F701381）为参考器件，从硬件一路向上讲到 MCAL、CAN Driver、CanIf / CanTp / PduR、DCM、RTE / SWC，最终串成 `22 F1 90`（读 VIN）的端到端链路。

## 从哪里开始

烧录前检查：[Flash preflight harness](flash-preflight/README.md) 与 [公司 agent 执行指令](flash-preflight/AGENT_GUIDE.md)。只做离线校验，默认缺少公司器件配置或烧录计划时不能通过。

| 入口 | 用途 |
|---|---|
| **[Master Index（学习指南总目录）](rh850-autosar-tutorial-content.md)** | 全部章节、完成状态、前置 / 后续章节、对应 SWS 与源码、§23 十个问题的答案位置、已知限制 |
| [00 学习路线图](00-learning-roadmap.md) | 章节依赖图、学习顺序与时间估计、里程碑、每 Part 自测题 |
| [分析与学习计划](analysis-and-learning-plan.md) | Phase 1 对现有项目、AUTOSAR SWS、openAUTOSAR、RH850 资料的分析与规划 |
| [写作约定](reference/research/00-writing-conventions.md) | 事实来源优先级、关键已确认事实、`[AUTOSAR Standard]` / `[RH850 Hardware]` / `[Educational Implementation]` 等标记 |

## 运行教学 demo（主机 PC，无需 RH850 硬件）

需要 Python 3 和 PATH 上的主机 `gcc`（C99；不使用 RH850 目标工具链）。

新实验：[CAN 接收 → 硬件 FIFO → EI190 → ISR](../examples/can_irq_demo/README.md)，包含中文讲解和注释、错误 EI90 绑定实验、清标志竞争及 FIFO 溢出测试。

启动前篇：[ECU 复位 → CRT → EcuM → OS 接管](../examples/can_irq_demo/startup/README.md)，提供主机模型、14 项测试及目标启动汇编/C 参考，范围停在 OS 接管。

```text
python tools/run_can_irq_demo.py # examples/can_irq_demo：14 项启动测试 + 16 项 CAN 测试
                                # 输出 artifacts/can-irq-demo/boot_trace.txt、trace.txt、results.txt
python tools/run_uds_demo.py     # examples/uds_diag_demo：Virtual CAN → Can → CanIf → CanTp → PduR → Dcm → Rte → SWC
                                 # 输出 artifacts/uds-demo/trace.txt、results.txt
python tools/run_host_tests.py   # examples/rh850_mcal_reference：OSTM、CAN FD 位时间、tick 累加的主机测试
                                 # 输出（覆盖）artifacts/host-build/results.txt
```

如何阅读 demo 的 trace，见 [08-integration/04 F190 VIN Demo](08-integration/04-f190-vin-demo.md)；demo 与 SWS 的已知偏差 D1–D6 见 [demo README](../examples/uds_diag_demo/README.md)。

## 目录

- `01-rh850/` … `09-real-project-preparation/`：按 Part I–IX 组织的章节（编号即阅读顺序）
- 独立指南：[调试手册](debugging-autosar-diagnostics.md)、[DCM 升级指南](dcm-upgrade-guide.md)、[SWC + RTE 教程](autosar-swc-rte-tutorial.md)
- 启动失败 / 早期 trap 调试系列：[10-boot-debug/README.md](10-boot-debug/README.md)
- **Classic AUTOSAR 入门（原理与工作流，新手从这里开始）**：[11-classic-autosar-primer/README.md](11-classic-autosar-primer/README.md)
- 产业生态调研（ETAS / Vector / EB、OEM 合作模式、RTA-CAR 工作流）：[12-industry-ecosystem/README.md](12-industry-ecosystem/README.md)
- 商业调研（MCU 趋势、机器人实时软件）：[13-market-research/README.md](13-market-research/README.md)
- **动手项目：可运行的迷你 Classic AUTOSAR ECU（看 RTE / SWC / OS 代码架构）**：[14-mini-autosar-project/README.md](14-mini-autosar-project/README.md)
- `reference/`：API / 模块 / 配置地图、术语表、源码追踪表；`reference/research/`：写作底稿与复审日志
- `legacy/` 及带 Legacy banner 的文件：上一阶段产物，只作线索，不再维护
