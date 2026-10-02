# 10 产业生态调研（docs/12）复核日志

- 复核日期：2026-10-02。范围：`docs/12-industry-ecosystem/01–06`。
- 方法：用 Windows curl（带浏览器 UA）重新打开报告中全部 96 个唯一 URL，转文本后按原文断言逐条 grep；curl 失败的页面再用 WebFetch/WebSearch 补查。
- 结果概览：96 个 URL 中 79 个可读取（其中 3 个仅得导航/二进制内容，细节未能核对）、1 个 404（AUTOSAR Newsletter Q1 2026）、1 个 404（VW.os Italy 页）、其余 403/超时（mathworks×5、gasgoo×4、automotiveworld×2、computer-automation、designnews、medium、mobilityengineering、businesswire、st.com×2、fptsoftware；其中 globenewswire、businesswire、fptsoftware 经 WebFetch 补查，businesswire 仍 403）。
- 处理的断言：确认 34 条、更正 11 条、降级 17 条、删除 5 条（见下表，未逐条列出的确认项归在同一行）。
- 标签规则：[确认] 原页支持；[更正] 与原页不符，已改；[降级] 原页未找到，改为推断/未复核/仅搜索摘要；[删除] 与来源矛盾或无来源，已移除。

## 一、ETAS RTA-CAR（04、06、01、03、05）

| URL | 结果 | 修改 |
|---|---|---|
| etas.com .../rta-car-v12-3-0/ (S3) | [确认] 组件 ISOLAR-A/B 12.3.0、RTA-BSW 12.3.0、RTA-RTE 12.3.1、RTA-OS 12.3.0；ZIP 3.3 GB；05/11/2024 | 无 |
| etas.com .../rta-os-rh850ghs-product-installer/ (S4) | [确认] V5.0.39、ZIP 6.4 MB、11/27/2024 | 无 |
| etas.com .../autosar-classic-profile-rta-car/ (S1) 与旧分类页 (S2) | [确认] up to 50% less memory、ASIL-D、ISO 21434/UN-R155、60+ MCU 与编译器端口（40+ 合作伙伴）、350+ customers、Academy/Webinars/Support 入口、与 CI/CD 集成；两个 URL 内容相同。[更正] 页面正文**未**逐项列出 ISOLAR-A/B/RTA-BSW/RTA-OS/RTA-RTE；[降级] "虚拟化"字样不存在；[删除] "RTA-OS 单栈、栈需求 over 50%"（页面写的是 "RT-OS reduces memory usage up to 50%"） | 04 §1 组件清单改引 S3/S5/S11；虚拟化降级；RTA-OS 句改写；01/03/05/06 同步 |
| asam.net .../etas-isolar-a (S5) | [确认] ISOLAR-A 为设计工具；RTA-CAR 含 ISOLAR-B、RTA-RTE、RTA-OS、RTA-BSW、RTA-FBL "and more"；MCD-2 D/NET。[更正] ASAM 措辞是 ISOLAR-A 与 RTA-CAR "互操作"，发行包含它的依据是 S3 | 04/03 注明口径 |
| shop.lhpes.com 与 lhpes.com news (S7) | [确认] ISOLAR A/B 培训，学员有临时 ETAS ISOLAR A/B 许可；[删除] "RTA-FBL 属 RTA-CAR"（页面未见）、"60 分钟 RTA-CAR webinar"（页面未见） | 04 §1、§7 |
| nobleprog.co.uk .../autosarbasa (S10) | [降级] 只见 ISOLAR-A/B 概览、Dem、NvM、CANoe 诊断；**未见** "ODX container 导入""DCM/DID/RID/DTC 配置" | 04 §5 改写 |
| electronicspecifier.com ?p=135666 (S8) | [确认] 2014-03，ETAS 扩展 RTA 以覆盖 AUTOSAR 4.x BSW；"more than one billion ECUs" | 04 §2；01 引用 10 亿 ECU 并标 2014 年厂商自述 |
| macnica.co.jp .../etas/products/144617 (新增 S11) | [确认] RTA-BSW/RTE/OS 概述；RTA-BSW "forms the R4.x platform"；RTA-BSW 可用于最高 ASIL-D；RTA-RTE ISO 26262 ASIL-D 认证；BSW 含 diagnostic protocols | 04/06 新增来源 |
| tasking.com/content/partner/etas/ (新增 S12) | [确认] RTA-OS 2008 年首发、52 个目标端口、OSEK/VDX 2.2.3 | 04 §4 |
| docs.etas.com (S9) | [确认] 公开列表无 RTA-CAR（有 ASCET、INCA、RTA-SQF 等） | 04 来源注记 |
| ETAS "TÜV SÜD 评审 RTA-BSW"（01/02） | [降级→推断/未复核]：ETAS、Macnica、Tasking 页面均未找到该句 | 01、02 改写 |
| marklines.com/ja/news/340217 | [确认] "Robert Bosch の100%子会社 ETAS GmbH"（2026-02）；S-CORE 部分未取到 | 01 所有权行；"1994 成立"降级 |
| bosch.co.jp basic-data PDF | [降级] 二进制，未能核对 ETAS 条目 | 01 以 MarkLines 为准 |
| st.com ETAS partner page、neusar-4-0 | 超时，仅搜索摘要 | 01 标注 |

