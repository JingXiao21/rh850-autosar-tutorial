# 12 产业生态调研：Classic AUTOSAR 的"谁、怎么合作、交付什么、RTA-CAR 怎么用"

## 目的

本目录是**基于公开资料**的产业调研，用来支撑本教程：把 [11 入门章节](../11-classic-autosar-primer/README.md) 里抽象的角色（OEM / Tier1 / BSW 栈厂商 / 芯片厂 / 工具厂商）落到具体公司、具体合作模式、具体交付物，并为"在 ETAS RTA-CAR 工程里升级 DCM"这一目标工作准备背景。本目录不替代任何厂商文档；凡厂商文档需登录、未公开或需在真实项目确认的内容，各篇都明确写了"公开资料未确认"。

与 [11/02 谁做什么](../11-classic-autosar-primer/02-who-builds-what.md) 的关系：11/02 讲 METH 方法论角色与典型分工；本目录补充行业实践（具体厂商、OEM 案例、商业模式、生命周期）。02 篇开头给出了两者的模式对应表（本目录 (a)(c)(d) = 11/02 的模式 A/B/C），不存在角色冲突。

## 阅读顺序

| 顺序 | 文档 | 内容概要 |
|---|---|---|
| 1 | [01 产业生态全景](01-autosar-industry-landscape.md) | AUTOSAR 伙伴分层与费用（官网表格已核对）、Core Partner 构成与 2026-01 变化；BSW 栈厂商（Vector、ETAS、EB、Siemens、KPIT、国内厂商）及所有权；MCAL/编译器/工具/服务公司；SDV、OEM 自研、开源（OpenBSW、S-CORE）对格局的影响。 |
| 2 | [02 合作模式与生命周期](02-oem-tier1-vendor-cooperation-models.md) | OEM-Tier1-栈厂商的五种合作模式（含 Toyota/Nissan 的"推荐 BSW 厂商"公开案例）、交付形态与授权（公开能确认与不能确认的商业要素）、缺陷/升级/回归由谁负责，以及对 DCM 升级的含义。 |
| 3 | [03 各方交付物](03-vendor-deliverables.md) | Vector（Package-Based / SIP-Based）、ETAS、EB 的交付形态；Renesas MCAL 公开页面内容；OS 与编译器 port；一张"谁交付什么、什么格式、何时、做什么"的总表与交接流程。 |
| 4 | [04 RTA-CAR 工作流](04-rta-car-workflow.md) | RTA-CAR 公开命名的组件与版本（V12.3.0 下载条目、RTA-OS RH850GHS 5.0.39）、端到端工作流图、RTA-OS 端口概念、诊断配置、授权与下载渠道；列出所有"公开资料未确认"的项。 |
| 5 | [05 Vector 与 EB 工作流对比](05-vector-eb-workflows-comparison.md) | DaVinci 与 EB tresos 的公开工作流，以及与 RTA-CAR 的概念映射（给有 Vector/EB 经验者的迁移表）。 |
| 6 | [06 RTA-CAR 中升级 DCM](06-rta-car-dcm-upgrade-implications.md) | Dcm 在 RTA-CAR 工程里的形态、升级意味着什么、应检查什么、怎样与 ETAS/集成商沟通、回归测试与检查清单。 |

## 一页纸结论

