# 11 市场调研（docs/13-market-research）核查日志

- 核查日期：2026-10-02。对象：[`docs/13-market-research/`](../../13-market-research/README.md) 01–05。
- 方法：对关键论断打开原页面（WebFetch / curl）或用搜索交叉验证；403/超时的页面只记录“未能打开”，不当作已核实。
- 结果标记：确认 = 保留；更正 = 改数字/表述；降级 = 改为“仅见搜索摘要/未核实/推断”；删除 = 与来源矛盾或无支撑。

## 汇总

打开/交叉验证的 URL 约 56 个；核查论断约 44 条：确认 22、更正 14、降级 6、删除 4（删除项均已在正文说明）。

## 市场数字

| URL | 结果 | 修改 |
|---|---|---|
| edge-ai-vision（Yole MCU，2025-12） | 确认：2030 年 340 亿、CAGR 6%；车用 130 亿、CAGR 3%；2024 前四约 70%；TinyML 2028 年 ≥10% | 无 |
| edge-ai-vision ?p=54938（Yole 车用半导体） | 确认：680→1320 亿美元；前五约一半 | 加“已核对” |
| heise 11247952（Omdia 2025） | 确认：约 221 亿、-0.3%、Infineon 23.2%、前五约 80% | 无 |
| goldmansachs.com/insights/articles/…38-billion…（2024-02-27） | 确认为旧版：2035 TAM 380 亿、约 140 万台 | 04 改为“旧版”，链接换为 insights 路径 |
| humanoid.guide GS 上调；Yahoo Finance GS；24/7 Wall St. 与 GuruFocus（搜索结果） | 确认（二手转述，2026-09-14/15）：2035 年约 650 万台（旧 138/140 万）、约 1380 亿美元（旧 380 亿）；2030 年 89 万（旧 25.6 万）；2026 年 7.5 万（旧 5.1 万） | 04、05 改写；两版并列，均标 [分析师预测] 与年份；GS 新版原文未能直接打开 |
| goldmansachs.com …ai-accelerant | 403 | 05 删除该链接（未核实） |
| technode.com（Omdia 2026-01） | 更正：Unitree 为约 4,200（5,500 为 Unitree 自报）；总量约 1.3 万、AgiBot 5,168（39%）、UBTech 约 1,000 确认；文中无 Morgan Stanley | 04 更正；删除“MS 2026 年出货 5 万台” |
| developer.tenten.co（Morgan Stanley） | 确认：2050 年硬件年收入约 4.7 万亿、约 10 亿台、中国 3.02 亿台、2035 约 1300 万台；“含软件约 5 万亿”仅见标题 | 04 改写并保留二手标注；05 删除未复核的“2035 年超 2500 亿” |
| meticulousresearch.com 区域控制器 | 更正：页面现为 2036 年 218 亿、CAGR 21.4%、2026 基数 31.2 亿（原稿 179 亿/12.8%） | 01 更正并注明不稳定 |
| psmarketresearch.com SDV | 更正：页面现为 2025 年 3150 亿 → 2032 年 5748 亿、CAGR 9.0%（原稿 2917→4897 亿） | 05 更正 |
| mckinsey.com（2024 版） | 超时；搜索摘要显示“through 2030”版为 4690 亿（2020–2030 CAGR 约 7%），与原稿 4620 亿/5.5% 不一致 | 05 降级，并写明以原文为准 |
| dri.co.jp（QYResearch 2023） | 确认：前五合计 88.43%；顺序 Infineon/NXP/Renesas/Microchip/ST；另有 2023 市场 118.6 亿 | 02 补口径说明 |
| counterpointresearch、TechInsights、trendforce、集微网 jw.ijiwei.com/n/871460 等 | 未打开，保持“仅搜索摘要”标签 | 无 |

## 厂商与路线图