## 二、Vector（03、05、02）

| URL | 结果 | 修改 |
|---|---|---|
| vector.com/en/product/microsar-classic-package-based-delivery/ (V1) | [确认] Package-Based Delivery（初始订阅、在线仓库下载、无需详细报价）；"Software Integration Package (SIP) Based Delivery" 为交钥匙方案 | 03 §1.1 引用原页 |
| vector.com/microsar-classic (V6) | [确认] ASIL D、generic/customer-specific、平台与编译器覆盖、ASPICE/21434/R155；[删除] "BSW 模块归入 cluster 并组合成各自 SIP"（原页无） | 03 删除并加注 |
| help.vector.com DaVinci-Package-Manager custom_package_vs_package_based_delivery | [确认] CSP（RFQ→报价→组装测试）与 Package Based（约 3 天、更新无前置时间）；新增来源 V7 | 03、02 §5 |
| help.vector.com .../the_embedded_packages | [确认] 5 类包、MOP/MPP 多份、PSP、"3rd Party MCAL Integration Helper"；新增来源 V8 | 03 §1.1；05 §1.4 |
| help.vector.com davinci-configurator-classic 6.3: configure-first-project / check-configuration / from-scratch-project / fundamentals | [确认] BSW Package 路径、`dvcfg-b project validate -p -b`、"Validation Successful"、Derive ECU Extract、ECU Extract Producer、Hybrid Generator、Creation Methods。V3（from-scratch）并非"搜索摘要"，可打开 | 05 来源注记更新 |
| vector.com .../davinci-configurator-classic/ (V1/V5) | [确认] Configuration-as-Code、CI/CD、AI 辅助、自 OEM 系统/诊断描述自动生成 BSW 配置；[降级] "6 版新 UI"以手册目录为准 | 05 |
| vector.com .../evaluation-bundles-and-packages-overview | [确认] 评估免费、含完整栈与 DaVinci 工具；[降级] "评估包为目标码"原页未见 | 02 §5.1 降级为推断 |
| vector.com .../application-areas/diagnostics/ | [确认] CANdelaStudio 支持 ODX 2.0.1/2.2.0；PREEvision 支持 ODX 与 AUTOSAR DEXT；Vector 提供 DCM/DEM BSW | 无 |
| eenewseurope .../autosar-compliant-diagnostic-configuration | [确认] CANdelaStudio 8.5 SP2 起导出 AUTOSAR DEXT（2017-02） | 无 |
| eenewseurope .../vector-canoe-12-0 (T2) | 可打开；MCD-2 MC/MCD-2 D 细节未逐字核对 | 无 |
| vector.com .../vteststudio/ (T1) | [确认] vTESTstudio 为测试设计环境 | 无 |
| certification.vector.com .../view.php?id=451 | [确认] DIAG(Dem/Dcm)、SYS、COM(NM)、Gateway、加密/私有传输协议等差异区；BSW 模块可聚合 | 无 |
| en.wikipedia.org/wiki/Vector_Informatik | [确认] 2011 年股份转入家族基金会与非营利基金会；[降级] 60%/40% 仅见搜索摘要，"94% 表决权"未复核 | 01 所有权行 |
| ti.com/partner/ja-jp/VCTR、iar.com/about/partners/vector | 可打开，合作关系成立；细节未逐条核对 | 无 |

## 三、Renesas MCAL（03、01）