1. **标准组织**：AUTOSAR 官网公开伙伴分层（Core / Premium(+) / Development / Associate）及年费（如 Premium 31,000 EUR + 1.5 FTE）；2026-01 起 DENSO、Huawei、Vector 加入 Core Partner，Core 现有 10 家（含 AUMOVIO、Bosch、BMW、Mercedes-Benz、VW、Toyota、GM）。
2. **BSW 栈厂商**：国际主流为 Vector MICROSAR、ETAS RTA-CAR、Elektrobit tresos AutoCore、Siemens Capital VSTAR（另有 KPIT）；国内有 iSOFT、东软睿驰、经纬恒润等。公开资料没有可靠的市场份额与价格。
3. **所有权**：ETAS 为 Bosch 全资子公司；EB 2015 年被 Continental 以 6 亿欧元收购（AUMOVIO 分拆后 EB 归属仅见二手来源称仍属 Continental，未经官方原文核实）；Vector 为基金会持股的独立公司。
4. **合作模式**：经典模式是 OEM 出规格、Tier1 在自选栈上集成；也有 OEM 推荐/认可栈（Toyota 认可 Mentor VSTAR 与 Vector、Nissan 推荐 EB 并预集成扩展模块）、OEM 提供 SWC 库、OEM 自研。VW/Mercedes 统一指定 Classic 栈的公开证据未找到。
5. **各方交付**：OEM 交通信矩阵/ECU Extract/诊断规范；栈厂商交 BSW 包 + 配置/生成工具；芯片厂交 MCAL（Renesas MCAL 公开页：AR4.2.2、SPAL/COM/TEST，软件包免费 as is）；Tier1 向 OEM 交 hex/A2L/ODX/ARXML 与证据文件。Tier1 是缺陷与升级的枢纽，配置 MCAL 是 ECU Integrator 的任务（与 11/02 一致）。
6. **商业层面公开可知的**：项目/概念许可、RFQ 定制交付与自助包交付（Vector 的 Package-Based 约 3 天获访问）、评估包、支持与更新、ASIL-D 认证声明（厂商自述）。per-ECU 版税、价格、源码交付默认形态：公开资料未给出。
7. **RTA-CAR 组成与版本**：发行包含 ISOLAR-A、ISOLAR-B、RTA-BSW、RTA-RTE、RTA-OS（V12.3.0 条目；RTE 为 12.3.1，各组件版本号可不同），ASAM 另列 RTA-FBL；RTA-OS 按 MCU+编译器端口独立版本化（如 RH850GHS 5.0.39）。下载需 ETAS 许可门户登录，手册/Release Notes 不公开。
8. **RTA-CAR 工作流要点**：ECU Extract/ARXML 导入 ISOLAR → ISOLAR-B 配置 ECUC（含 MCAL 参数定义）→ RTA-RTE/RTA-BSW/RTA-OS 各自生成 → GHS 等工具链编译链接 → 烧写调试。第三方 MCAL 与 ISOLAR-B 的集成方式、Os_Cbk 回调、CDD 导入、支持的 AUTOSAR Release 明细均公开资料未确认。
9. **与 Vector/EB 的差异**：三家流程骨架一致（输入、配置、校验、生成、构建、测试）；Vector 公开强调 as-Code/CI，EB 以 plugin 机制分发 MCAL，RTA-CAR 公开描述为 ISOLAR 配置加多个独立生成器。
10. **DCM 升级的含义**：Dcm 不是可单独替换的 .c 文件，通常等于升级 RTA-CAR/RTA-BSW 发行包或接收供应商专门的模块更新，涉及 BSWMD/参数定义、ECUC 迁移、生成代码、RTE 端口（DataServices/SecurityAccess/Session）、callout 签名与依赖模块，需全量重生成并 diff。
11. **方法**：基线 → 文档/BSWMD 差异 → 迁移 → 全量生成 diff → 回归；把"RTA-CAR + RTA-OS 端口 + MCAL + 编译器 + 工具"当成一个版本组合整体对齐；ASIL 项目向 ETAS 索取目标版本的 safety manual。
12. **格局变化**：SDV 与 OEM 自研平台（VW.OS、MB.OS）集中在中央计算层，Classic MCU 层仍是现实工作面；Eclipse OpenBSW/S-CORE 目前不是 RH850 + AUTOSAR 商用栈的替代品，但 ETAS 参与 S-CORE。

## 信息边界

- **仅公开来源**：官网、新闻稿、在线手册、标准组织页面、媒体报道。不含任何专有或保密信息，不依据未公开合同或内部文档。
- **访问日期**：2026-10（编辑复核同月）。数字（年费、版本号、日期）可能变化，使用前以官网为准。
- **需登录才能读/未能读取**：ETAS 用户手册、Release Notes、Port Guide、Migration Guide、docs.etas.com 的 RTA-CAR 文档；Renesas MCAL 登录区文件与器件表；Vector SIP 逐项目录。
- **抓取失败/仅搜索摘要的来源**：部分页面（如 Gasgoo、MathWorks 连接页、Automotive World、Design News、Business Wire、ST 合作伙伴页）复核时 403 或超时，相关结论在各篇中标注"仅搜索摘要"；Volkswagen Group Italia 的 VW.os 页面已 404。
- **标签体系**：`[事实/有来源]` 有公开出处；`[行业惯例]` 多源一致但无单一权威出处；`[推断]` 作者推断；"公开资料未确认"表示未找到公开证据，不等于不存在。厂商营销表述（"up to 50%"、ASIL-D 等）均为厂商自述，非独立验证。
- 逐条复核记录见 [10 复核日志](../reference/research/10-industry-review-log.md)。

