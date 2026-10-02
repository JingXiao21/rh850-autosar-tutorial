# RH850 + AUTOSAR Classic 中文教学项目

一个**个人学习 / 准备用的教育项目**：以 **R7F701381 = RH850/P1M-E**（core RH850G3M，CAN = RS-CANFD）为参考器件，建立从硬件到应用的 AUTOSAR Classic 系统级 mental model：

```text
RH850 (CPU / Memory / Clock / Port / Interrupt) → MCAL → CAN Driver → CanIf → CanTp → PduR → DCM → RTE → SWC
```

目标是以后打开真实的 RH850 + RTA-CAR 工程时，能快速定位每一层、知道在哪里下断点、看什么配置，并为 DCM 升级做好准备。它**不是**量产 ECU 工程，也不是对任何真实项目的指令；所有内容区分 `[AUTOSAR Standard]` / `[RH850 Hardware]` / `[Educational Implementation]` / `[Real Project Consideration]`。

## 从这里开始

- **[教程总目录（Master Index）](docs/rh850-autosar-tutorial-content.md)**：Part I–IX 全部章节、完成状态、前置 / 后续章节、对应 SWS release 与源码、已知限制。
- [学习路线图](docs/00-learning-roadmap.md)：依赖图、学习顺序、里程碑与自测题。
- [docs/README.md](docs/README.md)：docs 目录入口。
- 端到端汇聚章：[UDS 端到端：CANoe → SWC → CANoe](docs/08-integration/05-uds-end-to-end.md)。
- **[烧录前离线检查 harness](docs/flash-preflight/README.md)**：HEX/ELF 一致性、器件白名单、完整擦除扇区与证据哈希；[给公司 agent 的执行指令](docs/flash-preflight/AGENT_GUIDE.md)。不连接或烧录 ECU。

## 可运行的教学代码（主机 PC，需要 Python 3 与 PATH 上的 gcc）

| 命令 | 内容 | 输出 |
|---|---|---|
| `python tools/run_can_irq_demo.py` | [ECU 启动到 OS 接管](examples/can_irq_demo/startup/README.md) + [CAN FIFO/EI190 实验](examples/can_irq_demo/README.md)；中文注释，14 项启动测试 + 16 项 CAN 测试 | `artifacts/can-irq-demo/boot_trace.txt`、`trace.txt`、`results.txt` |
| `python tools/run_uds_demo.py` | [examples/uds_diag_demo](examples/uds_diag_demo/README.md)：Virtual CAN → Can → CanIf → CanTp → PduR → Dcm → Rte → VehicleInfoSWC，跑通 `22 F1 90` 等 UDS 请求 | `artifacts/uds-demo/trace.txt`、`results.txt` |
| `python tools/run_host_tests.py` | [examples/rh850_mcal_reference](examples/rh850_mcal_reference/README.md)：OSTM 驱动、CAN FD 位时间、tick 累加的主机测试 | `artifacts/host-build/results.txt`（每次覆盖） |
| `python tools/build_hardware_index.py --check` | 寄存器索引（`docs/hardware-registers.json/.csv`）一致性检查 | — |

以上代码都只在 PC 上运行，**没有在真实 RH850 硬件上验证过**。

## 资料与规划

- 规划：[claude_plan.md](claude_plan.md)（当前计划）、[分析与学习计划](docs/analysis-and-learning-plan.md)
- 事实底稿与复审：[docs/reference/research/](docs/reference/research/00-writing-conventions.md)

## 参考资料获取（PDF 不随仓库分发）

教程引用的 PDF 页码来自下列资料。它们受 Renesas / AUTOSAR / NXP 版权保护，**本仓库不包含 PDF 及其抽取文本**，请从官方渠道下载后放到仓库根目录（`r01uh0585ej0120.pdf`、`TJA1041A.pdf` 放到 `references/downloads/`）：