| URL | 结果 | 修改 |
|---|---|---|
| renesas.com/en/software-tool/renesas-mcal (R1) | [确认] SPAL(ADC/DIO/FLS/GPT/ICU/MCU/PORT/PWM/SPI/WDG)、COM(CAN/LIN/FR/ETH)、TEST(Core/Ram/Flash)，AR4.2.2；"free of charge and as is"；[更正] 社区讨论只是用户提问 DaVinci 还是 EB tresos，非"提及为可选工具"的背书；示例视频针对 RH850/X1x | 03、05 |
| renesas.com/cn/en/application/automotive/autosar (R3 新增) | [确认] production ready MCAL、3.x/4.x、Safety MCU 开发过程符合 ISO26262、Premium 成员自 2004-07、"close cooperation with all major 3rd party AUTOSAR partners"；新增事实：支持列表中 F1K/F1KM/F1KH/E2M/C1M-A 列在 4.2.2，P1M 等列在 4.0.3 | 01/03/04/README 加入，并与仓库"P1M AR4.2.2 API"并列为待确认 |
| renesas.com .../tata-elxsi-mcal-support-activities-rh850f1kx-2020 | [降级] 二进制，仅确认 URL 可达 | 01 已如此标注 |
| eejournal IAR RH850/U2A MCAL (R2) | [确认] 2025-09-04，MCAL 基于 R22-11，ASIL D MP，IAR v2.21.2 FS | 03 |
| renesas.com green-hills-multi-device-support-packages、ghs.com RH850、AbsInt targets | 页面可打开；GHS RH850 开发页确认；AbsInt 页文本无 RH850 字样（脚本解析限制，未确认） | 01 §4.1 AbsInt 句保留但视为未复核 |
| hitex.com MCAL (I2) | 可打开，仅导航文本，细节未核对 | 无 |

## 四、Elektrobit / EB 所有权与 AutoCore（01、03、05）

| URL | 结果 | 修改 |
|---|---|---|
| globenewswire 2015-05-19（WebFetch 补查） | [确认] 买方 Continental AG，6 亿欧元现金，预计 2015 年 7 月初交割 | 01 补充 |
| continental.com/en/investors/events/spin-off-automotive | [确认] 2025-09-18 AUMOVIO 独立上市；未提 EB | 01 |
| autosar.org core-partner | [确认] Core 列表含 "AUMOVIO (formerly Continental)" | 01 |
| WebSearch EB 归属（两次） | 仅搜索摘要称"截至 2026 年初 EB 仍属 Continental"；无官方原文 | 01/README 保持"未经官方核实" |
| elektrobit.com/?p=45325（Nissan 案例/产品线） | [确认] Nissan 推荐 vendors，EB 预集成 Nissan Extension Module、作为 ECU startup package；EB 评估包含 RH850/F1KM | 02 无改；03 新增评估包事实 |
| elektrobit.com/?p=27049（HL Mando） | [确认] global concept license；[降级] "prototype project license" 措辞未见 | 02 §5.1 |
| elektrobit.com/?p=1807 (B2) | [降级] 页面只见产品/网络研讨会列表，导入器清单与"引导式工作流"未找到 | 03/05 标仅搜索摘要 |
| software-dl.ti.com .../MCAL_Configuration_and_EB_Tresos (B3) | [确认] MCAL 以 EB tresos plugin 交付、xdm 格式、Im-/Exporters、ARXML 4.3.1 导出、AutoCalc | 无 |
| infineon.com AUTOSAR 页 (I1) | [降级] curl 只得导航文本，AS422/AS440/TC4x R20-11 等未核对 | 03/05 标仅搜索摘要 |
| mathworks 连接页（B1/E1/M1/KPIT） | 403，无法核对 | 03/05/01 标仅搜索摘要；E1 改引 ASAM |
| siemens blog 2020 (short history)、Siemens Capital VSTAR Fact Sheet | [确认] Volcano 2005、Mecel JV（4.x 栈 2011 起）、2014 收购技术、2017 完成 Mentor 收购 | 无 |

## 五、OEM 案例（02）

| URL | 结果 | 修改 |
|---|---|---|
| autotechinsight .../5236990 Toyota–Mentor；cimdata item 7297 | [确认] CIMdata 日期 2016-11-30；S&P 发布 2016-12-02 | 02 加注 |
| vector.com .../toyota-selects-vector... | [确认] 2017-05-18，recommended AUTOSAR BSW vendor，Next Communication Stack；2016 首个 ASIL-D 认证；[更正] 原写"产品当时已有 ASIL-D 认证"改为页面措辞 | 02 |
| blogs.sw.siemens.com 2022 "How standard is a standard" | [更正] 原写"OEM 4.0→4.1→4.2→4.3 逐步迁移"；原文是"许多用 4.0、少数 4.1、更多 4.2，不少长期停留 4.0，4.3 被视为大步" | 02 |
| autotechinsight .../5249955 Tata Elxsi–Great Wall | [确认] Adaptive；页面日期 2019-05-23（原写 05-22） | 02 |
| gasgoo ×4、automotiveworld ×2、designnews、businesswire、mobilityengineering、computer-automation | 403/失败，仅搜索摘要 | 02/01 标注；MB.OS、AUTOSEMO、AutoChips-EB 等保持摘要级 |
| trendingtopics CARIAD；cariad.technology | [确认] CARIAD 开发 VW.OS、"bracket"；2022-06-22 加入 Eclipse SDV | 01/02 |
| volkswagengroup.it VW.os | 404 | 01/02 删除链接，"system of systems"降级 |

## 六、AUTOSAR 伙伴与标准事实（01）