## 后续在真实项目中要确认的问题

**版本与组成（来自 04/06）**
- [ ] 当前 RTA-CAR 版本（读者环境称 12.9.0）及 ISOLAR-A/B、RTA-BSW、RTA-RTE、RTA-OS 各自版本号；RTA-OS RH850GHS 端口版本。
- [ ] RTA-CAR/RTA-BSW 当前与目标版本支持哪些 AUTOSAR Release（R19-11/R20-11/R21-11/R22-11…）？
- [ ] 本地安装目录里是否有 Release Notes、Migration Guide、Port Guide、ISOLAR-B 帮助；它们是否随安装包提供？
- [ ] "RTA-SK / ISOLAR-AB / ISOLAR-EVE / VRTA 虚拟目标"等名称在真实环境是否存在（公开检索未找到）；虚拟 ECU/CI 工具的名称与用法。

**MCAL、OS、编译器（来自 03/04）**
- [ ] Renesas P1M MCAL 的实际 AUTOSAR 版本（Renesas 公开页把 RH850/P1M 列在 AUTOSAR 4.0.3，仓库示例称 AR 4.2.2 API）。
- [ ] 第三方 MCAL 如何导入 ISOLAR-B（BSWMD/参数定义）或用 MCAL 自带配置工具生成；是否有兼容矩阵。
- [ ] RTA-OS RH850GHS 的 Os_Cbk 回调名、中断入口命名、OSTM 绑定与 variant 列表。
- [ ] GHS 编译器版本在 ETAS/Renesas 的认证/支持范围内吗？

**DCM 升级（来自 06/02）**
- [ ] 目标版本中 Dcm 对应的 AUTOSAR Release；能否单独更新 Dcm 还是必须升级整个 RTA-BSW/RTA-CAR？
- [ ] ISOLAR-B 是否提供 ECUC 配置迁移功能与迁移报告；哪些参数需手动处理？
- [ ] DataServices/SecurityAccess 的 RTE 端口或 callout 签名有无变更？
- [ ] ODX/CDD/表格诊断数据的导入格式与脚本是否随版本变化（RTA-CAR 是否直接导入 CDD 公开资料未确认）？
- [ ] Dem、PduR、CanTp、ComM、BswM、SchM 的版本配套要求；已知问题（known issues）里有无 Dcm 相关项。
- [ ] 升级包的获取权限与许可证有效期；ETAS 支持工单入口。

**合同、商业与安全（来自 02/03）**
- [ ] 项目属于 02 篇的哪种模式（a/b/c/d）；OEM 是否有推荐/认可 BSW 厂商或 OEM 扩展模块（MOP 类），升级是否需跟随 OEM 节奏。
- [ ] 支持合同范围：Dcm 缺陷谁出补丁；MCAL 缺陷找 Renesas 还是第三方支持商；许可模式（浮动/节点锁定）。
- [ ] ASIL 项目：目标版本的 safety manual 与认证范围（TÜV 评审等厂商声称需向 ETAS 索取书面范围）。
- [ ] 版税、价格、源码 vs 目标码交付：公开资料未给出，须读合同或问采购/FAE。

**OEM 与诊断需求（来自 02）**
- [ ] OEM 的诊断规范格式（ODX/PDX、CDD、DEXT）与通信矩阵格式（DBC/ARXML/ECU Extract）；OEM 在 DIAG/NM/网关/安全服务上的特例。

**所有权类（来自 01）**
- [ ] 如需引用 EB 所有权：取得 Continental/EB 官方对 AUMOVIO 分拆后归属的原文。
