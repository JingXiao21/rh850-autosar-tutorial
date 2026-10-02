# 机器人 MCU 硬件、功能安全标准与市场趋势

## 报告头

- **本报告回答的问题**:机器人(工业臂、AMR/AGV、人形、四足)用什么 MCU/SoC?车规 MCU 和 AUTOSAR 是否用于机器人?适用哪些安全标准?市场预测与汽车行业的趋同如何?
- **调研日期**:2026-10(来源访问日期均为 2026-10)
- **主要来源类型**:芯片厂商页面(**多为营销材料,已标注**)、标准组织页面、投行/研究机构预测(经媒体转述)、行业新闻
- **证据标签**:[事实/有来源] [多来源一致] [分析师预测]（注明机构与年份）[推断];[来源缺口] 表示未找到
- 与 [03-robotics-realtime-software-architecture.md](03-robotics-realtime-software-architecture.md)(软件架构)配套;汽车 MCU 趋势见 [01](01-automotive-mcu-trends.md)、[02](02-mcu-vendor-landscape-and-roadmaps.md),不重复。

---

## 1. 机器人里的 MCU / SoC 选择

### 1.1 按层次汇总(均来自厂商公开页面,属于**厂商营销**性质,反映"厂商希望被选用"而非实际市占)

| 厂商/器件 | 公开信息 | 来源 |
|---|---|---|
| TI C2000 TMS320F28P55x | 150 MHz C28x DSP(厂商称相当于 300 MHz Cortex-M7 的实时信号链性能),带 CLA 与集成 NPU(厂商称用于低延迟故障检测);TI 定位为适合机器人关节电机控制 | [TI 数据手册](https://www.ti.com/lit/pdf/sprsp85), [Mouser 介绍](https://www.mouser.in/new/texas-instruments/ti-tms320f28p55x-real-time-mcus/) [事实/有来源,厂商营销] |
| ST STM32G4 / H5 / H7 / C5 / N6;STSPIN32G4 | ST 应用笔记给出人形灵巧手的电机控制架构,从 6 DoF 到 16 DoF 传感器丰富平台;STSPIN32G4 为双 FOC 驱动,用于机器人手(厂商称 150 W、4x4.5 cm) | [ST AN6570](https://www.st.com/resource/en/application_note/an6570-motor-control-architectures-for-dexterous-humanoid-robot-hands-stmicroelectronics.pdf), [ST 关节页](https://www.st.com/en/applications/robotics/articulations-legs-and-arms.html) [事实/有来源,厂商营销] |
| NXP MCX E / MCX A / i.MX RT1180 / i.MX 94 | NXP 人形与移动机器人运动控制参考设计:i.MX RT1180 构成运动链,支持 CAN-FD、EtherCAT、TSN,功能安全导向设计,支持 BiSS-C 等编码器 | [NXP 参考设计页](https://www.nxp.com/design/design-center/development-boards-and-designs/motor-and-motion-control-for-humanoids-and-mobile-robots:MOBILE-ROBOTICS-MOTION-CONTROL-HOLOSCAN-SENSOR) [事实/有来源,厂商营销] |
| Renesas RZ/T2M、RZ/T2L | RZ/T2M:双 Cortex-R52 最高 800 MHz,3 端口千兆以太网交换机含 TSN,支持 EtherCAT/PROFINET/EtherNet/IP;宣称电机电流环 <1 us,多种编码器接口;RZ/T2L 为单 R52 成本优化,面向伺服、工业机器人、协作机器人 | [Renesas RZ/T2M](https://www.renesas.com/sg/en/products/microcontrollers-microprocessors/rz-mpus/rzt2m-high-performance-multi-function-mpu-realizing-high-speed-processing-and-high-precision-control), [RZ/T2L](https://www.renesas.com/en/products/microcontrollers-microprocessors/rz-mpus/rzt2l-high-performance-mpu-realizing-high-speed-and-high-precision-real-time-control-ethercat) [事实/有来源,厂商营销] |
| Infineon PSOC / AURIX | Infineon 称其微控制器提供多核实时处理与安全,用于人形;并与 NVIDIA Jetson Thor 联合推广 | [Barchart 转载新闻稿](https://www.barchart.com/story/news/34371148/infineon-to-enable-humanoid-robots-with-precise-motion-and-efficiency-powered-by-nvidia-technology), [Infineon 网络研讨会标题](https://www.infineon.com/ja/event/webinar/2026/automotive-mcus-the-right-solution-for-humanoids) [厂商营销;网络研讨会正文未能取得] |
| NVIDIA Jetson AGX Thor | 14 核 Arm Neoverse-V3AE CPU、128 GB LPDDR5X、最高 2070 FP4 TFLOPS、40-130 W;称为 AGX Orin 的 7.5 倍 AI 性能(厂商宣称) | [JetsonHacks](https://jetsonhacks.com/2025/09/21/nvidia-jetson-agx-thor-the-new-king-of-embedded/amp/), [Peridio](https://docs.peridio.com/hardware/under-evaluation/jetson-agx-thor) [多来源一致,数值源自厂商] |

**NVIDIA Orin、Jetson 之外的 AMD/Qualcomm 等**:未调研。
**哪家 MCU 在哪款具体机器人中:**公开拆解证据极少,**未取得**可靠的"某机器人用某 MCU"来源。[来源缺口] 不要把上表理解为市占率。

### 1.2 按机器人类型
- **工业臂/协作臂**:伺服驱动 + EtherCAT 为主流,厂商以 RZ/T2x 等伺服 SoC 为目标 [Renesas 页面,厂商营销];各品牌臂内部具体芯片未取得来源 [来源缺口]。
- **AMR/AGV**:安全标准为 ISO 3691-4(见第 3 节);具体 MCU 选型未取得来源 [来源缺口]。
- **人形/四足**:计算侧 Atlas、Digit 使用 Jetson Thor [Agility](https://agilityrobotics.com/content/agility-robotics-powering-the-future-of-robotics-with-nvidia-jetson-thor), [Boston Dynamics](https://bostondynamics.com/news/boston-dynamics-expands-collaboration-with-nvidia/) [事实/有来源];Boston Dynamics 与 NVIDIA 合作定义"功能安全与安全架构"。关节侧见下一节。

---

## 2. 人形关节架构趋势

- **Tesla Optimus**:公开资料称有 28 个结构执行器,整合为 6 种设计(旋转 20/110/180 Nm,带集成位置与力矩传感器;线性 500/3900/8000 N);单手 6 个执行器、11 DoF [NotATeslaApp](https://www.notateslaapp.com/news/1000/everything-we-know-about-optimus-the-tesla-robot), [Robotics 24/7](https://www.robotics247.com/article/tesla_shares_more_details_about_optimus_humanoid_robot) [多来源一致,数据来自 Tesla 公开演示的二次报道]。(不含手部总数为 28,手部另计;原文表述有版本差异,以 Tesla 原始发布为准。)
- **Unitree G1**:膝关节执行器约 1 kg,含 18 槽 16 极电机、22.5:1 两级行星减速器、双编码器、驱动电路;连接器 XT30 2+2,两根大针传 DC 母线,两根小针传差分数据 [China-AMASS 拆解报道](https://www.china-amass.net/news/deep-dismantling-of-unitree-g1-knee-joint-actuator-full-analysis-of-120-n%c2%b7m-torque-twostage-planetary-reduction-and-dual-encoders/) [事实/有来源,单一来源]。**说明:关节集成驱动电路,经"电源+差分数据"同一连接器,典型为分布式关节控制器 + 总线** [推断]。具体是 CAN 还是 RS485/EtherCAT,该来源未明确。[来源缺口]
- **通信总线趋势**:人形系统通信接口通常是 CAN-FD 或基于以太网(含 EtherCAT)[NXP 页面,厂商营销];EtherCAT 学术人形架构 >2 kHz、I/O 延迟 <1 ms [TUM](https://portal.fis.tum.de/en/publications/an-ethercat-based-real-time-control-system-architecture-for-human/) [事实/有来源]。
- **Figure、UBTech、Agility 的关节驱动器与总线细节**:未取得公开拆解或分析师报告。[来源缺口]
- **趋势归纳** [推断]:关节"电机+减速器+编码器+驱动器"一体化,每个关节一个 MCU/驱动 SoC,经 CAN FD/EtherCAT 菊花链连接中央计算。这与整车"区域控制器 + 传感器执行器节点"相似。

---

## 3. 功能安全与标准

| 标准 | 要点 | 来源 |
|---|---|---|
| IEC 61508 | 通用功能安全(SIL);RTOS 认证常用(SafeRTOS SIL 3、QNX SIL 3、ThreadX SIL 4 声明、Zephyr 目标 SIL 3) | 见下文 RTOS 表 |
| ISO 13849-1 | 安全相关控制系统可靠性(PL);ISO 3691-4 引用它;ISO 3691-4 还引用 IEC 61496(激光扫描仪等) | [Fabrico 解读](https://www.fabrico.io/blog/iso-3691-4-driverless-industrial-trucks/) [二手来源] |
| ISO 10218-1:2025 / -2:2025 | 工业机器人安全,2025 年 2 月发布;-1 面向机器人本体(作为部分完成机器),-2 面向集成与应用;不适用于服务机器人、医疗等 | [ISO 10218-1 页面](https://www.iso.org/standard/73933.html), [EVS](https://www.evs.ee/en/evs-en-iso-10218-1-2025) [多来源一致]。2025 版相对旧版的具体技术变化:[来源缺口] |
| ISO/TS 15066 | 协作机器人接触力限值;ISO 25785-1 草案引用其生物力学数据 | [i-SCOOP](https://www.i-scoop.eu/iso-25785-1-explained-and-what-it-means-for-humanoid-robot-safety/) [单一来源,间接]。TS 15066 原文未直接核实。 |
| ISO 3691-4:2023 | 无人驾驶工业车辆及系统(AGV/AMR),规定检测人员、速度控制、与人共享空间时的行为 | [Fabrico](https://www.fabrico.io/blog/iso-3691-4-driverless-industrial-trucks/), [Genorma](https://genorma.com/en/standards/iso-3691-4-2023/amp) [多来源一致] |
| ISO 13482 | 个人护理机器人(非工业场景) | [i-SCOOP](https://www.i-scoop.eu/iso-25785-1-explained-and-what-it-means-for-humanoid-robot-safety/) [间接,单一来源] |
| ISO 25785-1(已核实状态) | 主动平衡工业移动机器人(腿式、轮式等)的安全要求;ISO 页面显示为 Committee Draft/工作草案阶段;工作草案于 2025 年 5 月出现;美国代表团(含 A3、Agility Robotics、Boston Dynamics)主导;i-SCOOP 称截至 2026 年初仍为工作草案,业界预计 2026 年底或 2027 年发布 | [ISO 页面](https://www.iso.org/standard/91469.html), [i-SCOOP](https://www.i-scoop.eu/iso-25785-1-explained-and-what-it-means-for-humanoid-robot-safety/) [事实/有来源;发布时间为二手估计,2026-10 当前最新状态请查 ISO 页面] |

### 认证 RTOS / 软件(与 [03](03-robotics-realtime-software-architecture.md) 一致)
| 项 | 状态 | 来源 |
|---|---|---|
| SafeRTOS | IEC 61508 SIL 3 预认证(TÜV SÜD) | [FreeRTOS.org](https://FreeRTOS.org/FreeRTOS-Plus/Safety_Critical_Certified/SafeRTOS.html) |
| QNX OS for Safety | IEC 61508 SIL 3、ISO 26262 ASIL D(TÜV Rheinland);QNX 称与 NVIDIA IGX Thor 组合预认证到 SIL 3/ASIL D(厂商营销) | [QNX 产品简介](https://qnx.software/content/dam/qnx-xwalk/pdf/product-briefs/qnx-os-for-safety-product-brief.pdf), [QNX-NVIDIA](https://qnx.software/en/partner/partner-program/nvidia-igx-thor) |
| Eclipse ThreadX | 历史上获 IEC 61508 SIL 4、ISO 26262 ASIL D 预认证等(二手来源);安全工件由 Eclipse ThreadX 工作组维护 | [Wikipedia](https://en.wikipedia.org/wiki/ThreadX) |
| Zephyr | 2024 年获 IEC 61508 概念批准,目标 SIL 3,Route 3S,尚未完成认证 | [Zephyr 文档](https://docs.zephyrproject.org/latest/safety/safety_overview.html) |

### 驱动级安全功能
- STO/SS1 等遵循 IEC 61800-5-2 [Motion Control Tips](https://www.motioncontroltips.com/faq-what-are-typical-drive-based-safety-functions)。人形/四足"断电即停"不成立,因此 ISO 25785-1 涉及受控下降、跌倒区(约 2 m,1.7 m 高、1.5 m/s 示例)[i-SCOOP](https://www.i-scoop.eu/iso-25785-1-explained-and-what-it-means-for-humanoid-robot-safety/) [单一来源]。

---

## 4. 车规 MCU / AUTOSAR 是否用于机器人?

- **有公开线索,但属于厂商推广,不等于大规模量产**:
  - Infineon 举办"车规 MCU 为何最适合人形"的网络研讨会并称 AURIX/PSOC 适合人形(厂商营销,正文未取得)[Infineon](https://www.infineon.com/ja/event/webinar/2026/automotive-mcus-the-right-solution-for-humanoids)。
  - **AURIX DRIVECORE TC4 IT2**(Infineon + Intron + Vector + TASKING;基于 TC4D9 与 Intron OPEN-ECU 硬件):TASKING 页面明确称面向 ADAS、自动驾驶与**机器人**开发,预集成 **Vector MICROSAR Classic(AUTOSAR)**(页面已核对,无发布日期) [TASKING](https://www.tasking.com/aurix-drivecore-tc4-it2-infineon-intronvector-tasking/) [事实/有来源,厂商营销]。这是少数把 Classic AUTOSAR 栈与机器人并提的公开例证。
  - Infineon 收购 Marvell 汽车以太网业务,并推出 BRIGHTLANE 系列,称其为人形的以太网核心 [eeNews Europe](https://www.eenewseurope.com/en/infineon-looks-to-humanoid-robots-with-marvell-ethernet-buy/) [事实/有来源标题,正文未读]。
- **未找到**:某具体人形/AMR 量产机型使用 RH850 或 Classic AUTOSAR 的公开证据。[来源缺口]
- 结论 [推断]:机器人关节侧更多使用 TI C2000/ST STM32/NXP/Renesas RZ 等"工业/通用 MCU",车规器件与 AUTOSAR 主要作为"可选高安全路线"被厂商推广。

---

## 5. 市场趋势

| 机构/来源 | 预测或数据 | 标签 |
|---|---|---|
| Goldman Sachs Research(2024-02-27 发布,Jacqueline Du)——**旧版,已被上调** | 2035 年人形机器人 TAM 约 380 亿美元(此前预测 60 亿美元),出货约 140 万台;BOM 成本区间由 5 万-25 万美元降至 3 万-15 万美元(GS 原页面已打开核对) | [分析师预测,Goldman Sachs 2024] [Goldman Sachs](https://www.goldmansachs.com/insights/articles/the-global-market-for-robots-could-reach-38-billion-by-2035) |
| Goldman Sachs Research(2026-09-14 前后,**最新公开版本,经媒体转述**) | 2035 年出货约 650 万台(旧 138 万/140 万)、市场约 1380 亿美元(旧 380 亿美元);2030 年出货 89 万台(旧 25.6 万);2026 年 7.5 万台(旧 5.1 万);每台人形含 3,000-6,000+ 美元半导体 | [分析师预测,Goldman Sachs 2026-09,经 humanoid.guide / Yahoo Finance 转述;GS 新版原文页面未能打开,未直接核对,以 GS 官网为准] [humanoid.guide](https://humanoid.guide/goldman-raises-2035-humanoid-robot-forecast-to-6-5-million/)、[Yahoo Finance](https://finance.yahoo.com/technology/ai/articles/goldman-sachs-just-supercharged-humanoid-135551371.html) |
| Morgan Stanley(Global Humanoid Model,2025,经媒体转述) | 2050 年人形硬件年收入约 4.7 万亿美元(转述文标题称含软件约 5 万亿美元,未核对原报告);2050 年约 10 亿台,其中中国约 3.02 亿台(约 30%)、美国约 7,770 万台;2035 年约 1300 万台(工商业场景);工商业占 90%(Tenten 转述已核对)。原稿“2026 年出货 5 万台”的出处(TechNode)正文并未提及 Morgan Stanley,已删除 | [分析师预测,Morgan Stanley 2025] [Tenten 转述](https://developer.tenten.co/morgan-stanleys-bold-prediction-humanoid-robots-will-create-a-5-trillion-market-by-2050), [TechNode/Omdia 报道](https://technode.com/2026/01/09/chinas-agibot-leads-global-humanoid-robot-shipments-in-2025-omdia-says/);均为二手转述,未核对原报告 |
| Omdia(2026-01) | 2025 年全球人形出货约 13,000 台;AgiBot 5,168 台(39%),Unitree 约 4,200 台(TechNode 转述 Omdia 数字;Unitree 自报超 5,500 台,口径不同),UBTech 约 1,000 台(TechNode 已核对) | [分析师预测/事实] [TechNode](https://technode.com/2026/01/09/chinas-agibot-leads-global-humanoid-robot-shipments-in-2025-omdia-says/), [Jiemian](https://en.jiemian.com/article/13924474.html), [SCMP](https://scmp.com/tech/tech-trends/article/3340446/chinas-unitree-ships-more-5500-humanoid-robots-2025-surpassing-us-peers) [多来源一致] |

- **注意**:预测跨度大、假设强,且有媒体称存在"泡沫"争议 [Talking Logistics 2026-01](https://talkinglogistics.com/2026/01/12/the-humanoid-robot-bubble-is-getting-hard-to-ignore/)(未读全文,仅作存在争议的提示)。
- **中国供应链**:中国有从零部件到整机的完整产业链,政策与制造规模支撑增长 [Omdia 经 TechNode/36Kr 转述](https://technode.com/2026/01/09/chinas-agibot-leads-global-humanoid-robot-shipments-in-2025-omdia-says/);减速器/电机/芯片国产化比例未取得来源。[来源缺口]

---

## 6. 与汽车的趋同

- 车企入局:XPeng 于 2026-09-08 宣布在广州启用 IRON 人形机器人产线(称核心工序 80%+ 自动化,目标 2026 年底量产、2027 年交付;媒体称初期月产 1,000+ 台、2030 年目标年产 100 万台,但公司未披露产线额定产能)[humanoid.guide](https://humanoid.guide/?p=17841), [Interesting Engineering](https://interestingengineering.com/ai-robotics/xpeng-iron-humanoid-robot-production) [事实/有来源,企业自述,未独立验证];Xiaomi 于 2026-03-02 经微博称其人形机器人在 EV 工厂压铸车间执行自攻螺母放置任务,成功率 90.2%、连续自主运行 3 小时(经 Finviz 转述;机型名未给出,原稿的“CyberOne”表述已删除)[Finviz](https://finviz.com/news/326566/tesla-rival-xiaomi-deploys-humanoid-robot-with-3-hours-of-autonomous-operating-time-at-ev-assembly-plant)。原稿中“18 家车企入局”“XPeng CEO 称 70% 技术可共享”“Iron 参与 P7+ 组装”仅见 CoAI 单一二手来源,本次未能在其他页面复核,降为[推断/未核实]。
- 共享供应商:Infineon、NXP、ST、Renesas、TI 同时服务车与机器人(见第 1 节)[多来源一致,厂商营销]。
- 共享计算平台:NVIDIA Thor 系列同时用于汽车与机器人;QNX OS for Safety 同时有 ISO 26262 与 IEC 61508 认证 [事实/有来源]。
- "SDV 式"架构的机器人对应:中央计算 + 分布式关节/区域控制器 + 以太网/CAN FD [推断]。
- **汽车工程师流向机器人**:有企业层面的转型证据(上条),但**个人层面的人才流动统计未取得来源**。[来源缺口]

---

## 结论要点

1. 机器人关节/电机控制 MCU 主要是 TI C2000、ST STM32、NXP i.MX RT/MCX、Renesas RZ/T 等(均为厂商宣称);计算侧以 NVIDIA Jetson Thor 为代表 [厂商营销 + 事实]。
2. 车规 MCU + Classic AUTOSAR 在机器人上只有厂商推广级例证(如 AURIX DRIVECORE TC4 + MICROSAR Classic),未发现量产机型证据 [事实/有来源 + 来源缺口]。
3. 标准:ISO 10218-1/-2:2025 已于 2025-02 发布;ISO 25785-1 仍在草案阶段;AMR 看 ISO 3691-4:2023 [多来源一致]。
4. 认证 RTOS 在机器人上可选:SafeRTOS、QNX、ThreadX、Zephyr(进行中)[事实/有来源]。
5. 市场预测差异巨大(Goldman Sachs 2026-09 版 2035 年约 1380 亿美元/650 万台,较其 2024-02 版 380 亿美元上调;Morgan Stanley 2025 年 2050 年硬件年收入约 4.7 万亿美元),均为 [分析师预测],且口径(TAM/收入/保有量)不同;2025 实际出货约 1.3 万台(Omdia 2026-01)[分析师预测/事实]。

## 对你的意义

- **你的优势**:ISO 26262 思维(ASIL/FMEA/FTTI)可迁移到 IEC 61508/ISO 13849;RH850/AURIX 类多核锁步、ECC、安全外设的经验,对机器人关节安全 MCU 评估有帮助。
- **岗位方向** [推断]:机器人关节控制固件(FOC + CAN FD/EtherCAT + 安全)、功能安全工程师、Zephyr/FreeRTOS 板级支持。
- **差距**:电机控制(FOC)、EtherCAT、ROS 2、Linux 实时、机器人安全标准(ISO 10218/13849/3691-4)。
- **风险提示**:机器人行业成熟度低于汽车,AUTOSAR 式标准化栈缺位,工程方法更碎片化;人形市场预测存在泡沫争议。

## 来源列表(访问日期 2026-10)

- TI:https://www.ti.com/lit/pdf/sprsp85 ; https://www.mouser.in/new/texas-instruments/ti-tms320f28p55x-real-time-mcus/
- ST:https://www.st.com/resource/en/application_note/an6570-motor-control-architectures-for-dexterous-humanoid-robot-hands-stmicroelectronics.pdf ; https://www.st.com/en/applications/robotics/articulations-legs-and-arms.html
- NXP:https://www.nxp.com/design/design-center/development-boards-and-designs/motor-and-motion-control-for-humanoids-and-mobile-robots:MOBILE-ROBOTICS-MOTION-CONTROL-HOLOSCAN-SENSOR
- Renesas:https://www.renesas.com/sg/en/products/microcontrollers-microprocessors/rz-mpus/rzt2m-high-performance-multi-function-mpu-realizing-high-speed-processing-and-high-precision-control ; https://www.renesas.com/en/products/microcontrollers-microprocessors/rz-mpus/rzt2l-high-performance-mpu-realizing-high-speed-and-high-precision-real-time-control-ethercat
- Infineon:https://www.infineon.com/ja/event/webinar/2026/automotive-mcus-the-right-solution-for-humanoids ; https://www.barchart.com/story/news/34371148/infineon-to-enable-humanoid-robots-with-precise-motion-and-efficiency-powered-by-nvidia-technology ; https://www.tasking.com/aurix-drivecore-tc4-it2-infineon-intronvector-tasking/ ; https://www.eenewseurope.com/en/infineon-looks-to-humanoid-robots-with-marvell-ethernet-buy/
- NVIDIA Thor:https://jetsonhacks.com/2025/09/21/nvidia-jetson-agx-thor-the-new-king-of-embedded/amp/ ; https://docs.peridio.com/hardware/under-evaluation/jetson-agx-thor
- Agility / Boston Dynamics:https://agilityrobotics.com/content/agility-robotics-powering-the-future-of-robotics-with-nvidia-jetson-thor ; https://bostondynamics.com/news/boston-dynamics-expands-collaboration-with-nvidia/
- Optimus:https://www.notateslaapp.com/news/1000/everything-we-know-about-optimus-the-tesla-robot ; https://www.robotics247.com/article/tesla_shares_more_details_about_optimus_humanoid_robot
- Unitree G1:https://www.china-amass.net/news/deep-dismantling-of-unitree-g1-knee-joint-actuator-full-analysis-of-120-n%c2%b7m-torque-twostage-planetary-reduction-and-dual-encoders/
- EtherCAT 人形:https://portal.fis.tum.de/en/publications/an-ethercat-based-real-time-control-system-architecture-for-human/
- 标准:https://www.iso.org/standard/73933.html ; https://www.evs.ee/en/evs-en-iso-10218-1-2025 ; https://www.iso.org/standard/91469.html ; https://www.i-scoop.eu/iso-25785-1-explained-and-what-it-means-for-humanoid-robot-safety/ ; https://www.fabrico.io/blog/iso-3691-4-driverless-industrial-trucks/ ; https://genorma.com/en/standards/iso-3691-4-2023/amp ; https://www.motioncontroltips.com/faq-what-are-typical-drive-based-safety-functions
- RTOS:https://FreeRTOS.org/FreeRTOS-Plus/Safety_Critical_Certified/SafeRTOS.html ; https://qnx.software/content/dam/qnx-xwalk/pdf/product-briefs/qnx-os-for-safety-product-brief.pdf ; https://qnx.software/en/partner/partner-program/nvidia-igx-thor ; https://en.wikipedia.org/wiki/ThreadX ; https://docs.zephyrproject.org/latest/safety/safety_overview.html
- 市场:https://www.goldmansachs.com/insights/articles/the-global-market-for-robots-could-reach-38-billion-by-2035 ； https://humanoid.guide/goldman-raises-2035-humanoid-robot-forecast-to-6-5-million/ ； https://finance.yahoo.com/technology/ai/articles/goldman-sachs-just-supercharged-humanoid-135551371.html ; https://developer.tenten.co/morgan-stanleys-bold-prediction-humanoid-robots-will-create-a-5-trillion-market-by-2050 ; https://technode.com/2026/01/09/chinas-agibot-leads-global-humanoid-robot-shipments-in-2025-omdia-says/ ; https://en.jiemian.com/article/13924474.html ; https://scmp.com/tech/tech-trends/article/3340446/chinas-unitree-ships-more-5500-humanoid-robots-2025-surpassing-us-peers ; https://talkinglogistics.com/2026/01/12/the-humanoid-robot-bubble-is-getting-hard-to-ignore/
- 车企入局:https://getcoai.com/news/ev-maker-unveils-humanoid-robot-to-compete-with-tesla-optimus ; https://www.automotiveworld.com/news/xpeng-launches-iron-robot-production-line-2026-launch/ ; https://govt.chinadaily.com.cn/s/202604/10/WS69e6ec31498e23165e06eef6/robotics-offer-growth-path-for-automakers.html
