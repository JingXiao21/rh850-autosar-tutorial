# 车载 MCU 技术与架构趋势

- **本报告回答的问题**：E/E 架构从域控走向区域（zonal）+ 中央计算后，MCU 会怎样变化？存储、核、安全、网络、软件栈各条技术线的公开信号是什么？对 RH850 + Classic AUTOSAR 工程师意味着什么？
- **调研日期**：2026-10（所有来源访问日期均为 2026-10）
- **主要来源类型**：厂商新闻稿（属厂商营销，会标注）、标准组织页面、行业媒体、分析机构公开摘要（付费报告正文未获取）

标签说明：[事实/有来源]、[多来源一致]、[分析师预测]（注明机构与年份）、[推断]。找不到公开数据处写"公开资料未给出"。

---

## 1. E/E 架构：域控 → 区域（zonal）+ 中央计算

- 主流叙述是：功能整合进中央计算（SoC）与若干 zone controller，MCU 负责实时 I/O、网关、安全岛。Renesas 2023 年路线图提出 Arm 架构、带内置 NVM 的 32 位 "crossover MCU"，面向 domain 与 zone control unit，填补高端 SoC 与传统 32 位控制 MCU 之间的性能空白 [事实/有来源，厂商营销] [Renesas 路线图新闻稿](https://www.renesas.com/us/en/about/press-room/renesas-unveils-processor-roadmap-next-gen-automotive-socs-and-mcus)。
- NXP 2022 年发布 S32Z/S32E 实时处理器：8 个 Cortex-R52 核，S32Z 面向安全处理与域/区域控制，S32E 面向电动车控制与智能执行器，集成以太网交换与通信卸载引擎 [事实/有来源，厂商营销] [Electronic Design](https://www.electronicdesign.com/markets/automotive/article/21247325/electronic-design-real-time-processors-target-software-defined-vehicles)、[EE Journal](https://www.eejournal.com/industry_news/nxp-extends-s32-automotive-platform-with-s32z-and-s32e-real-time-processor-families-for-new-software-defined-vehicles/)。2025 年 S32K5 面向 zonal 与电气化架构（16nm FinFET + MRAM）[事实/有来源，厂商营销] [NXP 新闻稿（Nasdaq 转载）](https://www.nasdaq.com/press-release/new-s32k5-microcontroller-family-advances-zonal-sdv-architectures-and-extends-nxp)。
- **对 MCU 数量的影响**：TechInsights 称，高价值的域/区域控制器增加成本的速度快于 ECU 整合削减成本的速度 [分析师预测，TechInsights，Automotive System Demand 2021–2030 页面摘要] [TechInsights](https://techinsights.com/ko/node/52279)（仅搜索摘要，正文未获取）。"MCU 数量更少但单颗更强"的量化数据：公开资料未给出。[推断] 低端传感器/执行器节点（LIN/CAN 小 MCU）仍会大量存在，AUTOSAR Classic 存量不会消失。

### 市场规模（须带出处）
| 指标 | 数值 | 来源 / 标签 |
|---|---|---|
| 全球车用半导体 | 2024 年 680 亿美元 → 2030 年 1320 亿美元（已核对转载原文） | [分析师预测，Yole Group 2025] [edge-ai-vision 转载](https://www.edge-ai-vision.com/?p=54938) |
| 全球 MCU | 2030 年超 340 亿美元，2024–2030 CAGR 6% | [分析师预测，Yole Group 2025] [edge-ai-vision](https://www.edge-ai-vision.com/2025/12/microcontrollers-enter-a-new-growth-cycle-as-the-market-targets-us34-billion-in-2030/) |
| 车用 MCU | 2030 年约 130 亿美元，CAGR 3%（最大单一应用领域） | [分析师预测，Yole Group 2025] 同上 |
| 2025 全球 MCU 收入 | 约 221 亿美元，较 2024 年 -0.3% | [Omdia 数据，经 heise 报道] [heise](https://heise.de/-11247952) |
| 区域控制器市场 | 2036 年 218 亿美元，CAGR 21.4%（2026–2036），2026 年基数 31.2 亿美元（2026-10 打开页面所见；本稿早期版本曾引用 179 亿/12.8%，同一页面数字已变动，说明该小型机构数字不稳定，仅作情景参考） | [分析师预测，Meticulous Research 2026，方法论未核实] [页面](https://meticulousresearch.com/product/automotive-zonal-controller-market-6842) |

[推断] 车用 MCU CAGR 仅 3%，说明"单价上升"与"数量下降"大致抵消，并非爆发性增长市场。

## 2. 性能与核架构

- **多核 + lockstep**：RH850/U2B 最多 4 个 400 MHz 核加最多 3 组双核 lockstep（合计 6 核形态），每核含虚拟化支持与 QoS，部分型号集成基于 RISC-V 的并行协处理器 DR1000C（带向量扩展、ASIL-D 能力）[事实/有来源，厂商页面] [Renesas U2B 产品页](https://renesas.com/products/microcontrollers-microprocessors/rh850-automotive-mcus/rh850u2b-zonedomain-and-vehicle-motion-microcontroller)。RH850/U2C（2026-03-04 发布）：4 个 RH850 核最高 320 MHz（含 2 个 lockstep）、最多 8 MB flash、28 nm，定位 U2 系列低端 [事实/有来源，厂商营销] [Renesas 新闻稿](https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications)。
- **Cortex-R52 路线**：ST Stellar P6 最多 6 个 Cortex-R52 核（部分 lockstep、部分 split-lock）[事实/有来源，厂商营销] [ST 新闻（powersystemsdesign）](https://www.powersystemsdesign.com/articles/stmicroelectronics-introduces-stellar-p6-automotive-mcu-for-ev-platform-system-integration/6/19191)；NXP S32Z/E 8×R52；杰发 AC7870x 基于 R52 [事实/有来源] [Gasgoo](https://autonews.gasgoo.com/articles/market-industry/2072956708790779905)。
- **Cortex-M85 / crossover**：Renesas RA8 系列（通用 MCU，非车规 RH850 线）率先采用 M85；2025-11 报道称新 RA8 达 1 GHz 并加入 MRAM，面向 HMI [事实/有来源，经 TechInsights 转述] [TechInsights](https://www.techinsights.com/blog/can-renesas-enable-high-performance-computing-embedded-mram-automotive-hmi)、[Renesas M85 新闻](https://design-reuse.com/news/55115/renesas-mcu-arm-cortex-m85-processor.html)。车规 crossover 具体产品与规格：公开资料未给出（路线图称 2024 年起陆续发布）。
- NXP S32K5：Arm Cortex 核最高 800 MHz，16 nm FinFET [事实/有来源，厂商营销] [Embedded.com](https://www.embedded.com/?p=4494791)。

## 3. 嵌入式非易失存储：超越 28 nm eFlash

- 行业共识：eFlash 难以推进到 28 nm 以下 [多来源一致，行业媒体] [eeNews Europe](https://www.eenewseurope.com/en/embedded-mram-available-on-22nm-fdsoi)、[mram-info](https://www.mram-info.com/embedded-mram-what-future-holds)。
- 各厂商选择（均为厂商声明）：

| 厂商 | NVM | 工艺 / 状态 | 来源 |
|---|---|---|---|
| Infineon AURIX TC4x | RRAM（与 TSMC，2022-11 宣布，称首批 28 nm RRAM 样品 2023 年底） | 28 nm；TC4Dx 2024-11-06 发布（6 个 TriCore 核、500 MHz，据 eeNews），送样中，称 2025 量产；TC4Dx 具体采用哪种 NVM 在已打开页面未明确（eeNews 仅称“flash”）[推断：沿用 TC4x 的 RRAM 路线] | [Infineon 2022](https://infineon.com/cms/en/about-infineon/press/market-news/2022/INFATV202211-031.html)、[Infineon 2024](https://infineon.com/cms/en/about-infineon/press/market-news/2024/INFATV202411-018.html)（搜索摘要） |
| NXP S32K5 | MRAM | 16 nm FinFET（2025-03-11 发布，已核对）；“写入比 eFlash 快 15 倍以上”仅见 Embedded.com 摘要，本次未在新闻稿检索结果中复核，视为厂商宣称 | [Embedded.com](https://www.embedded.com/?p=4494791) |
| ST Stellar P6 / xMemory | ePCM（相变存储） | 28 nm FD-SOI，P6 最高 20 MB PCM；xMemory 的存储单元采用 18 nm 与 28 nm 工艺 | [powersystemsdesign](https://www.powersystemsdesign.com/articles/stmicroelectronics-introduces-stellar-p6-automotive-mcu-for-ev-platform-system-integration/6/19191)、[ST xMemory 报道](https://themachinemaker.com/news/stmicroelectronics-introduces-expandable-memory-technology-for-automotive-microcontrollers/) |
| Renesas | RH850/U2x 为 28 nm 片内 flash；MRAM 见于 RA8 与 Renesas–TSMC 合作 | 车规 RH850 后续的 NVM 技术：公开资料未给出 | [TechInsights](https://www.techinsights.com/ja/node/56787) |
| 国芯科技 CCFC3009PT | 22 nm RRAM（据搜索摘要） | 开发中 | [集微网](https://jw.ijiwei.com/n/846103)（搜索摘要，未核实原文） |

- 工程意义：RRAM/MRAM/PCM 支持按位覆写（无需整块擦除），OTA 与编程时间显著缩短 [事实/有来源，ST 与 NXP 厂商均有此说]。[推断] Flash 驱动（Fls/Fee）与 Bootloader 对扇区擦除、写粒度的假设在新存储上会变化。

## 4. 虚拟化 / Hypervisor
- RH850/U2B 每核含虚拟化支持，强调多个 ASIL-D 软件分区在同一 ECU 内保证 FFI [事实/有来源，厂商] [Renesas](https://renesas.com/products/microcontrollers-microprocessors/rh850-automotive-mcus/rh850u2b-zonedomain-and-vehicle-motion-microcontroller)。S32K5 称有"软件定义、硬件强制的隔离架构"，生态伙伴含 QNX、Green Hills、Wind River、Elektrobit、ETAS、Vector 等 [事实/有来源，厂商] [Embedded.com](https://www.embedded.com/?p=4494791)。AURIX TC4x 宣传亦含虚拟化 [事实/有来源，厂商] [Infineon](https://infineon.com/cms/en/about-infineon/press/market-news/2024/INFATV202411-018.html)。
- 各 hypervisor 的出货份额：公开资料未给出。

## 5. 信息安全（Security）
- UNECE R155（CSMS）与 R156（SUMS）：新车型自 2022-07 起适用，2024-07 起适用于所有生产车辆 [事实/有来源] [VCA](https://www.vehicle-certification-agency.gov.uk/connected-and-automated-vehicles/cyber-security-and-software-updating/)、[Trustonic](https://www.trustonic.com/?p=18325)；R155 要求可映射到 ISO/SAE 21434，R156 对应 ISO 24089 [事实/有来源，Bureau Veritas] [BV](https://cybersecurity.bureauveritas.com/services/how-to-reach-unece-compliance)。
- 硬件安全：RH850/U2B 满足 EVITA Full [事实/有来源，厂商]；RH850/U2C 声称符合 ISO/SAE 21434 并支持后量子密码（PQC），含中国/国际算法 [事实/有来源，厂商营销] [Renesas](https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications)；S32K5 集成含 PQC 能力的安全加速器 [事实/有来源，厂商] [Embedded.com](https://www.embedded.com/?p=4494791)。
- PQC 标准：NIST 于 2024-08 批准 FIPS 203（ML-KEM）、204（ML-DSA）、205（SLH-DSA）[事实/有来源] [NIST](https://www.nist.gov/news-events/news/2024/08/announcing-approval-three-federal-information-processing-standards-fips)。车用 PQC 落地时间表：公开资料未给出。[推断] 长寿命 ECU（可能 15 年以上）使 crypto agility 与 HSM 固件可更新成为设计要点。

## 6. 功能安全
ASIL-D 已是高端 MCU 的标配宣传：U2B/U2C、S32K5、Stellar，以及杰发 AC7870x（据报道 2026 年获 TÜV Rheinland ISO 26262 ASIL-D 产品认证，具体月份未核实；6 个 Cortex-R52 核、360 MHz，来源为 electronica China 2026 报道摘要）[事实/有来源] [Gasgoo](https://autonews.gasgoo.com/articles/market-industry/2072956708790779905)。[推断] 趋势是用硬件分区/隔离（FFI）在单芯片混合多个 ASIL 软件，而非只靠 lockstep。

## 7. 连接：CAN XL、10BASE-T1S、TSN
- CAN XL（CiA 610-1）已并入 ISO 11898-1:2024（2024-05 发布），数据域最长 2048 字节，PWM 编码可达 10 Mbit/s 及以上 [事实/有来源] [CiA](https://can-cia.org/s/usvER)、[ISO](https://www.iso.org/standard/86384.html)。
- 已集成的 MCU：ST Stellar P 称为首批可用于 2024 车型年的 CAN XL 器件 [事实/有来源，厂商]；RH850/U2C 支持 CAN-XL、10BASE-T1S、TSN（1G/100M）、I3C [事实/有来源，厂商]。
- 10BASE-T1S/TSN 的装车规模：公开资料未给出。

## 8. OTA（A/B 切换）
ST 称其 PCM 可在后台动态分配空间写入新镜像、验证后再切换，实现无停机 OTA [事实/有来源，厂商营销]；U2B 称支持 "no-wait OTA"；NXP 称 MRAM 加速 OTA [事实/有来源，厂商]。[推断] "真 A/B（双倍 flash）"与"虚拟 A/B（动态分配）"各家方案不同，需查各自参考手册。

## 9. 边缘 AI / NPU
S32K5 集成 eIQ Neutron NPU [事实/有来源，厂商] [Embedded.com](https://www.embedded.com/?p=4494791)；U2B 的 DR1000C 为 RISC-V 向量协处理器；AURIX TC4x 宣传含 AI 能力（具体规格本次未取得）。Yole 预计 TinyML 将在 2028 年前至少占全部 MCU 的 10%（全行业，非仅车用）[分析师预测，Yole 2025] [edge-ai-vision](https://www.edge-ai-vision.com/2025/12/microcontrollers-enter-a-new-growth-cycle-as-the-market-targets-us34-billion-in-2030/)。车用 MCU NPU 的实际装车情况：公开资料未给出。

## 10. RISC-V
- Quintauris：Bosch、Infineon、Nordic、NXP、Qualcomm 共同成立（2023-12-22 正式成立，慕尼黑），ST 于 2024-08 作为第六位股东加入；目标是提供兼容的 RISC-V 参考架构与软件，自身不开发核 [事实/有来源] [Hackster](https://hackster.io/news/bosch-infineon-nordic-nxp-and-qualcomm-give-a-name-to-their-risc-v-joint-venture-quintauris-c8e0321e6554)。2025-10 与 Everspin 合作 [事实/有来源] [BusinessWire](https://www.businesswire.com/news/home/20251001508341/en/quintauris-and-everspin-technologies-partner-to-advance-dependable-risc-v-solutions-for-automotive)。
- Infineon 于 2025-03-06 宣布将推出基于 RISC-V 的 AURIX 新家族，覆盖入门到高性能；并给出虚拟原型，Elektrobit、Green Hills、HighTec、IAR、Lauterbach、Tasking 等已开始使用 SDK [事实/有来源，厂商营销] [Infineon](https://new-origin.infineon.com/cms/en/about-infineon/press/press-releases/2025/INFATV202503-067.html)、[design-reuse](https://www.design-reuse.com/news/57514/infineon-automotive-risc-v.html)。2026-03-04 Infineon 发布 DRIVECORE 的 RISC-V Virtual Prototype 捆绑包，称为“即将推出的 RISC-V AURIX 家族”做生态准备 [事实/有来源，厂商] [EE Journal](https://www.eejournal.com/industry_news/infineon-expands-drivecore-portfolio-with-three-new-software-bundles-accelerating-customer-transition-toward-future-risc-v-automotive-microcontrollers/)。样品/量产时间：该新闻稿未给出；德文媒体摘要称首批样品 2026、量产约 2028（仅见搜索摘要，[推断] 级别，以 Infineon 官方为准）。
- Renesas：有 RISC-V 通用 MCU 与电机控制 ASSP [事实/有来源] [BusinessWire](https://www.businesswire.com/news/home/20240326133312/en/Renesas-Introduces-Industry%E2%80%99s-First-General-Purpose-32-bit-RISC-V-MCUs-with-Internally-Developed-CPU-Core)；RH850/U2B 内含 RISC-V 协处理器 IP。本次未找到 Renesas 车规 RISC-V 主控 MCU 的公开发布（公开资料未给出）。

## 11. 电气化（电机控制 / BMS）
U2B 集成电机控制加速器 EMU3S、GTM v4.1、TSG3、RDC3X 旋变/感应位置接口 [事实/有来源，厂商]；U2C 目标含底盘、BMS、车身灯光与电机 [事实/有来源，厂商]；S32E 面向 EV 控制；ST Stellar P6 面向电驱；TI F29H85x 基于 64 位 C29 内核，称可达 ASIL D / SIL 3 [事实/有来源，经 Mouser] [Mouser](https://www.mouser.lu/ti-f29h85xtu-mcus)。

## 12. 软件侧
- AUTOSAR：R25-11 已发布（2025-12-04 线上发布会，AUTOSAR 称 800+ 参与者、250+ 公司，已核对），含 Classic 平台上的 Vehicle Data Protocol、DDS 支持与安全改进 [事实/有来源] [AUTOSAR](https://www.autosar.org/news-events/detail/release-event-r25-11)；合作伙伴 350+ [事实/有来源] [AUTOSAR](https://autosar.org/about/partners)。
- Classic 在深度嵌入式控制 MCU 上仍然合适，Adaptive 面向高算力中央/区域 SoC [多来源一致，偏厂商/咨询博客] [Siemens](https://blogs.sw.siemens.com/ee-systems/2022/04/27/what-is-autosar-classic-platform-r20-11/)、[Promwad](https://promwad.com/news/autosar-adaptive-central-compute-classic-platform-bottleneck)。Classic 具体市占率：公开资料未给出。
- 开源：Eclipse S-CORE（2024 年起，初始成员 Accenture、BMW、ETAS、Mercedes-Benz、Qorix）[事实/有来源] [Eclipse 新闻](https://newsroom.eclipse.org/node/42840)、[auto-innovations](https://www.auto-innovations.net/news/103156-etas-launches-open-auto-software)；OpenBSW 是 C++ 编写的 MCU BSW 栈（生命周期、CAN、DoCAN、UDS；Eclipse 项目处于 Incubating 阶段，Apache-2.0），初始贡献由 ESR Labs（Accenture 旗下）主导，仓库版权头为 Accenture 2024，即源头为 Accenture/ESR Labs [事实/有来源，仓库 README + Eclipse 项目页 + 社区博客]；“与 Zephyr 集成、可叠加 ICC1 AUTOSAR 层”仅见 ESR Labs 博客，未独立核实 [ESR Labs](https://medium.com/@ESRLabs/openbsw-a-code-first-software-platform-for-automotive-microcontrollers-609d3406cf0d)。
- Zephyr：2024 年获 IEC 61508 认证概念的书面批准（目标 SIL 3，Route 3S，SEooC，约 15,000 行内核范围）；项目表示后续计划 ISO 26262（早期表述 ASIL D）；两者均未完成认证 [事实/有来源] [Zephyr Project](https://www.zephyrproject.org/zephyr-project-rtos-first-functional-safety-certification-submission-for-an-open-source-real-time-operating-system/)。

---

## 结论要点
1. 架构迁移是"重新分工"而非 MCU 消失：Yole 预测车用 MCU 2030 年约 130 亿美元、CAGR 3%，价值向高端 zone/实时 MCU 集中。
2. 28 nm 是 eFlash 的天花板；Infineon（RRAM）、NXP（MRAM）、ST（ePCM）已各自押注新 NVM。Renesas 车规 RH850 目前仍是 28 nm，下一代 NVM 公开资料未给出。
3. Renesas 公开路线是"RH850 继续 + Arm 化 R-Car MCU/crossover"；Infineon 公开宣布 RISC-V AURIX。
4. 安全、网络（CAN XL、10BASE-T1S、TSN）、PQC 已写入 2025–2026 新品规格。
5. AUTOSAR Classic 仍在持续发布（R25-11）；开源（S-CORE/OpenBSW）是补充而非短期替代。

## 对你的意义（RH850 / Classic AUTOSAR 工程师）
- RH850 技能中短期仍有价值：U2C 在 2026 年才发布，说明家族仍在扩展。[推断] 长期需关注 Arm（R52/crossover）迁移。
- 值得补强：多核/lockstep 与 FFI 分区（MPU、OS-Application、保护 hook）；HSM/加密栈（Csm/Crypto/KeyM）；Flash/NVM 驱动与 OTA（Fls/Fee/Bootloader）；CAN XL/Ethernet（EthIf/TSN/SOME-IP）。
- 迁移的是"芯片抽象层"而非 AUTOSAR 方法论：MCAL、启动代码、核间通信会变，BSW 配置思路可复用 [推断]。
- 开源 BSW 可作为旁路阅读以了解趋势，不必现在替换 RTA-CAR 工作流。

## 来源列表（访问日期 2026-10）
1. Renesas 路线图 https://www.renesas.com/us/en/about/press-room/renesas-unveils-processor-roadmap-next-gen-automotive-socs-and-mcus
2. Renesas RH850/U2C 新闻稿 https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications
3. Renesas RH850/U2B 产品页 https://renesas.com/products/microcontrollers-microprocessors/rh850-automotive-mcus/rh850u2b-zonedomain-and-vehicle-motion-microcontroller
4. NXP S32K5 https://www.nasdaq.com/press-release/new-s32k5-microcontroller-family-advances-zonal-sdv-architectures-and-extends-nxp ； https://www.embedded.com/?p=4494791
5. NXP S32Z/E https://www.electronicdesign.com/markets/automotive/article/21247325/electronic-design-real-time-processors-target-software-defined-vehicles ； https://www.eejournal.com/industry_news/nxp-extends-s32-automotive-platform-with-s32z-and-s32e-real-time-processor-families-for-new-software-defined-vehicles/
6. Infineon https://infineon.com/cms/en/about-infineon/press/market-news/2024/INFATV202411-018.html ； https://infineon.com/cms/en/about-infineon/press/market-news/2022/INFATV202211-031.html ； https://new-origin.infineon.com/cms/en/about-infineon/press/press-releases/2025/INFATV202503-067.html ； https://www.design-reuse.com/news/57514/infineon-automotive-risc-v.html
7. ST Stellar https://www.powersystemsdesign.com/articles/stmicroelectronics-introduces-stellar-p6-automotive-mcu-for-ev-platform-system-integration/6/19191 ； https://themachinemaker.com/news/stmicroelectronics-introduces-expandable-memory-technology-for-automotive-microcontrollers/
8. eNVM 与 M85 https://www.eenewseurope.com/en/embedded-mram-available-on-22nm-fdsoi ； https://www.mram-info.com/embedded-mram-what-future-holds ； https://www.techinsights.com/ja/node/56787 ； https://www.techinsights.com/blog/can-renesas-enable-high-performance-computing-embedded-mram-automotive-hmi ； https://design-reuse.com/news/55115/renesas-mcu-arm-cortex-m85-processor.html
9. Yole（经 edge-ai-vision）https://www.edge-ai-vision.com/?p=54938 ； https://www.edge-ai-vision.com/2025/12/microcontrollers-enter-a-new-growth-cycle-as-the-market-targets-us34-billion-in-2030/
10. heise（Omdia）https://heise.de/-11247952
11. TechInsights https://techinsights.com/ko/node/52279 ； Meticulous Research https://meticulousresearch.com/product/automotive-zonal-controller-market-6842
12. R155/R156 https://www.vehicle-certification-agency.gov.uk/connected-and-automated-vehicles/cyber-security-and-software-updating/ ； https://www.trustonic.com/?p=18325 ； https://cybersecurity.bureauveritas.com/services/how-to-reach-unece-compliance
13. NIST PQC https://www.nist.gov/news-events/news/2024/08/announcing-approval-three-federal-information-processing-standards-fips
14. CAN XL https://can-cia.org/s/usvER ； https://www.iso.org/standard/86384.html
15. Quintauris https://hackster.io/news/bosch-infineon-nordic-nxp-and-qualcomm-give-a-name-to-their-risc-v-joint-venture-quintauris-c8e0321e6554 ； https://www.businesswire.com/news/home/20251001508341/en/quintauris-and-everspin-technologies-partner-to-advance-dependable-risc-v-solutions-for-automotive
16. Renesas RISC-V https://www.businesswire.com/news/home/20240326133312/en/Renesas-Introduces-Industry%E2%80%99s-First-General-Purpose-32-bit-RISC-V-MCUs-with-Internally-Developed-CPU-Core
17. AUTOSAR https://www.autosar.org/news-events/detail/release-event-r25-11 ； https://autosar.org/about/partners ； https://blogs.sw.siemens.com/ee-systems/2022/04/27/what-is-autosar-classic-platform-r20-11/ ； https://promwad.com/news/autosar-adaptive-central-compute-classic-platform-bottleneck
18. S-CORE/OpenBSW https://newsroom.eclipse.org/node/42840 ； https://www.auto-innovations.net/news/103156-etas-launches-open-auto-software ； https://medium.com/@ESRLabs/openbsw-a-code-first-software-platform-for-automotive-microcontrollers-609d3406cf0d
19. Zephyr https://www.zephyrproject.org/zephyr-project-rtos-first-functional-safety-certification-submission-for-an-open-source-real-time-operating-system/
20. TI F29H85x https://www.mouser.lu/ti-f29h85xtu-mcus ； AutoChips（Gasgoo）https://autonews.gasgoo.com/articles/market-industry/2072956708790779905 ； 集微网 https://jw.ijiwei.com/n/846103
