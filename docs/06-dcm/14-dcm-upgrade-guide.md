# DCM 升级（章节入口）

> Prerequisite: [05 DCM 配置](05-dcm-configuration.md)、[11 DCM 运行流程](11-dcm-runtime-flow.md)、[13 DCM 调试](13-dcm-debugging.md)
> Next: [Part VII — SWC 概念](../07-rte-swc/01-swc-concept.md)
> 完整内容: **[docs/dcm-upgrade-guide.md](../dcm-upgrade-guide.md)**（独立指南，`claude_plan.md` §9 要求）；进入真实项目第一周的准备见 [09-real-project-preparation/07](../09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md)
> 对应规范: SWS DCM **CP R20-11**（Change History p.1–5）。本仓库**没有** R21-11 及之后的 CP DCM SWS。

---

本章只是 Part VI 目录中的入口，**不重复**独立指南的内容。按 [analysis-and-learning-plan.md §6.2](../analysis-and-learning-plan.md) 的去重规则，DCM 升级的方法论只维护在 [docs/dcm-upgrade-guide.md](../dcm-upgrade-guide.md) 一处。

## 1. 为什么 Part VI 的最后一章是"升级"

前 13 章建立了 DCM 的内部模型（DSL / DSD / DSP、会话、安全、DID、DEM、MainFunction、调试）。升级把这些知识用在"两个版本之间"：同一个模型，比较两种实现、两套配置、两份规范。本教程的最终目标之一正是"升级 RTA-CAR 中的 DCM"——该项目不在本仓库，其版本与实现**需在真实项目环境中确认**。

## 2. 指南结构速览

| 指南章节 | 回答的问题 | 与 Part VI 哪章相关 |
|---|---|---|
| [§1 阅读陌生 DCM](../dcm-upgrade-guide.md#1-如何阅读一个陌生-dcm-implementation) | 为升级而读：读两份、看边界、出对照表 | [01](01-dcm-overview.md)、[09/03](../09-real-project-preparation/03-how-to-read-dcm.md) |
| [§2 识别版本](../dcm-upgrade-guide.md#2-如何识别-dcm-version) | 版本宏、`Dcm_GetVersionInfo`（`SWS_Dcm_00065`）、Module ID 53（0x35）、BSWMD、代码指纹 | — |
| [§3 对比 Release](../dcm-upgrade-guide.md#3-如何对比两个-autosar-release) | R20-11 Change History 证据；规范瑕疵；R3.1.5 → R4.x 演练 | [02](02-dsl.md)、[07](07-security-access.md)、[09](09-dtc-dem.md) |
| [§4 API](../dcm-upgrade-guide.md#4-如何分析-api-changes) | TP、ComM、Dem、BswM、类型 | [02](02-dsl.md)、[12](12-dcm-mainfunction.md) |
| [§5 配置](../dcm-upgrade-guide.md#5-如何分析-configuration-changes) | 高影响 ECUC 参数、默认值、约束 | [05](05-dcm-configuration.md) |
| [§6 callout](../dcm-upgrade-guide.md#6-如何分析-callback-changes) | `UsePort` 决定的 ReadData/WriteData/GetSeed/Routine 原型与语义 | [08](08-did.md)、[07](07-security-access.md) |
| [§7 Dcm_Cfg](../dcm-upgrade-guide.md#7-如何分析-dcm_cfg) | 配置指纹表：语义比较而非文本 diff | [05](05-dcm-configuration.md) |
| [§8 生成代码](../dcm-upgrade-guide.md#8-如何分析-generated-code) | 同一配置两次生成后 diff | — |
| [§9 依赖](../dcm-upgrade-guide.md#9-如何分析-integration-dependencies) | PduR/CanTp/DEM/NvM/RTE/BswM/EcuM/SchM/ComM/Application；**为什么不能只替换 Dcm.c** | [08-integration/03](../08-integration/03-dcm-integration.md) |
| [§10 回归测试](../dcm-upgrade-guide.md#10-如何做-regression-test) | 总线级用例、黄金基线；以 demo 12 个用例为模板 | [10](10-uds-services.md)、[13](13-dcm-debugging.md) |

## 3. 三句话记住升级

1. **DCM 升级 ≠ 替换 `Dcm.c`**：静态代码、BSWMD、配置迁移、重新生成（Dcm、PduR、RTE、SchM、BswM…）、依赖模块兼容、callout/SWC 适配、回归测试，一个都不能少。
2. **没有证据的部分显式列为待确认**：本仓库的 CP DCM 证据止于 R20-11。
3. **行为差异只能靠总线级回归发现**：NRC 顺序、0x78、会话切换时机、安全访问计数——在升级前录好黄金基线。

## 4. 练习

在本仓库中做一次"纸面升级"：以 openAUTOSAR 的 Dcm（R3.1.5）为旧版本、`examples/uds_diag_demo/diag/`（R4.x / R20-11 API）为新版本，按指南 §3.5 的表逐行复核证据，再按 §9.3 解释"为什么只把 openAUTOSAR 的 `Dcm.c` 换成 demo 的 `Dcm.c` 不可能工作"。

## 5. 对未来真实项目的意义

[Real Project Consideration] 进入真实项目后，从 [09/07 第一周清单](../09-real-project-preparation/07-rtacar-dcm-upgrade-preparation.md) 开始，再按 [DCM 升级指南](../dcm-upgrade-guide.md) 的 §2 → §10 顺序推进，并用 §12 的总检查清单收尾。

## 6. 下一章

Part VI 结束。[Part VII](../07-rte-swc/01-swc-concept.md) 进入 RTE 与 SWC：DCM 的 callout 和端口最终连接到哪里。