| URL | 结果 | 修改 |
|---|---|---|
| renesas.com 路线图（2023-11） | 确认：Arm 32 位 crossover R-Car MCU（带 NVM）与 R-Car MCU 车身/动力系列；2024 年起陆续发布 | 无；另搜索未发现具体量产型号，维持“公开资料未给出” |
| renesas.com RH850/U2C 新闻稿 | 确认：2026-03-04、4 核 320 MHz 含 2 lockstep、8 MB、28 nm、ASIL D、PQC、CAN-XL/10BASE-T1S/TSN/I3C、自 2013 年累计 40 亿颗 | 无 |
| tasking.com AURIX DRIVECORE TC4 IT2 | 确认：Infineon + Intron + Vector + TASKING，TC4D9，面向 ADAS/自动驾驶/机器人，预集成 MICROSAR Classic，无发布日期 | 04 补充 |
| 搜索（eeNews/Infineon DRIVECORE 2025-03） | 确认：AURIX DRIVECORE AUTOSAR 捆绑含 Infineon MCAL + MICROSAR Classic | 02 补充 |
| infineon.com TC4Dx 页面 | 页面不含规格；eeNews：2024-11-06、28 nm、6 个 TriCore、500 MHz、送样、2025 量产；未明确 NVM 类型 | 01/02 更正并标注 |
| Infineon 2022 RRAM（搜索摘要） | 确认：2022-11-25、TSMC RRAM、首批样品 2023 年底 | 01 补充 |
| new-origin.infineon.com RISC-V（2025-03） | 连接被拒，未打开；eejournal 2026-03-04 确认 RISC-V Virtual Prototype 捆绑包；样品 2026/量产约 2028 仅见德文媒体摘要 | 01/02 补充并降级 |
| NXP S32K5（搜索摘要，nasdaq 超时） | 确认：2025-03-11、16 nm FinFET + MRAM、Arm 至 800 MHz；“写入快 15 倍”未复核 | 01 降级 |
| powersystemsdesign Stellar P6 | 确认：最多 6 个 R52、20 MB PCM、28 nm FD-SOI、CAN XL、无停机 OTA | 无 |
| Quintauris（Wikipedia/搜索摘要） | 确认：2023-12-22 成立；ST 2024-08 成第六股东 | 01 补充 |
| NIST FIPS 203/204/205 | 确认：2024-08-13 批准 | 无 |
| CAN XL / ISO 11898-1:2024（搜索） | 确认：2024-05 采用，数据域 2048 字节；10 Mbit/s 未在该摘要中出现 | 保留（CiA 页面重定向未打开） |
| autosar.org R25-11（搜索） | 确认：2025-12-04、800+ 参与者、250+ 公司、Classic 上 VDP 与 DDS | 01 补充 |

## 软件平台与开源

| URL | 结果 | 修改 |
|---|---|---|
| eenewseurope real-time-officially-comes-to-linux；phoronix（搜索） | 更正：合入主线 2024-09-20；6.12 于 2024-11-17 发布（原稿 11-19）；x86/ARM64/RISC-V | 03 更正 |
| openrobotics.org Kilted Kaiju 发布博客 | 确认：2025-05-23 发布；首个把 Zenoh 作为 Tier 1 RMW 的发行版 | 无；“是否默认 RMW”仍未验证 |
| docs.zephyrproject.org 安全概述；inovex（Embedded World 2026） | 确认：IEC 61508 Route 3S、目标 SIL 3、约 15,000 行范围、SEooC；ISO 26262 仅为后续计划；文档页本身不提 26262 | 01/05 补充；统一为“均未完成认证” |
| raw.githubusercontent eclipse-openbsw README；projects.eclipse.org OpenBSW | 确认：版权头 Accenture 2024；Incubating、Apache-2.0；搜索摘要称初始贡献由 ESR Labs（Accenture 旗下）主导 | 01/05 写明 Accenture/ESR Labs 源头，从缺口中删除 |
| newsroom.eclipse.org/node/42222（S-CORE） | 确认：参与者 BMW、Mercedes-Benz、Bosch、ETAS、QNX、Qorix、Accenture；0.5 目标 2025-10；开发流程接受认证机构审核；参考平台 QNX SDP 8.0 | 05 更正名单并注明来源间不一致 |
| cnx-software OpenAMP | 确认：RemoteProc + VirtIO/rpmsg 共享内存 | 无 |
| bostondynamics.com NVIDIA 合作 | 确认：Atlas 采用 Jetson Thor；协作定义功能安全与安全架构 | 03 补充 |
| portal.fis.tum.de EtherCAT 人形 | 确认：>2 kHz、<1 ms | 无 |
| threadx.io FAQ | 重定向未打开 | 保留“二手来源”标签 |

