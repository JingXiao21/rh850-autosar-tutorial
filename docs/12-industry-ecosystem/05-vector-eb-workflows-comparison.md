# 05 Vector 与 EB 工作流对比（及与 RTA-CAR 的概念映射）

## 本报告回答的问题

- Vector MICROSAR + DaVinci 与 EB tresos Studio + AutoCore 的公开工作流是什么？
- 与 ETAS RTA-CAR 相比概念如何对应？从 Vector 项目转到 RTA-CAR 项目要换哪些心智模型？

## 调研日期

2026-10（编辑复核同月，逐条结果见 docs/reference/research/10-industry-review-log.md）。

## 主要来源

Vector DaVinci Configurator Classic 6.3 在线手册、Vector 产品页、Elektrobit/MathWorks 产品页、TI 公开的 EB tresos 使用指南、eeNews 诊断配置报道。完整列表见文末。

> 标签：[事实/有来源]、[行业惯例]、[推断]。RTA-CAR 细节见 [04-rta-car-workflow.md](./04-rta-car-workflow.md)；交付物对比见 [03-vendor-deliverables.md](./03-vendor-deliverables.md)。

---

## 1. Vector MICROSAR + DaVinci

### 1.1 工具分工

- **DaVinci Developer**：AUTOSAR SWC 设计，定义接口、端口、Runnable，支持 ARXML 编辑与 RTE 相关配置 [来自搜索摘要：[M1] MathWorks 页复核时 403，未能原页核对]。
- **DaVinci Configurator Classic**：配置、校验、生成 MICROSAR Classic BSW 与 RTE 的核心工具；官网称强调 Configuration-as-Code、CI/CD 集成与 AI 辅助工作流，并可从 OEM 系统与诊断描述自动生成 BSW 配置 [事实/有来源：[V1]，2026-10 复核；"6 版""新 UI"字样见在线手册 What's New in Version 6 目录]。
- 公开手册以 "Classic" 命名；"Pro" 与 "Classic" 的版本关系本轮公开资料未能区分 [未覆盖]。
- 架构：client-daemon，含 CLI client 与两代 UI client [事实/有来源：[V4]]。

### 1.2 工作流（以公开手册为准）

1. **创建工程**：指定工程名、目标目录与 **BSW Package** 路径（即 Vector 交付的 MICROSAR 包，见 03 §1.1）；也可用创建文件（creation file）创建 [事实/有来源：[V3]，复核时原页可打开，页内目录含 Creation Methods: From Scratch / From Creation File]。
2. **添加 Modules**：决定 BSW Package 的哪些模块纳入工程（通信、诊断、存储等） [事实/有来源：[V2]]。
3. **配置 Containers**：在 Basic Editor 中配置模块下的容器与参数 [事实/有来源：[V2]]。
4. **导入输入文件（Input Files）**：以 ECU Extract 为主，可用 derive-ecuc 命令派生；诊断数据经转换工具导入；"ECU Extract Producer" 为配套工具，可生成 invariant/variant 提取件 [事实/有来源：[V2][V4]]。
5. **更新工程（Update Project）**：消除输入文件变更造成的不一致 [事实/有来源：[V2]]。
6. **校验（Validation）**：检查完整性、一致性、正确性；CLI 形如 dvcfg-b project validate；成功提示 "Validation Successful"；界面提供 solving action 快速修复 [事实/有来源：[V5][V4]]。
7. **生成（Generate）**：运行各模块生成器；支持 Hybrid Generator，可配置生成器序列并接入外部生成器 [事实/有来源：[V4]]。生成物完整清单公开页未列全 [未覆盖]。
8. **构建与测试**：由 Tier1 自有构建系统编译；CANoe、vTESTstudio 用于仿真与测试 [事实/有来源：[T1]]。

文件格式：EcuExtract.arxml、ecuc.Initial.arxml 等 ARXML [事实/有来源：[V2]]。

### 1.3 诊断导入

CANdelaStudio 可将 ODX 等格式迁移生成 AUTOSAR DEXT，DaVinci Configurator 读取 DEXT 自动配置诊断相关 BSW 模块 [事实/有来源：[D1]]。

### 1.4 MCAL 集成

Renesas MCAL 页面的社区区有一位用户（2023-06）询问配置 Renesas MCAL 该选 DaVinci Configurator 还是 EB tresos [事实/有来源：[R1]]——这只是提问，**不构成** Renesas 的背书或认证关系；官方认证关系与具体 RH850 支持矩阵未公开 [未覆盖]。另：EB 官网称其评估包覆盖 Renesas RH850/F1KM（见 03 §1.3）；Vector 帮助页称 BSW Package 缺少 MCAL Supply 时需自行集成，并提供 3rd Party MCAL Integration Helper（见 03 §1.1）。

---

## 2. EB tresos Studio + AutoCore

### 2.1 概念

- **EB tresos Studio**：Eclipse 基础的配置/生成工具；GUI 与命令行均可触发生成；代码生成引擎、参数校验语言、数据模型接口开放，便于接入既有工具链 [来自搜索摘要：[B1] 403、[B2] 页面未列该细节，均未能原页核对]。
- **Project**：配置工程，含项目名、位置、目标器件，容纳模块配置与生成输出 [事实/有来源：[B3]]。
- **Module Configuration**：选择所需 plugin 后加入各模块配置 [事实/有来源：[B3]]。
- **Plugin**：含 ARXML 与 XDM 配置定义、头/源模板、BSWMD 生成、manifest [事实/有来源：[B3]]。
- 官方提供引导式工作流逐步指引 BSW 配置 [来自搜索摘要：[B2]，未能原页核对]。

### 2.2 工作流

1. **导入**：系统描述（AUTOSAR XML）、DBC、LDF、OIL、Fibex 等导入器 [来自搜索摘要：[B2]，未能原页核对；TI 指南 [B3] 的目录可见 Im-/Exporters 功能]。
2. **配置**：各模块；AutoCalc 可自动计算可推导值 [事实/有来源：[B3]]。
3. **校验**：对工程右键验证，显示错误和警告 [事实/有来源：[B3]]。
4. **生成**：生成配置头、源代码、链接时配置等 [事实/有来源：[B3]]。
5. **导出**：可创建 Exporter 导出 ARXML（示例为 4.3.1），与 DaVinci 等工具互通 [事实/有来源：[B3]]。
6. **MCAL 集成**：TI 以 EB tresos plugin 形式分发 MCAL（[B3] 复核："MCAL modules are delivered in the form of EB Tresos plugins"，参数定义为 xdm 格式）；Infineon 同类说法 [I1] 仅搜索摘要 [事实/有来源，Infineon 未复核]。
7. 构建/测试：由项目自有环境完成 [行业惯例]。

---

## 3. 与 RTA-CAR 对比（高层）

| 维度 | Vector | EB | ETAS RTA-CAR |
|---|---|---|---|
| 工具名 | DaVinci Developer / Configurator | EB tresos Studio | ISOLAR-A / ISOLAR-B 等 [事实/有来源：[E2]] |
| 配置格式 | ARXML（ECU Extract、ecuc） | ARXML + XDM | ARXML（ASAM 目录称 ISOLAR-A 支持多种标准以与第三方互操作 [E2]；未明说 ARXML）；ISOLAR-B 细节见 04 |
| RTE 生成器 | MICROSAR RTE，由 DaVinci 驱动 | AutoCore 内（细节未调研） | RTA-RTE [E2] |
| OS | MICROSAR OS | AutoCore OS [B1，仅搜索摘要；EB 官网产品线页列有 EB tresos 操作系统] | RTA-OS [E2] |
| MCAL 集成 | BSW Package 可含 MCAL Supply，否则用 3rd Party MCAL Integration Helper（03 §1.1） | plugin 生态，TI 采用 [B3 已复核]、Infineon [I1 仅搜索摘要] | 见 04（公开资料未确认） |
| 诊断导入 | DEXT（CANdelaStudio）到 Dcm/Dem [D1] | 本轮未覆盖 | 见 04 |
| 许可可见性 | 订阅，价格未公开 [V1] | 商务许可；合作方有评估许可 [I1] | 见 04 |

## 4. 从 Vector 转到 RTA-CAR：概念映射

| Vector 概念 | RTA-CAR 对应（高层，细节见 04） |
|---|---|
| BSW Package（Vector 另有 SIP Based Delivery 交钥匙交付，见 03 §1.1） | RTA-CAR 发行包（ISOLAR-A/B、RTA-BSW、RTA-RTE、RTA-OS；见 04 §1） |
| DaVinci Developer（SWC 设计） | ISOLAR-A [E2] |
| DaVinci Configurator | ISOLAR-B（BSW 配置） [E2] |
| Input Files（ECU Extract） | 同样导入 ECU Extract (ARXML) 到 ISOLAR [推断] |
| Validation / Solving Action | ISOLAR 校验（界面见 04） |
| Generate | RTA-BSW / RTA-RTE / RTA-OS 各生成器 [E2；Macnica 页（04 的 S11）旁证] |
| CLI（dvcfg-b） | ETAS 命令行/脚本，见 04 |
| DEXT 诊断导入 | 视 ETAS 支持，见 04 |

术语说明：ECU Extract、BSWMD、ECUC 的含义与 [11/02](../11-classic-autosar-primer/02-who-builds-what.md) §5 一致；"BSW Package / SIP" 是 Vector 的交付术语，EB 对应物为 tresos plugin，RTA-CAR 对应物见 04。

思维差异 [推断]：Vector 倾向一套工具驱动全套；ETAS 公开描述为 ISOLAR 配置加多个独立生成器（RTE/OS/BSW）的组合。

## 结论要点

1. 三家流程骨架一致：输入（ECU Extract/DBC）、配置、校验、生成、构建、测试。[事实/有来源]
2. 差异主要在工具形态、plugin 机制与 CLI/as-Code 能力。
3. Vector 公开强调 as-Code/CI；EB 的 MCAL plugin 生态在公开资料中出现较多。[事实/有来源，后者带推断]

## 对你的意义

把已有 Vector 经验按第 4 节映射；重点补学 ISOLAR-A/B 分工、RTA-OS 配置、与 Renesas MCAL 的集成路径（见 04）。

## 未能公开确认的缺口

DaVinci Pro 与 Classic 版本关系；Vector/EB 对 RH850 MCAL 的支持矩阵；EB 诊断导入细节；各工具生成物完整清单；价格。

## 来源列表（访问日期 2026-10）

- [V1] https://www.vector.com/en/product/microsar-classic/davinci-configurator-classic/
- [V2] https://help.vector.com/davinci-configurator-classic/en/6.3/user-manual/getting-started/workflow/local/configure-first-project.html
- [V3] https://help.vector.com/davinci-configurator-classic/en/6.3/user-manual/project-setup/from-scratch-project.html
- [V4] https://help.vector.com/davinci-configurator-classic/en/6.3/user-manual/fundamentals/davinci-configurator.html
- [V5] https://help.vector.com/davinci-configurator-classic/en/6.3/user-manual/getting-started/workflow/local/check-configuration.html
- [M1] https://cn.mathworks.com/products/connections/product_detail/product_35766.html
- [D1] https://www.eenewseurope.com/en/autosar-compliant-diagnostic-configuration-at-the-press-of-a-button/
- [R1] https://www.renesas.com/en/software-tool/renesas-mcal
- [B1] https://es.mathworks.com/products/connections/product_detail/eb-tresos.html
- [B2] https://www.elektrobit.com/?p=1807 （搜索摘要引用；2026-10 复核页面未见导入器清单/引导式工作流）
- [B3] https://software-dl.ti.com/mcu-plus-sdk/esd/PLATFORM_SW_MCAL/AM263x/10.02.00/src/MCAL_Configuration_and_EB_Tresos.html
- [I1] https://infineon.com/cms/en/tools/aurix-embedded-sw/AUTOSAR/Infineon
- [E1] https://ch.mathworks.com/products/connections/product_detail/isolar-a.html （复核时 403，本文已改引 [E2]）
- [E2] https://www.asam.net/members/product-directory/detail/etas-isolar-a
- [T1] https://www.vector.com/en/product/vteststudio/
