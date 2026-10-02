# 车载 MCU 厂商格局与路线图

- **本报告回答的问题**：谁在车载 MCU 市场里占多大份额？各家主力家族、核、存储与安全定位是什么？Renesas RH850 的公开路线图是什么，RH850 用户该如何看待长期支持与迁移？
- **调研日期**：2026-10（所有来源访问日期均为 2026-10）
- **主要来源类型**：厂商新闻稿/产品页（厂商营销）、行业媒体、分析机构公开摘要（付费正文未获取）、中文财经/行业媒体

标签：[事实/有来源]、[多来源一致]、[分析师预测]、[推断]。无公开数据处写"公开资料未给出"。

---

## 1. 市场份额（务必注意口径）

| 口径 | 数据 | 来源 / 标签 |
|---|---|---|
| 2025 全球 MCU（全应用）收入 | 约 221 亿美元（-0.3%）；Infineon 23.2%（+1.8 个百分点）居首，其后依次 NXP、Renesas、ST、Microchip；前五约占 80%；其余份额公开资料未给出 | [Omdia 2025 数据，经 heise 报道] [heise](https://heise.de/-11247952) |
| 2024 全球 MCU | Infineon、NXP、Renesas、ST 合计近 70% | [分析师，Yole Group 2025] [edge-ai-vision](https://www.edge-ai-vision.com/2025/12/microcontrollers-enter-a-new-growth-cycle-as-the-market-targets-us34-billion-in-2030/) |
| 车用 MCU 前五（2023） | Infineon、NXP、Renesas、Microchip、ST，合计约 88.43%（已核对；同页称 2023 车用 MCU 市场 118.6 亿美元→2030 年 191.7 亿美元，与 Yole 的 130 亿口径不同）；各家单独百分比摘要中未给出 | [QYResearch 2023 摘要，市场报告销售页] [dri.co.jp](https://dri.co.jp/auto/report/qyr/240929-automotive-microcontrollers-mcu-global.html) |
| 车用 MCU 各家份额（另一搜索摘要） | 摘要给出另一组 2024 年排名（Infineon、NXP、ST、TI、Renesas）但**无法核实出版方与方法**，本报告不引用其数值 | [未核实，不采用] |

[推断] 各口径排名不一致（尤其 Renesas 与 ST/Microchip 的次序），说明车用 MCU 份额的公开数据质量有限；做决策请以付费报告原文为准。车用半导体整体：前五约占 50%（Yole 2025，[edge-ai-vision](https://www.edge-ai-vision.com/?p=54938)）。

## 2. 各厂商

### Infineon
- **AURIX TC3x/TC4x（TriCore）**：TC4Dx 于 2024-11 发布，28 nm，称虚拟化、AI、功能安全、网络均有增强，当时在送样，称 2025 量产；TC4x 使用与 TSMC 合作的 RRAM [事实/有来源，厂商营销] [Infineon](https://infineon.com/cms/en/about-infineon/press/market-news/2024/INFATV202411-018.html)、[Infineon RRAM 2022](https://infineon.com/cms/en/about-infineon/press/market-news/2022/INFATV202211-031.html)。TC4Dx：6 个 TriCore 核（均可锁步）、500 MHz、PPU 并行处理单元用于嵌入式 AI，支持 5 Gbit/s Ethernet、PCIe、10BASE-T1S、CAN-XL [事实/有来源，eeNews 转述厂商新闻稿]。
- **RISC-V**：2025-03 宣布将推出 RISC-V 的 AURIX 新家族，与 TriCore AURIX、Arm 的 TRAVEO/PSOC 并存 [事实/有来源，厂商营销] [Infineon](https://new-origin.infineon.com/cms/en/about-infineon/press/press-releases/2025/INFATV202503-067.html)；2026-03-04 发布 RISC-V Virtual Prototype 的 DRIVECORE 捆绑包 [EE Journal](https://www.eejournal.com/industry_news/infineon-expands-drivecore-portfolio-with-three-new-software-bundles-accelerating-customer-transition-toward-future-risc-v-automotive-microcontrollers/)；样品/量产日期官方稿未给出（媒体摘要称样品 2026、量产约 2028，仅见搜索摘要）。
- **DRIVECORE 软件捆绑**：AURIX DRIVECORE AUTOSAR 捆绑含 Infineon MCAL 与 Vector MICROSAR Classic，TASKING 提供认证工具链，2025-03 随 embedded world 发布 [事实/有来源，厂商营销] [eeNews Europe](https://www.eenewseurope.com/en/drive-core-bundles-software-for-infineon-automotive-microcontrollers/)。
- **TRAVEO**：基于 Arm，具体新品规格本次未取得，公开资料未给出。

### NXP
- **S32K3**：本次未取得一手规格页，公开资料未给出（不转述未核实内容）。
- **S32Z/S32E**：8 个 Cortex-R52 核，2022-06 发布，计划 2023 Q4 量产（后续实际进度本次未核实）[事实/有来源，厂商] [EE Journal](https://www.eejournal.com/industry_news/nxp-extends-s32-automotive-platform-with-s32z-and-s32e-real-time-processor-families-for-new-software-defined-vehicles/)。
- **S32K5**：2025-03-11 发布，称业界首个 16nm FinFET + MRAM 车用 MCU，Cortex 核最高 800 MHz，eIQ Neutron NPU，ASIL-D，PQC 安全加速器；属 S32 CoreRide 平台扩展；生态含 ETAS、Vector、Elektrobit、Green Hills、QNX、Wind River 等 [事实/有来源，厂商营销] [NXP via Nasdaq](https://www.nasdaq.com/press-release/new-s32k5-microcontroller-family-advances-zonal-sdv-architectures-and-extends-nxp)、[Embedded.com](https://www.embedded.com/?p=4494791)。

### Renesas（重点）
- **RH850 家族**：官方称自 2013 年累计出货 40 亿颗以上 [事实/有来源，厂商营销，经 U2C 新闻稿转述] [Renesas](https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications)。
- **RH850/U2B（高端）**：28 nm，最多 4×400 MHz + 3 组 lockstep，虚拟化，EVITA Full，电机控制加速器，部分带 RISC-V 协处理器 DR1000C [事实/有来源，厂商] [Renesas](https://renesas.com/products/microcontrollers-microprocessors/rh850-automotive-mcus/rh850u2b-zonedomain-and-vehicle-motion-microcontroller)。
- **RH850/U2A（中端）**：被 Renesas 定位为中端；详细规格本次未取得，公开资料未给出。
- **RH850/U2C（低端）**：2026-03-04 发布并"现已可用"；4 核最高 320 MHz（含 2 核 lockstep）、最多 8 MB flash、28 nm、ASIL D、ISO/SAE 21434、PQC、CAN-XL/10BASE-T1S/TSN/I3C；称可从 RH850/P1x、F1x 平滑迁移；有 Starter Kit [事实/有来源，厂商营销] 同上。新闻稿未给出明确的家族路线图或长期供货年限。
- **RH850/P1x、F1x**：成熟产品线，新闻稿仅称可迁移至 U2C；官方停产/长期供货承诺（longevity）细节：公开资料未给出。
- **Arm 路线**：2023-11 路线图宣布两类 Arm 架构新 MCU——(1) 带内置 NVM 的 32 位 crossover R-Car MCU，面向 domain/zone，(2) 延伸 RH850 的 "vehicle control" R-Car MCU 系列，面向动力、车身、底盘、仪表；称 2024 年起陆续发布，2024 Q1 起提供虚拟开发环境 [事实/有来源，厂商营销] [Renesas](https://www.renesas.com/us/en/about/press-room/renesas-unveils-processor-roadmap-next-gen-automotive-socs-and-mcus)。本次**未找到**这两类 MCU 已正式发布并具体命名/规格的公开页面，故其当前状态为：公开资料未给出（需自行核查 Renesas 官网）。
- **R-Car Gen5**：SoC 已送样，并提供评估板与 RoX Whitebox SDK（CES 2026 展示）[事实/有来源，经 S&P Global 报道] [S&P Global](https://autotechinsight.spglobal.com/news/5285669/ces-2026-renesas-to-showcase-r-car-gen-5-soc-based-multidomain-solution-platform)。
- **RISC-V**：Renesas 有自研内核的通用 RISC-V MCU 与电机控制 ASSP [事实/有来源] [BusinessWire 2024](https://www.businesswire.com/news/home/20240326133312/en/Renesas-Introduces-Industry%E2%80%99s-First-General-Purpose-32-bit-RISC-V-MCUs-with-Internally-Developed-CPU-Core)；车规 RISC-V 主控 MCU 的公开发布：公开资料未给出。
- **MRAM**：见于 RA8（通用线）与 TSMC 合作报道 [TechInsights](https://www.techinsights.com/blog/can-renesas-enable-high-performance-computing-embedded-mram-automotive-hmi)。
- **工具链**：IAR 称其平台支持 4000+ Renesas 器件（含 RH850）[事实/有来源，厂商营销] [IAR](https://www.iar.com/blog/rh850-mcus-with-iar-powering-next-generation-software-defined-vehicle-development)。

### ST
- **Stellar P6 / SR6**：28 nm FD-SOI，最高 20 MB PCM，最多 6 个 Cortex-R52（部分 lockstep/split-lock），ASIL-D，CAN XL，支持无停机 OTA [事实/有来源，厂商营销] [powersystemsdesign](https://www.powersystemsdesign.com/articles/stmicroelectronics-introduces-stellar-p6-automotive-mcu-for-ev-platform-system-integration/6/19191)。
- **xMemory（ePCM 扩展）**：2025-04 发布，首先用于 Stellar P6，称 2025 年量产，存储单元 18 nm / 28 nm [事实/有来源，厂商] [themachinemaker](https://themachinemaker.com/news/stmicroelectronics-introduces-expandable-memory-technology-for-automotive-microcontrollers/)。
- **SPC5**：本次未取得资料，公开资料未给出。

### TI
- AM263Px-Q1：最多 4 个 400 MHz Cortex-R5F，AEC-Q100 [事实/有来源] [Mouser](https://www.mouser.in/en/new/texas-instruments/ti-am263px-arm-based-mcus/)；F29H85x：64 位 C29 核，称达 ASIL D / SIL 3 [事实/有来源] [Mouser](https://www.mouser.lu/ti-f29h85xtu-mcus)。TI 偏重实时控制/电机/电源与雷达，不走 zonal 大 MCU 路线是[推断]。

### Microchip
- PIC32CZ CA：Cortex-M7，8 MB flash，面向工业与车用 [事实/有来源] [electronica-azi](https://international.electronica-azi.ro/mouser-now-shipping-microchip-technology-pic32cz-ca-microcontrollers-for-secure-industrial-and-automotive-applications)；dsPIC33（含车规与功能安全特性）[事实/有来源] [Microchip](https://www.microchip.com/en-us/solutions/automotive-and-transportation/automotive-products/microcontrollers-and-microprocessors/dspic-dscs)。

## 3. 中国车规 MCU 厂商与国产化

- **整体国产化率**：搜索摘要称车规 MCU 国产率仍低于 5%（另一来源称接近 10%），预测 2026–2030 年升至 10%–20%；MIIT 要求 2025 年单品类 10%、总体 20% 的本地化率（相关指引）[多来源基本一致但口径不一，媒体摘要，出版方原文未核实] [集微网](https://jw.ijiwei.com/n/871460)、[TrendForce 2024](https://www.trendforce.com/news/2024/12/06/news-chinas-automotive-chip-localization-ratio-reportedly-to-hit-15-by-year-end-amid-u-s-reliance/)、[Counterpoint 2026 报告页](https://counterpointresearch.com/en/reports/china-automotive-semiconductor-insight-2026-mcu-power-chip-sensor-chip-communication-chip)。认证周期 3–5 年、ASIL-D 要求与 OEM 供应链锁定被列为障碍（摘要）。
- **芯驰科技 SemiDrive E3**：基于 Arm Cortex-R5F 的高性能 MCU，ASIL-D，称出货达百万级、量产于 40+ 车型 [事实/有来源，厂商/媒体；另称最高 800 MHz、6 核，Gasgoo 页面 403，经搜索摘要与 SemiDrive 官网摘要交叉] [Gasgoo](https://autonews.gasgoo.com/articles/news/semidrive-kicks-off-delivery-of-e3-high-performance-mcu-70021586)、[MarkLines](https://www.marklines.com/en/news/277932)。
- **杰发科技 AutoChips**：AC7870x，2023-10 发布，Cortex-R52 多核，ASIL-D；6 个 Cortex-R52 核、360 MHz；据报道 2026 年通过 TÜV Rheinland ISO 26262 ASIL-D 产品认证（具体月份与 2023-10 发布日期本次未复核）[事实/有来源，媒体，Gasgoo 页面 403，经搜索摘要] [Gasgoo](https://autonews.gasgoo.com/articles/market-industry/2072956708790779905)。
- **兆易创新 GigaDevice**：Frost & Sullivan 称其 2024 年全球 MCU 排名第八，GD32 为中国最大的 Arm MCU 系列；2024 年全部产品出货 43.6 亿颗；GD32A 车规 MCU 累计出货超 800 万颗 [事实/有来源，搜索摘要，经公司财报/媒体转述，集微网原页面 502 未能打开]。原稿“年出货超 24 亿颗、营收第三”本次未能复核，已删除。车规 MCU 业务营收：公开资料未给出。
- **芯旺微 ChipON**：KF32A158 车规 MCU（ASIL B），称在长安、上汽、一汽、广汽量产应用 [仅见搜索摘要，原页面 502 未能打开，未核实]。
- **国芯科技 C*Core**：CCFC3009PT，面向车身/底盘/动力域与跨域控制，22 nm RRAM，多核 RISC-V 架构，500 MHz、约 10,500 DMIPS，24 MB 代码 NVM，0.3 TOPS NPU，支持 PQC；据称已完成研发进入流片试产阶段 [搜索摘要，来源为公司公告/媒体，厂商宣称；原稿“6000+ DMIPS、开发中”已更正]。
- **云途 YTMicro、紫光国芯**：本次未取得可核实的公开资料，公开资料未给出。
- 低端车规 MCU 领域另有比亚迪半导体、CHIPWAYS 等 [搜索摘要] 同上。
- [推断] 国产 MCU 目前以 Cortex-M/R5/R52 为主，对标的是 NXP/ST/Infineon 的 Arm 线；对 RH850 专有 RH850 核无直接替代，OEM 切换成本主要在 MCAL、工具链、功能安全认证与既有软件。

## 4. 对比表（核/工艺/NVM/安全/定位）

| 家族 | 核 | 工艺 / NVM | 安全 | 定位 | 来源 |
|---|---|---|---|---|---|
| Renesas RH850/U2B | RH850，4×400 MHz + 3 组 lockstep，部分 RISC-V DR1000C | 28 nm，片内 flash | ASIL-D，EVITA Full | 高端 zone/domain/vehicle motion | [Renesas](https://renesas.com/products/microcontrollers-microprocessors/rh850-automotive-mcus/rh850u2b-zonedomain-and-vehicle-motion-microcontroller) |
| Renesas RH850/U2C | RH850，4 核 320 MHz（含 2 lockstep） | 28 nm，最多 8 MB flash | ASIL D，21434，PQC | U2 系列低端，底盘/BMS/车身 | [Renesas](https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications) |
| Infineon AURIX TC4x | TriCore（TC4Dx：6 核、500 MHz） | 28 nm，RRAM | 厂商称功能安全增强（等级以官方为准） | 高端，E/E 新架构 | [Infineon](https://infineon.com/cms/en/about-infineon/press/market-news/2024/INFATV202411-018.html) |
| NXP S32K5 | Arm Cortex 至 800 MHz（具体型号未取得）+ NPU | 16 nm FinFET，MRAM | ASIL-D | zonal / 电气化 | [Embedded.com](https://www.embedded.com/?p=4494791) |
| NXP S32Z/E | 8× Cortex-R52 | 工艺/NVM 本次未取得 | 面向安全处理（ASIL 等级以官方为准） | 实时处理，域/区域、EV | [EE Journal](https://www.eejournal.com/industry_news/nxp-extends-s32-automotive-platform-with-s32z-and-s32e-real-time-processor-families-for-new-software-defined-vehicles/) |
| ST Stellar P6 | 最多 6× Cortex-R52 | 28 nm FD-SOI，最高 20 MB PCM | ASIL-D | 电驱/整合，CAN XL | [powersystemsdesign](https://www.powersystemsdesign.com/articles/stmicroelectronics-introduces-stellar-p6-automotive-mcu-for-ev-platform-system-integration/6/19191) |
| TI AM263Px-Q1 | 最多 4× Cortex-R5F 400 MHz | 工艺未取得 | 厂商资料未在本次摘要给出 | 实时控制 | [Mouser](https://www.mouser.in/en/new/texas-instruments/ti-am263px-arm-based-mcus/) |
| Microchip PIC32CZ CA | Cortex-M7 | 8 MB flash | 厂商称面向车用，ASIL 等级未取得 | 网关/图形/通用 | [electronica-azi](https://international.electronica-azi.ro/mouser-now-shipping-microchip-technology-pic32cz-ca-microcontrollers-for-secure-industrial-and-automotive-applications) |
| 杰发 AC7870x | Cortex-R52 多核 | 未取得 | ASIL-D（TÜV 认证，据报道） | 动力/底盘 | [Gasgoo](https://autonews.gasgoo.com/articles/market-industry/2072956708790779905) |
| 芯驰 E3 | Cortex-R5F | 未取得 | ASIL-D | 高性能 MCU | [Gasgoo](https://autonews.gasgoo.com/articles/news/semidrive-kicks-off-delivery-of-e3-high-performance-mcu-70021586) |

## 5. 对 RH850 用户的含义

- **家族仍在更新**：U2C 于 2026-03 发布并声称与 P1x/F1x 可迁移，说明 Renesas 把 RH850 作为现役主力，而非仅维护 [推断，基于事实]。
- **路线图的不确定性**：官方公开的"Arm 化 R-Car MCU / crossover"未见具体量产型号的公开页面（本次检索范围内），RH850 与 Arm 路线如何分工、RH850 在 28 nm 之后是否有下一代：公开资料未给出。
- **Longevity**：本次未找到 RH850 具体长期供货年限承诺，建议向 Renesas FAE 书面确认（PCN、EOL 政策）。
- **迁移路径**：P1x/F1x → U2C/U2A/U2B（同 RH850 核，MCAL 与工具链连续性较好）[推断]；跨到 Arm（R52、S32K5 等）则需重做 MCAL/启动/MPU/核间通信，AUTOSAR 配置层可部分复用。
- **工具链/AUTOSAR 支持**：IAR 列有 RH850 支持 [IAR](https://www.iar.com/blog/rh850-mcus-with-iar-powering-next-generation-software-defined-vehicle-development)；Renesas 称有自家与生态伙伴的车规软件包 [Renesas](https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications)。ETAS RTA-CAR 对具体型号（如 U2C）的 MCAL 支持清单：公开资料未给出，需向 ETAS/Renesas 确认。NXP S32K5 生态已列 ETAS、Vector、Elektrobit 等 [Embedded.com](https://www.embedded.com/?p=4494791)。

---

## 结论要点
1. 份额：Infineon 在 2025 全球 MCU 居首（Omdia 23.2%），NXP/Renesas/ST/Microchip 紧随；车用口径的单家百分比公开数据质量有限。
2. 三家欧系都已给出下一代存储/架构路线（RRAM、MRAM、ePCM；Infineon 另有 RISC-V），Renesas 的公开路线是 RH850 延续 + Arm 化 MCU。
3. RH850 在 2026 年仍有新品（U2C），短中期无断档迹象；长期路线与 longevity 条款需向厂商确认。
4. 中国厂商在 Arm（R5/R52）上追赶，国产率仍在个位数到 10% 区间（口径不一）。

## 对你的意义（RH850 / RTA-CAR 工程师）
- 项目中的 RH850/U2x 经验在当前量产周期内仍有直接价值；把"RH850 特有部分（核特性、INTC、MPU、MCAL）"与"AUTOSAR 通用部分"分开积累，利于将来迁移。
- 建议对 Arm R52 / TriCore 做一次横向学习（启动、MPU、核间通信、Safety 机制），因为这是客户需求变化时最常被问的迁移项。
- 做技术选型文档时，请对市场份额数据标注口径与出处，避免引用未核实的排名。

## 来源列表（访问日期 2026-10）
1. heise（Omdia 2025）https://heise.de/-11247952
2. Yole via edge-ai-vision https://www.edge-ai-vision.com/2025/12/microcontrollers-enter-a-new-growth-cycle-as-the-market-targets-us34-billion-in-2030/ ； https://www.edge-ai-vision.com/?p=54938
3. QYResearch 摘要 https://dri.co.jp/auto/report/qyr/240929-automotive-microcontrollers-mcu-global.html
4. Infineon https://infineon.com/cms/en/about-infineon/press/market-news/2024/INFATV202411-018.html ； https://infineon.com/cms/en/about-infineon/press/market-news/2022/INFATV202211-031.html ； https://new-origin.infineon.com/cms/en/about-infineon/press/press-releases/2025/INFATV202503-067.html
5. NXP https://www.nasdaq.com/press-release/new-s32k5-microcontroller-family-advances-zonal-sdv-architectures-and-extends-nxp ； https://www.embedded.com/?p=4494791 ； https://www.eejournal.com/industry_news/nxp-extends-s32-automotive-platform-with-s32z-and-s32e-real-time-processor-families-for-new-software-defined-vehicles/
6. Renesas https://www.renesas.com/en/about/newsroom/renesas-expands-auto-mcu-portfolio-28nm-rh850u2c-vehicle-control-and-automotive-safety-applications ； https://renesas.com/products/microcontrollers-microprocessors/rh850-automotive-mcus/rh850u2b-zonedomain-and-vehicle-motion-microcontroller ； https://www.renesas.com/us/en/about/press-room/renesas-unveils-processor-roadmap-next-gen-automotive-socs-and-mcus ； https://autotechinsight.spglobal.com/news/5285669/ces-2026-renesas-to-showcase-r-car-gen-5-soc-based-multidomain-solution-platform ； https://www.businesswire.com/news/home/20240326133312/en/Renesas-Introduces-Industry%E2%80%99s-First-General-Purpose-32-bit-RISC-V-MCUs-with-Internally-Developed-CPU-Core ； https://www.techinsights.com/blog/can-renesas-enable-high-performance-computing-embedded-mram-automotive-hmi
7. IAR https://www.iar.com/blog/rh850-mcus-with-iar-powering-next-generation-software-defined-vehicle-development
8. ST https://www.powersystemsdesign.com/articles/stmicroelectronics-introduces-stellar-p6-automotive-mcu-for-ev-platform-system-integration/6/19191 ； https://themachinemaker.com/news/stmicroelectronics-introduces-expandable-memory-technology-for-automotive-microcontrollers/
9. TI https://www.mouser.in/en/new/texas-instruments/ti-am263px-arm-based-mcus/ ； https://www.mouser.lu/ti-f29h85xtu-mcus
10. Microchip https://international.electronica-azi.ro/mouser-now-shipping-microchip-technology-pic32cz-ca-microcontrollers-for-secure-industrial-and-automotive-applications ； https://www.microchip.com/en-us/solutions/automotive-and-transportation/automotive-products/microcontrollers-and-microprocessors/dspic-dscs
11. 中国厂商与国产化 https://jw.ijiwei.com/n/846103 ； https://jw.ijiwei.com/n/871460 ； https://www.trendforce.com/news/2024/12/06/news-chinas-automotive-chip-localization-ratio-reportedly-to-hit-15-by-year-end-amid-u-s-reliance/ ； https://counterpointresearch.com/en/reports/china-automotive-semiconductor-insight-2026-mcu-power-chip-sensor-chip-communication-chip ； https://autonews.gasgoo.com/articles/news/semidrive-kicks-off-delivery-of-e3-high-performance-mcu-70021586 ； https://www.marklines.com/en/news/277932 ； https://autonews.gasgoo.com/articles/market-industry/2072956708790779905