## 标准

| URL | 结果 | 修改 |
|---|---|---|
| iso.org/standard/91469.html | 403；搜索结果标题显示“Committee Draft”（ISO/CD 25785-1） | 03/05 统一为 ISO/CD |
| i-scoop ISO 25785-1 | 确认：工作草案 2025-05、美国代表团（A3、Agility、Boston Dynamics）牵头、预计 DIS 2026/发布 2026 末或 2027、跌倒区约 2 m 示例 | 无 |
| ISO 10218（iso.org 403；搜索） | 确认：-1/-2:2025 于 2025-02 发布 | 无 |

## 机器人与车企

| URL | 结果 | 修改 |
|---|---|---|
| humanoid.guide Hyundai；Axios/eWeek（搜索摘要） | 更正：2028 年起年产约 30,000 台 Atlas（25,000+ 自用）；2028 佐治亚 Metaplant、2029 Kia 佐治亚；美国年产 30 万+ 执行器；Mobis 未被该页面提及 | 05 更正，删除“十年内 3 万台”，Mobis 降级 |
| automotiveworld Hyundai 完全控股 BD | 403，仅见标题 | 05 降级 |
| humanoid.guide ?p=17841；interestingengineering（XPeng） | 确认：2026-09-08 广州产线、80%+ 自动化、年底量产、2027 交付；年产百万台为媒体转述（2030），IE 页面不含 | 04/05 更正；删除截断的 Electrek URL |
| finviz（Xiaomi） | 更正：2026-03-02 微博，压铸车间放螺母 90.2%、3 小时；未出现 CyberOne | 04/05 更正 |
| getcoai（XPeng/Xiaomi/18 家车企/70%） | 页面仅支持 Iron 在 P7+ 产线；70%、18 家、CyberOne 无支撑 | 04 降级为未核实 |

## 中国厂商

| URL | 结果 | 修改 |
|---|---|---|
| Gasgoo AutoChips（403，经搜索摘要） | 确认：AC7870x 为 6 个 R52、360 MHz、TÜV 莱茵 ASIL-D，2026 年（electronica China 2026 报道）；原稿“2026-05”未复核 | 01/02 改为“2026 年” |
| Gasgoo / 搜索 SemiDrive E3 | 确认：Cortex-R5F、ASIL D、百万级出货、40+ 车型；最高 800 MHz、6 核 | 02 补充 |
| 搜索 GigaDevice | 更正：Frost & Sullivan 2024 全球 MCU 第八；全部产品 43.6 亿颗；GD32A 累计 >800 万；“24 亿颗/营收第三”无支撑 | 02 删除 |
| 搜索 国芯 CCFC3009PT | 更正：多核 RISC-V、500 MHz、约 10,500 DMIPS、22 nm RRAM、流片试产（原稿 6000+ DMIPS/开发中） | 02 更正 |
| jw.ijiwei.com/n/846103（ChipON 等） | 502 | 02 芯旺微降级为仅见搜索摘要 |

## 跨报告一致性与链接、Mermaid

- 统一项：PREEMPT_RT（2024-09-20 合入、6.12 提供）、ISO 25785-1（ISO/CD）、Zephyr（IEC 61508 概念批准、均未完成认证）、OpenBSW 源头、Quintauris 成员、GS 预测（新旧两版并列）。
- “01/02/03/04 号报告”类引用已全部改为指向实际文件名的相对链接（03、04、05）。
- 全部相对链接（含 `../11-classic-autosar-primer/`、`../02-autosar-classic/`、`../04-can-mcal/`、`../10-boot-debug/`、`../12-industry-ecosystem/`）均能解析。
- Mermaid：`node check.mjs docs/13-market-research` 结果见最终报告（修改后 bad=0）。

## 仍未解决

GS 新版原文、McKinsey 与 Counterpoint/TechInsights 原文、ISO/Gasgoo/Axios 页面（403）、Infineon RISC-V 官方样品与量产日期、NXP“15 倍写入”、芯旺微 KF32A158、中国国产化率口径。