| URL | 结果 | 修改 |
|---|---|---|
| autosar.org/about/partners | [确认] 350+ partners；费用表：PPP 90,000 EUR + 5 FTE + 1 FTE；Premium 31,000 + 1.5 FTE；Development 10,000 + 0.5 FTE（<100 人）；Associate 21,000；Subscriber 3,000；脚注可用补偿费代替 FTE。原"来自搜索摘要、证书错误"的说明过时（curl 可直接打开） | 01 |
| autosar.org core-partner | [确认] 2003-07 六家创始方；Toyota 2003-12；GM 次年 11 月；2026-01 DENSO/Huawei/Vector；现 10 家。[删除] "Ford 2003-11、PSA"（无来源） | 01 |
| .../welcome-our-three-new-core-partners | [确认] 日期 15.01.2026 | 01 |
| .../autosar-chat-episode-4... | [降级] 区域伙伴数 41/140/174/5 与"2022 China Center"均未找到（页面仅有 China Hub 与参与度表述） | 01 降级/删除 |
| Newsletter_Q1_2026_External.pdf | 404 | 01 删除链接 |
| autosar.org premium-partner | [确认] 列有 EB、ETAS、Infineon、iSOFT、Neusoft Reach 等 | 01 |
| R25-11 Release Overview PDF；release-event | PDF 为二进制；release-event 页 [确认] R25-11/R24-11 与 DDS Support on CP、Rust in CP Outlook、R26-11 活动 3 Dec；具体日期 2025-11-27 未能核对 | 无（02/01 仍引用该日期，标为未直接核对） |
| iSOFT 成功案例页 | [确认] 2010 年加入 AUTOSAR、中国首家 Premium、TÜV Rheinland ASIL D | 无 |
| hpmicro 新闻 | [确认] 2024-05-14，INTEWORK-EAS 适配 HPM6200，兼容 DBC/LDF/PDX/ODX/ARXML | 01 |
| autotechinsight .../5290256 IAR–Neusoft | [确认] NeuSAR OS 与 IAR 工具合作（2026-08-26） | 无 |
| tataelxsi AUTOSAR services | [确认] MCAL/BSW/Adaptive 服务 | 无 |
| fptsoftware（WebFetch） | [确认] 提及 AUTOSAR（MaaZ PRO） | 无 |
| projects.eclipse.org/node/30204 与 openbsw 页、github、mailing list | [确认] Apache 2.0、Incubating、Eclipse SDV；POSIX、S32K148、UDS/DoCAN、FreeRTOS；[更正] 项目负责人为 Matthias Kessler 与 Martin Thiede，版权归 Accenture，原"Accenture 的 Martin Thiede 提出"改写；STM32 未复核 | 01 |
| epdtonthenet S-CORE | [确认] 2024 年底启动；支持方 BMW、Mercedes、Bosch、QNX、Accenture、ETAS、Qorix；[更正] 原写"由 Accenture、BMW、ETAS、Mercedes、Qorix 发起" | 01 |
| vda.de 2026 | [确认] Berlin 2026-01-06 Eclipse–VDA 扩大合作 | 01 |

## 七、跨报告一致性与链接、Mermaid

- RTA-CAR 组件口径统一为："发行包（V12.3.0 条目）= ISOLAR-A、ISOLAR-B、RTA-BSW、RTA-RTE、RTA-OS；ASAM 另列 RTA-FBL"。已改 01（ETAS 行）、03 §1.2、04 §1/结论、05 工具表与映射表、06 §1。
- 所有权口径统一：ETAS=Bosch 100% 子公司；EB=2015 年 Continental 收购（6 亿欧元），AUMOVIO 分拆后归属未经官方核实；Vector=基金会持股独立公司。见 01、README；02/03/05 无冲突表述。
- 术语：SIP 仅指 Vector 的 Software Integration Package Based Delivery（03 §1.1、05 术语说明）；BSWMD、ECU Extract、ECUC 与 11/02 §5 一致。
- 与 11/02：02 篇新增模式对应说明（a=A，c=B，d=C）。无矛盾。
- 相对链接：全部 6 篇的相对链接与 `examples/uds_diag_demo/tests/test_uds_demo.c` 引用均存在，无需修复；dcm-upgrade-guide.md §2/§3/§6/§10 章节存在。
- Mermaid：`node check.mjs docs/12-industry-ecosystem` 结果 blocks=6 bad=0（未改动图）。

## 八、仍未解决

- EB 在 AUMOVIO 分拆后的官方归属；Vector 持股比例原文；R25-11 发布日 2025-11-27 的原页核对。
- Vector SIP 逐项内容、Renesas MCAL 登录区清单、ETAS 手册类文档（需登录）。
- 403 来源（MathWorks、Gasgoo、Automotive World、Design News、Business Wire、ST）的细节，未能原页核对。