| 文件名 | 内容 | 来源 |
|---|---|---|
| `r01uh0585ej0120.pdf` | RH850/P1M-E User's Manual: Hardware Rev.1.20（教程主依据 HW-E） | [Renesas](https://www.renesas.com/en/document/mah/rh850p1m-e-group-users-manual-hardware-rev120) |
| `r01ds0505ed0100-rh850p1m-e.pdf` | RH850/P1M-E Datasheet（DS-E） | [Renesas RH850/P1M-E 产品页](https://www.renesas.com/en/products/rh850-p1m-e) |
| `REN_r01uh0436ej0140-rh850p1x_MAH_*.pdf` | RH850/P1x User's Manual: Hardware（对照用） | Renesas 官网文档搜索 R01UH0436 |
| `REN_r01ds0506ed0100-rh850p1x-c_DST_*.pdf` | RH850/P1x-C Datasheet（对照用） | Renesas 官网文档搜索 R01DS0506 |
| `AUTOSAR_CP_SWS_MCUDriver.pdf` (R24-11)、`AUTOSAR_CP_SWS_IOHardwareAbstraction.pdf` (R24-11) | MCU Driver / IoHwAb SWS | [AUTOSAR Classic Platform 标准](https://www.autosar.org/standards/classic-platform) |
| `AUTOSAR_SWS_CANDriver.pdf` (R22-11)、`AUTOSAR_SWS_DiagnosticCommunicationManager.pdf` (R20-11) | CAN Driver / DCM SWS | 同上（选择对应 release） |
| `AUTOSAR_SWS_Diagnostics.pdf` (AP R22-11) | Adaptive Platform Diagnostics（仅概念对照） | [AUTOSAR Adaptive Platform 标准](https://www.autosar.org/standards/adaptive-platform) |
| `TJA1041A.pdf` | NXP CAN transceiver datasheet | NXP 官网 |

生成可 grep 的分页文本（`=== PDF PAGE n ===`，教程中的页码即 PDF 页码）：

```bash
python -m venv .venv && .venv/Scripts/pip install -r tools/requirements.txt   # Linux/macOS: .venv/bin/pip
.venv/Scripts/python tools/pdf_extract.py *.pdf references/downloads/*.pdf      # 输出到 artifacts/pdf-text/
```

### AUTOSAR Classic Platform 全套 SWS（R25-11，按 stack 分类）

```bash
python tools/download_autosar_specs.py            # 默认 R25-11，约 170 MB，105 份
python tools/download_autosar_specs.py --release R24-11
```

下载到 `specs/autosar-cp-R25-11/<stack>/`（MCAL、CAN stack、Com services、Diagnostics、Memory、System services、Crypto、RTE…），目录说明见 [specs/autosar-cp-R25-11/README.md](specs/autosar-cp-R25-11/README.md)。PDF 同样不提交到 git。

## License

[MIT](LICENSE)。引用的第三方规范、手册与 openAUTOSAR 源码版权归各自所有者；本仓库只包含对其的说明与页码 / 行号引用。

## 前一阶段交付（legacy）

项目上一阶段是“给内部 agent 的 RH850/P1M-E 技术解说交付”。这些文件保留作线索，顶部均有 Legacy banner（迁移去向与已知更正），**不再维护**：

- [原单页教程 v1](docs/legacy/rh850-autosar-tutorial-content-v1.md)
- [CAN / 诊断 / OS 硬件交接](docs/rh850-hardware-handoff.md)、[复审记录](docs/hardware-review.md)、[硬件核查报告](docs/hardware-findings.md)
- [40 项技术问题解答](docs/agent-guide.md)、[计数器设计](docs/counter-design.md)、[MCAL 七模块参考案例](docs/mcal-reference-guide.md)、[旧指令](docs/internal-agent-task.md)、[覆盖追踪](docs/requirements-status.md)
- [资料索引](docs/source-index.md)、[网上参考评估](docs/online-references.md)、[原截图需求提取](requirements-extracted.md)
- [上一阶段计划](plan.md)、[旧上板实施计划](plan-implementation-history.md)
