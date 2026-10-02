# 01 — 现有项目与既有文档技术审查（Phase 1 Research）

> 审查日期：2026-10-01
> 范围：claude_plan.md §1、§2、§17、Project Context、Information Boundary 指定的“理解现有项目 + 审查既有 Agent 文档”切片。
> 本文件只做 review，不修改任何既有文件。
> Source of Truth 优先级：AUTOSAR SWS > RH850 manual > 实际源码 > 项目文档 > 既有 Agent 输出。既有 docs 被视为“待验证的二手资料”。

术语缩写（沿用 docs/source-index.md:155-160 的定义）：

| 代号 | 文件 | 说明 |
| --- | --- | --- |
| HW-E | `references/downloads/r01uh0585ej0120.pdf`（文本：`artifacts/pdf-text/r01uh0585ej0120.txt`） | RH850/P1M-E User's Manual: Hardware R01UH0585EJ0120 Rev.1.20，3121 页，**目标器件主依据** |
| DS-E | `r01ds0505ed0100-rh850p1m-e.pdf` | RH850/P1M-E Datasheet Rev.1.00，72 页 |
| HW-P1x | `REN_r01uh0436ej0140-rh850p1x_MAH_20180330_1.pdf` | 旧 P1x 硬件手册，仅对照（其 §17 是 RS-CAN 而非 RS-CANFD） |
| DS-C | `REN_r01ds0506ed0100-rh850p1x-c_DST_20251218.pdf` | P1x-C 数据手册（M-CAN），与本目标无关 |
| SWS-CAN | `AUTOSAR_SWS_CANDriver.pdf` | **AUTOSAR CP R22-11** |
| SWS-DCM | `AUTOSAR_SWS_DiagnosticCommunicationManager.pdf` | **AUTOSAR CP R20-11** |
| SWS-MCU | `AUTOSAR_CP_SWS_MCUDriver.pdf` | **AUTOSAR CP R24-11** |
| SWS-IoHwAb | `AUTOSAR_CP_SWS_IOHardwareAbstraction.pdf` | **AUTOSAR CP R24-11** |
| SWS-Diag(AP) | `AUTOSAR_SWS_Diagnostics.pdf` | **AUTOSAR AP R22-11 —— Adaptive Platform，不是 Classic DCM！** |

“页码”均为 PDF 页码（`=== PDF PAGE n ===`），HW-E 中 PDF 页码与印刷页码一致（已抽查 p.257、p.791 页脚）。

---

## 1. 项目 Mental Model

### 1.1 项目最初目标与演变

项目经历了三次目标变化，这一点对后续重构至关重要：

| 阶段 | 目标 | 证据 |
| --- | --- | --- |
| ① 上板实施计划（已降级为历史） | 针对 R7F701381 / RH850/P1M-E，核实 RTA-CAR 12.9.0 + GHS + Renesas MCAL 的硬件适配，解决 `Rte_TickCounter` HARDWARE counter 链接阻塞（缺 `Os_Cbk_Now/Set/State/Cancel_Rte_TickCounter`），生成 ELF 并上板验证 CAN/诊断栈；另加 R1–R7 七模块 MCAL 参考实现（M6A） | plan-implementation-history.md:1,23,25,64-76,140-186；requirements-extracted.md:16-28 |
| ② “内部 agent 技术解说交付” | 不再上板；只回答 40 项（A1–J4）+ K1 + R1–R7，并产出供“公司内部 agent”使用的硬件交接资料 | plan.md:1-11, 17-23（P0–P7 全部标“完成”）；README.md:1-25 |
| ③ 当前（claude_plan.md） | 建立面向个人学习的**中文教学体系**：RH850 → MCAL → CAN → CanIf → CanTp → PduR → DCM → RTE → SWC，为未来公司内部 RTACAR DCM 升级做 transferable knowledge 准备；明确“不是真实量产项目”，不得虚构内部信息 | claude_plan.md:1-20, 1319-1363, 1650-1678 |

**关键冲突**：②阶段的文档（internal-agent-task.md、rh850-hardware-handoff.md、agent-guide.md 等）是以“交给公司内部 agent 去改内部工程”为语气写的（如 internal-agent-task.md:1-3 “你负责在内部 AUTOSAR Classic 工程中完成…”），与 ③ 的 Project Context（claude_plan.md:1323-1327 “不要把当前项目描述成真实 RTACAR 项目”）直接冲突。内容可复用，但**语境必须重写**。

### 1.2 已完成内容（按事实，而非按 plan.md 的“完成”勾选）

| 类别 | 实际状态 |
| --- | --- |
| 资料获取 | 补充下载了 P1M-E 专用硬件手册 HW-E（source-index.md:160-171，含 SHA-256）；全部 PDF 已抽取为带页码文本（tools/pdf_extract.py） |
| 硬件核查 | 存储图、时钟、中断通道、OSTM、RS-CANFD 寄存器/位域/地址、PORT ALT、WDTA、OPBT0、RESF/STAC 等已按页码整理（hardware-findings.md、rh850-hardware-handoff.md）。本次抽查全部吻合（见 §4） |
| 代码 | 仅 4 个小型 C 组件（OSTM 底层、CAN FD 位时间计算、tick 累计器、MMIO 注入层）+ 主机测试 |
| 工具 | PDF 抽取、主机测试脚本、寄存器索引生成/一致性检查脚本 |
| 未做 | 无 RH850 交叉编译、无启动代码/链接脚本、无 Can 驱动收发、无 CanIf/CanTp/PduR/DCM/RTE/SWC 任何代码、无上板、无 RTA 工程（截图路径均不存在，source-index.md:177-181） |

### 1.3 仓库结构

```text
rh850/
├── claude_plan.md                    # 当前（③）总体要求
├── plan.md / README.md               # ②阶段交付说明（自称“完成”）
├── plan-implementation-history.md    # ①阶段上板计划（历史）
├── requirements-extracted.md         # 4 张截图需求转录（A1–J4、K1）
├── 20261001_1227xx.jpg ×4            # 原始需求截图
├── *.pdf                             # RH850 DS/HW 手册、5 份 AUTOSAR 文档
├── references/downloads/r01uh0585ej0120.pdf   # P1M-E HW 手册（HW-E）
├── artifacts/
│   ├── pdf-text/*.txt|json           # 全文抽取（含页号）
│   ├── host-build/results.txt + .exe # 主机测试日志
│   ├── hardware-review/validation.txt
│   └── page-previews/*.png           # 手册页面截图（复杂表格人工核对用）
├── docs/                             # 既有 agent 文档（本文审查对象）
│   └── reference/research/           # Phase 1 研究输出（本文件所在）
├── examples/rh850_mcal_reference/    # 参考 C 组件
│   ├── platform/Rh850_Mmio.[ch]
│   ├── mcal/gpt/Ostm.[ch]
│   ├── mcal/can/Can_BitTiming.[ch]
│   ├── integration/Tick_Accumulator.[ch]
│   └── tests/test_reference.c
└── tools/ pdf_extract.py, run_host_tests.py, build_hardware_index.py, requirements.txt
```

外部依赖：rh850-autosar-tutorial-content.md §04 引用了仓库外的 `D:\side_project\openAUTOSAR`（HEAD `13499119e06e8c81f9470dc1e9b280c22cbfb0b3`，本次确认存在且 HEAD 一致）。

### 1.4 目标器件（已核实）

**R7F701381 = RH850/P1M-E，DPS 版本，Code Flash 1 MB，LFQFP100（14×14）。**
证据：DS-E p.2 Table 1.1 “DPS 1MB → R7F701381（100-pin）”；同表：CPU 160 MHz、“Main OSC = 16 MHz only”、Main core 1 + Lockstep、Double-precision FPU、MPU 16 ch、Local RAM 128 KB、Global RAM 64 KB、INTC1 32 ch / INTC2 352 ch。
注意：DS-E p.1 写“Code Flash up to 2MB / Data Flash up to 64KB”，是**系列上限**，不是本型号——这正是截图 `iROM 2048K` 假设出错的来源（requirements-extracted.md:46）。

### 1.5 工具链假设（全部来自截图，均未在本机验证）

| 项 | 截图值 | 状态 |
| --- | --- | --- |
| AUTOSAR 工具 | RTA-CAR 12.9.0（RTA-RTE 12.9.0） | 未验证；路径不存在（source-index.md:177） |
| OS | RTA-OS 12.9.0，Target port RH850GHS 5.0.39，variant `RH850GHS[P1M]` | 未验证；ETAS 安装包需登录（online-references.md:79） |
| 编译器 | Green Hills `ccrh850`，`-cpu=rh850g3m -fsoft -large_sda -sda=0 -registermode=32 -reserve_r2 -no_callt` | 未验证；PATH 中无 ccrh850/gbuild（source-index.md:181） |
| MCAL | Renesas P1M MCAL（Can/Dio/Fls/Gpt/Mcu/Port/Wdg），**AUTOSAR 4.2.2 API** | 未验证；Renesas 社区称 P1M-E 4.07.00 示例为 4.0.3（online-references.md:66） |
| 链接 | `rh850ghs.ld`：`iROM 2048K@0x0`（错，应 1 MB）、`PLRAM 118K@0xFEDE0000`、stack `10K@0xFEDFD800` | 地址算术核对正确，容量假设错误 |
| 主机测试 | TDM-GCC 10.3.0（C99, -O2 -Wall -Wextra -Werror -pedantic） | 已验证（见 §2.3） |

**对教程的含义**：按 Information Boundary（claude_plan.md:1650-1678），上述全部应在教程中标注为“截图示例 / 需要在真实项目环境中确认”，不得作为“公司项目事实”书写。

**AUTOSAR 版本错配**：截图工程用 4.2.2 API；本地 SWS 是 R20-11（DCM）、R22-11（CAN）、R24-11（MCU/IoHwAb）；既有 docs 引用的在线规范是 R23-11。各章写 API 签名时必须标注版本（例：`Can_Write` 在 R22-11 返回 `Std_ReturnType`（E_OK/E_NOT_OK/CAN_BUSY），SWS-CAN p.81 [SWS_Can_00233]；4.2.2 时代为 `Can_ReturnType`（CAN_OK/CAN_NOT_OK/CAN_BUSY）——后者属本人领域知识，本地无 4.2.2 SWS，标“unverified locally”）。

---

## 2. Inventory：真实实现 vs 教学简化 vs 纯文档

### 2.1 代码分类

| 文件 | 分类 | 实际做了什么 | 未做 / 局限 |
| --- | --- | --- | --- |
| platform/Rh850_Mmio.[ch] | [Educational Implementation] 可注入 MMIO 抽象 | 8/16/32 位 volatile 写、8/32 位读；测试可替换为 fake bus（Rh850_Mmio.h:6-16） | **没有 read16**（Rh850_Mmio.h:9-16），因此无法读 16 位 EIC / IC0CKSEL；无屏障/SYNCP；无临界区 |
| mcal/gpt/Ostm.[ch] | [RH850 Hardware Specific] 底层寄存器访问（非 AUTOSAR Gpt API） | OSTM0/1：TE 检查→IC0CKSELn(16bit)=0 选 PCLK→TOE=0→CMP(32)→CTL(8)；Start/Stop(有界轮询)/SetCompare/ReadCounter；`Ostm_IntervalCompare` 精确换算 CMP=N−1（Ostm.c:16-77） | 无 Gpt_Init/StartTimer/通知/ISR/EIC；不支持 OSTM3–7（FEINT）；不支持 TAUD/TAUJ 计数链；Stop 后自由运行原点归零（Ostm.h:18-20 已注明） |
| mcal/can/Can_BitTiming.[ch] | [RH850 Hardware Specific] 纯计算 | FD 接口 NCFG/DCFG 编码与范围检查、两阶段 BRP 相同、TDC 时 divider≤2、保守 `TSEG1>TSEG2>SJW`（Can_BitTiming.c:4-51） | **只支持 FD 接口布局**；Classical CFG（`0x023E0003`）只存在于文档和 build_hardware_index.py:152 的 assert 中，没有 C 实现；不写任何寄存器；无 Can 驱动 |
| integration/Tick_Accumulator.[ch] | [Educational Implementation] | 32 位自由上计数 → 16 位 1 ms tick，无符号差累计 + 余数（Tick_Accumulator.c:14-28） | 无扩展单调时间/epoch；无 `Os_Cbk_*` 回调；要求采样间隔 < 一圈且调用串行化（.h:6-8） |
| tests/test_reference.c | 主机测试 | 5 组：OSTM 访问宽度/顺序/忙拒绝/超时、周期换算边界、CAN 时序编码、100000 次 tick 采样回绕 | Fake bus 只模拟 OSTM；不模拟 EIC、CAN、时序 |

**完全没有代码的部分**（只有文档或连文档都没有）：Mcu、Port、Dio、Fls、Wdg 驱动；Can_Init/Write/Rx/ISR；EIC/向量/启动/链接；CanIf、CanTp、PduR、DCM、DEM、NvM、RTE、SWC、OS 端口——全部为 doc-only（mcal-reference-guide.md:193-202 自己也如实列出）。

### 2.2 文档分类

| 类型 | 文件 |
| --- | --- |
| 手册核查事实（可复用度高） | hardware-findings.md、rh850-hardware-handoff.md、hardware-registers.csv/json、agent-guide.md 中“手册确认”段落 |
| 设计/教学解说（概念为主） | counter-design.md、mcal-reference-guide.md、rh850-autosar-tutorial-content.md |
| 项目管理/追踪（对教程价值低） | plan.md、plan-implementation-history.md、requirements-status.md、hardware-review.md、internal-agent-task.md、source-index.md、online-references.md |
| 原始需求 | requirements-extracted.md |

### 2.3 Build / Test 状态（本次复测）

为避免改写既有 `artifacts/host-build/results.txt`（run_host_tests.py:30-34 会覆盖该文件），本次把 `examples/` 与 `tools/run_host_tests.py` 复制到 scratchpad 后执行 `python tools/run_host_tests.py`：

```text
40MHz candidate: NCFG=0x071E1801 DCFG=0x023E0001
PASS: 5 test groups; 100000 tick samples across hardware/OS rollovers.
exit=0     (gcc.EXE (tdm64-1) 10.3.0)
```

与仓库内 artifacts/host-build/results.txt 记录一致。另外 `python tools/build_hardware_index.py --check`（只读校验，不写文件；build_hardware_index.py:169-173）同样通过：

```text
PASS: 1185 register entries; 2 exclusive CAN maps; widths/alignment, worked addresses,
timing and FIFO profiles; published JSON/CSV match source.
```

结论：**现有代码可编译、主机测试通过；但这只证明算术与访问宽度模型，不证明任何目标硬件行为**（与 hardware-findings.md:311-313 的自述一致）。

---

## 3. 逐文档 Review 表

图例：Action = keep / split→章节 / merge / fix / archive。“Upgrade 价值”= 对未来真实 RTACAR DCM 升级的帮助（高/中/低）。

| 文件 | 目的 | 值得保留的正确内容 | 过于简化 | 可能错误 / 不一致（含证据） | 缺失（RH850 映射 / 代码路径 / SWS 映射） | Upgrade 价值 | 建议 Action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| README.md | ②阶段入口 | 复现命令（:24-26） | — | 自称“解说交付已完成”（:3），与③目标不符 | — | 低 | **rewrite**：改成新教程总入口或指向 docs/rh850-autosar-tutorial-content.md |
| plan.md | ②阶段交付计划 | §4 核心结论表（:224-234）是很好的“事实摘要” | — | 全部勾“完成”，读者易误以为项目已完成 | — | 低 | **archive**（移至 docs/archive/ 或顶部加“历史”标注）；结论表 merge 到 reference/rh850-autosar-mapping.md |
| plan-implementation-history.md | ①阶段上板计划 | K1 决策框架（:64-76）、验证证据清单（:78-89）、M6A 模块验收标准（:150-160） | — | — | — | 中（真实项目 bring-up 检查清单的雏形） | **archive**；:78-89 与 :150-160 可 split→ 09-real-project-preparation/01、08-integration/01-ecu-configuration-checklist |
| requirements-extracted.md | 截图需求转录 | 截图中的真实工程现象（Rte_TickCounter HARDWARE、四回调缺失、13 邮箱/7 规则、mask=0 疑问）是极好的“真实项目会遇到的问题”教学素材 | — | 其中 `RS-CAN`（:47,:70）应为 RS-CANFD（HW-E §17 标题 “CANFD Interface (RS-CANFD)” p.791）；`iROM 2048K`、`80 MHz fCAN` 为错误假设（已由后续文档纠正）；属原文转录，不应修改 | C:\VMEPS 等路径、`int.bbm.vm-eps.plrp` 属截图中的项目信息 → 教程中只可作“示例”，不得当公司事实 | 中 | **keep 原样（作为原始输入）**；在教程中以“案例问题”方式引用 |
| docs/agent-guide.md | A1–J4 逐项问答 | 证据标签体系（:22）；A1 核/FPU、A2 存储、A3 时钟、A4 位时间、B1 EITB/INTBP、B4 EIP、C1 OSTM 寄存器表（:96-105）、C4 回绕推导、D3 GAFLM 语义、F1 WDTA 公式、I2 ALT 编码、I3 未用引脚——抽查均正确 | G1 启动顺序只是依赖顺序；G2 GHS 选项未给出含义；J1–J4 全部“未知” | **:71 把 EI184 称为“CAN0 … RxFIFO”**，易误导：p.285 实为 “COM RX FIFO interrupt 0 (INTRCAN0REC)”（通道 Tx/Rx 共用 FIFO），Rx FIFO0–7 用 EI190（INTRCANGRECC, p.286、p.792）；hardware-review.md:13 已纠正但本文未同步 → **fix** | 无 SWS 映射；无代码路径；无 DCM 内容；直接/表向量偏移值未给（见 §4 F-INT-6） | 中（硬件事实高、DCM 无） | **split**：A→01-rh850/03,07；B/J3→01-rh850/06；C→03-mcal/05-gpt + 02-autosar-classic/06；D→04-can-mcal/02-04；E→附录；F→03-mcal（Wdg 若有）；G/H→01-rh850/04,05；问答形式本身 archive |
| docs/counter-design.md | K1：Rte_TickCounter 硬件/软件计数器方案 | 三种单位分离（raw/logical/OS tick, :20-28）；回绕与晚匹配、Cancel 不能停表、generation 概念；行为用例表（:160-173）——技术上扎实 | 回调签名均为概念名（正确地拒绝编造 ABI） | 无明显错误；80 MHz 下 65535 tick=5242800000 counts（:48）算术正确 | 缺 AUTOSAR OS SWS（Counter/Alarm、IncrementCounter 调用上下文）引用——本地无 OS SWS；缺 RTA-OS 回调契约（需真实环境） | 中（OS/Rte 集成理解） | **split→ 02-autosar-classic/06-os-task-isr.md（Counter/Alarm 小节）+ 03-mcal/05-gpt-driver.md（OSTM 时基）**；“四回调”部分标 [Conceptual] |
| docs/hardware-findings.md | 首轮硬件核查报告 | §1 差异表（:184-195）、§2 存储/时钟（:197-223）、§4 中断表（:247-274）、§5 OSTM（:276-298）——抽查全部吻合 | §6 Flash/WDTA 只给结论 | :261 EI184 标签 “CAN0 Tx/Rx FIFO receive completion” 与 p.792 原文一致，但与 :264 “CAN receive FIFO”并列时读者难以区分 common FIFO 与 Rx FIFO → 加注 | 无 SWS 映射；无代码路径 | 中 | **merge** 到 reference/rh850-autosar-mapping.md 和 01-rh850/03、06、07；原文件 archive |
| docs/hardware-review.md | 复审差异记录 | 对前次文档的 15 项修正（:7-25）本身就是“常见误区”素材；保留的 3 处手册内部差异（:27） | — | — | — | 低 | **archive**；:7-27 的误区可 merge 到 04-can-mcal/15-can-driver-debugging.md “常见错误” |
| docs/internal-agent-task.md | 给“内部 agent”的指令 | 10 步工作流（:9-18）、“不得绕过的硬件约束”（:22） | — | **语境违反 Project Context**（:1-3 “你负责在内部 AUTOSAR Classic 工程中…”）与 Information Boundary | — | 中（可改写为“到真实项目后的 checklist”） | **rewrite→ 09-real-project-preparation/02-how-to-read-mcal.md / 07-rtacar-dcm-upgrade-preparation.md 的 checklist**；原文件 archive |
| docs/mcal-reference-guide.md | R1–R7 七模块设计解说 | 资源归属表（:24-31）、公共错误处理（:35-39）、Port 初始化顺序（:65）、Dio 读回区分（:76）、Gpt CMP=N−1（:96）、Fls 作业状态机（:126-131）、WDTA 固定 0xAC/VAC 区分（:146） | 每模块只有“职责+步骤”，无 SWS API 表、无配置容器（EcucContainer）映射、无代码 | :54 列 `Mcu_GetPllStatus/Mcu_DistributePllClock` 而未说明：**P1M-E §12 根本没有软件可编程的 PLL/MOSC 控制寄存器**（HW-E §12.3 只有 CLKD2/3、CKSC2/3/8，pp.471-481；DS-E p.2 “Main OSC = 16 MHz only”）——这两个 API 在本器件上大概率为平凡实现，应显式写出（handoff:40 已写对） | 缺 SWS-MCU 映射（本地 R24-11 有 Mcu_Init/InitRamSection/InitClock/DistributePllClock/GetPllStatus/GetResetReason/GetResetRawValue/PerformReset/SetMode/GetRamState）；Port/Dio/Gpt/Fls/Wdg 无本地 SWS | 中 | **split**：R1→03-mcal/02-mcu-driver；R2→03-mcal/03-port；R3→03-mcal/04-dio；R4→03-mcal/05-gpt；R5→04-can-mcal/06,09-11；R6/R7→后续阶段附录 |
| docs/online-references.md | 网上资源评估 | 适用性边界判断（:60-71）、ETAS 5.0.39 安装包页（:79）、G3M 分支指令限制 TN（:71） | — | 外部链接本次未复核 | 无 openAUTOSAR 条目（教程却大量引用 openAUTOSAR） | 低 | **merge→ reference/source-traceability.md**（外部资源节） |
| docs/requirements-status.md | A–K、R1–R7 覆盖追踪 | 无新增技术内容 | — | “已解答”≠已验证，易误导 | — | 低 | **archive** |
| docs/source-index.md | PDF 来源、哈希、环境 | 资料 ID/版本/SHA-256（:155-171）、环境缺失表（:175-185）、复现命令 | — | 未列 5 份 AUTOSAR PDF 及其版本（R20-11/R22-11/R24-11/AP R22-11） | — | 中（溯源） | **keep + fix**：补 AUTOSAR 文档行，迁移为 reference/source-traceability.md |
| docs/rh850-hardware-handoff.md | CAN/诊断/OS 硬件交接（最详细） | 存储表（:26-34）、时钟与 CLKOUT（:38-42）、CAN pin/ALT（:48-58）、PORT 寄存器宽度（:62-76）、Classic/FD 双布局（:98-108）、核心寄存器位定义（:114-154）、位时间（:158-178）、AFL/FIFO/Tx（:180-260）、中断（:262-290）、冷启动 12 步（:292-311）、OSTM/RESF/STAC/WDTA（:313-361）、分层验收与故障表（:378-397）——**全仓库最有价值的 RH850 硬件资料** | 偏“寄存器 cookbook”，缺“为什么”与 AUTOSAR API 对应；故障表很好但未接到 CanIf/DCM | 内容抽查全部正确。语境问题同 internal-agent-task（:401 “内部 agent 可以自行从工程…读取”）| 缺 SWS-CAN 映射（如 Can_Write↔TMC/TMSTS、SWS_Can_00276/00016 ↔ TMTRF；Can_SetControllerMode↔CHMDC）；缺写保护机制（见 F-SYS-3）；缺异常源码 EIIC 值 | **高**（CAN 链路 debug 必需） | **split（主要种子）**：§2→01-rh850/03,07；§3→04-can-mcal/04；§4-5→04-can-mcal/02；§6→04-can-mcal/03；§7-9→04-can-mcal/07,10,11；§10→04-can-mcal/05 + 01-rh850/06；§11→04-can-mcal/06；§12→03-mcal/05 + 01-rh850/04；§13→04-can-mcal/15 + 08-integration/07 |
| docs/rh850-autosar-tutorial-content.md | 一份“单页大教程”的 Markdown 源（原用于生成 HTML） | §02 CPU 寄存器/系统寄存器表（:24-36，已核 HW-E pp.190-192）、§03 最小模块集合（:63-72）、§07 MCAL 公共行为（:183-192）、§10 CAN driver 契约表（:251-261）、§12 Tx/Rx 伪代码（:320-351）、§13 诊断路径与“四种编号”（:374-391）、§17 故障表（:483-496）、§18 理解检查五问（:511） | §13 DCM 只有 1 段（:393）；无 DSL/DSD/DSP、无 Dem/NvM/SecurityAccess、无 RTE/SWC | ① :231 “本案例可为 OS 独占 **OSTM0**”与 counter-design.md:7、agent-guide.md:18 的“OSTM0=Gpt、OSTM1=OS 候选”不一致（教学案例 vs 截图工程未区分）；② :333 `return CAN_OK` 未标版本（R22-11 为 E_OK，SWS-CAN p.81）；③ 24 处 `[R01]…[R21](#ref-rNN)` 锚点在 md 中**均未定义**，`<div id="layer-explorer|timing-calculator|flow-explorer|acceptance-checklist|reference-sources|register-explorer|embedded-hardware-guide">`（:59,:292,:368,:479,:517,:529,:531）指向仓库中不存在的 HTML 构建；:502 “把本HTML…交给内部agent”；④ 无 H1 标题 | openAUTOSAR 引用是外部仓库（抽查 2 项属实：`include/Platform_Types.h:31 CPU_BYTE_ORDER HIGH_BYTE_FIRST`；`system/SchM/src/SchM.c:424 SchM_MainFunction` 空体）但无文件:行号；无任何 SWS ID | 中-高 | **按 claude_plan.md:2061-2155 重构为 Master Index**；正文按 §6 映射拆分到各章；本文件保留目录 + 系统图 + 学习地图 |
| docs/hardware-registers.csv / .json | 寄存器地址索引（1185 条） | 模式分离（SYSTEM/CAN_BOTH/CAN_CLASSIC/CAN_FD）、宽度、页码；`--check` 通过 | 不是 SVD，只覆盖 CAN/PORT2-5/OSTM0-1/EIC/WDTA/RESF/STAC | WDTA0REF 标 RO 正确（HW-E p.1530 “can be read in 8-bit units”） | 无位域 | 中 | **keep**，移入 docs/reference/，作为 04-can-mcal 附录数据源 |
| examples/rh850_mcal_reference/README.md | 代码范围说明 | 实现/未实现表（:9-14）、集成前置条件（:35-43） | — | — | — | 中 | **keep**；在新教程 Code Walkthrough 中引用并打 [Educational Implementation] 标签 |
| tools/*.py | 抽取/测试/索引 | 全部可用 | — | run_host_tests.py 会覆盖 artifacts/host-build/results.txt（:30-34），后续 agent 运行前需知 | — | 低 | **keep** |
| artifacts/host-build/results.txt、artifacts/hardware-review/validation.txt | 测试与校验日志 | 与本次复测一致 | — | validation.txt 的“15 个 Markdown / 150 链接”是当时计数 | — | 低 | **keep**（证据） |

### 3.1 跨文档的系统性缺陷（重构时必须处理）

1. **零 SWS 追溯**：`grep "SWS_" docs/*.md` 结果为 0。所有 AUTOSAR 行为描述都没有 requirement ID，不满足 claude_plan.md §2“哪些缺少 AUTOSAR SWS requirement 对应关系”的要求。
2. **DCM/RTE/SWC 几乎空白**：与项目最终目标（RTACAR DCM 升级）最相关的 DSL/DSD/DSP、会话/安全访问、DID、Dem/NvM 交互、RTE/SWC 在所有既有 docs 中合计不足 1 屏（tutorial :370-405）。
3. **语境错误**：“内部 agent”“内部工程”叙事（internal-agent-task.md 全文、handoff.md:3-7,:399-412、agent-guide.md:5、plan.md:195）违反 Project Context。
4. **重复**：存储表、时钟表、中断表、OSTM 寄存器、CAN 位时间至少在 agent-guide、hardware-findings、handoff、tutorial 4 处重复，且措辞略有差异（如 EI184 标签）。重构时应“单一事实源”：事实只写在 reference/ 与对应章节一处，其他处链接。
5. **本地 SWS 不全且版本混杂**：缺 CanIf、CanTp、PduR、Dem、NvM、Rte、Os、EcuM、BswM、ComM、CanSM、Det、Port、Dio、Gpt、Icu、Layered Architecture SWS；`AUTOSAR_SWS_Diagnostics.pdf` 是 **Adaptive Platform**（文首 “AUTOSAR AP R22-11 … Part of AUTOSAR Standard: Adaptive Platform”），不可用于 Classic DCM 章节。
6. **既有 docs 的优点值得延续**：证据分级（手册事实/推导/未知）、拒绝编造 ABI、明确“候选值≠实测值”——这与 claude_plan.md §17 的 `[Conceptual]/[Educational Implementation]/[AUTOSAR API]/[RH850 Hardware Specific]` 标签可以一一对应。

---

## 4. 可复用的已核实事实（供后续章节作者直接引用）

标记：**✔ verified**＝本次直接对照手册/SWS 文本页面确认；**◐ verified-by-prior**＝既有文档声称经页面图像核对，本次文本抽取表格错位无法独立确认；**✘ unverified**＝本地无依据。

### 4.1 器件 / 存储

| ID | 事实 | 状态 / 来源 |
| --- | --- | --- |
| F-DEV-1 | R7F701381 = P1M-E、DPS、1 MB Code Flash、LFQFP100 14×14 | ✔ DS-E p.2 Table 1.1 |
| F-DEV-2 | CPU 160 MHz；1 个主核 + lockstep checker（非第二应用核）；双精度 FPU；MPU 16 ch；I-cache 16 KB 4-way | ✔ DS-E p.2；“two RH850G3Ms: one … master” DS-E p.1 |
| F-DEV-3 | FPU 使用通用寄存器 r0–r31，无独立浮点寄存器堆；FPSR 为 SR6,0 | ✔ HW-E p.213、p.192 |
| F-DEV-4 | OSTM 共 7 个：OSTM0、1（带输出）+ OSTM3–7；**无 OSTM2**；WDTA 仅 WDTA0 | ✔ DS-E p.3（OSTM 5 + OSTM(Output) 2；WDT 1）；HW-E p.1542、p.1522 |
| F-MEM-1 | Code Flash 用户区 `0x00000000–0x000FFFFF`（1 MB 型号） | ✔ HW-E p.257 Table 4.1 Note 1 |
| F-MEM-2 | Code Flash 扩展用户区 `0x01000000–0x01007FFF` 32 KB | ✔ HW-E p.257 |
| F-MEM-3 | LRAM self `0xFEDE0000–0xFEDFFFFF` 128 KB；`0xFEBE0000–0xFEBFFFFF` 为 PE1 area（同一 RAM 别名） | ✔ HW-E p.257 |
| F-MEM-4 | GRAM Bank A `0xFEEF8000–0xFEEFFFFF`、Bank B `0xFEF00000–0xFEF07FFF`，各 32 KB | ✔ HW-E p.257 |
| F-MEM-5 | Data Flash `0xFF200000–0xFF207FFF`（1 MB 型号 32 KB），64 B 块 | ✔ HW-E p.257 Note 3、p.2859 |
| F-MEM-6 | Data Flash 寿命 125000 次/20 年 或 250000 次/3 年（带温度条件） | ✔ DS-E p.63 Table 3.13 |
| F-MEM-7 | “self” I/O 区（FFFE E000H–FFFE FFFFH）含 INTC1 等 CPU 私有功能，只能由 PE1 访问 | ✔ HW-E p.257 Note 2 |
| F-MEM-8 | LRAM/GRAM/DTS RAM/CSIH RAM 有硬件初始化（含 ECC 位），所有 RAM（除 I-cache、Emulation RAM）带 ECC | ✔ HW-E p.2890 |
| F-MEM-9 | STAC_xxx（0xFFF81320/1420/1520/1E20）控制各 RAM 在后续复位时是否硬件初始化 | ◐ handoff.md:342-353（HW-E pp.427-430，本次未逐页核） |

### 4.2 CPU / 异常 / 中断

| ID | 事实 | 状态 / 来源 |
| --- | --- | --- |
| F-CPU-1 | r3=SP、r4=GP、r5=TP、r30=EP、r31=LP；r1 汇编器保留；r2 “used when the real-time OS used…” | ✔ HW-E p.190 |
| F-CPU-2 | 系统寄存器 (regID,selID)：EIPC(0,0) EIPSW(1,0) FEPC(2,0) FEPSW(3,0) PSW(5,0) FPSR(6,0) EIIC(13,0) FEIC(14,0) CTPSW(17,0) RBASE(2,1) EBASE(3,1) INTBP(4,1) | ✔ HW-E p.192 |
| F-CPU-3 | MPU 系统寄存器在 selID 5/6（MPRC SR1,5；MPUA0 SR1,6 …） | ✔ HW-E p.214 |
| F-CPU-4 | G3M little-endian | ✘ 本地无 G3M Software Manual（tutorial :49 引用的是外部 R01US0123EJ0140） |
| F-INT-1 | EIC 地址：EIC0–31 = `0xFFFEEA00 + 2n`（INTC1/self 区）；EIC32–383 = `0xFFFFB040…0xFFFFB2FE`（即 `0xFFFFB000 + 2n`） | ✔ HW-E p.266 寄存器表（txt:17402-17405）、p.267 |
| F-INT-2 | EIC 16 位：EICT[15] RO、EIRF[12]、EIMK[7]、EITB[6]、EIP[3:0]（0 最高、15 最低）；同级时小通道号优先 | ✔ HW-E pp.267-268、p.281 |
| F-INT-3 | EIC 复位值 `008FH`（EIMK=1、EIP=15），部分通道 `808FH`（EICT=1） | ✔ HW-E 寄存器表 txt:17402-17403 |
| F-INT-4 | EIMK=1 只屏蔽送 CPU，EIRF 仍会置位；bit 操作指令是读-改-写，可能丢/重中断；bits 15-13、11-8、5、4 禁止 bit 操作 | ✔ HW-E pp.267-268 |
| F-INT-5 | 表参考：handler 地址读取位置 = `INTBP + 4 × channel` | ✔ HW-E p.281 |
| F-INT-6 | 直接向量（RINT=0）：偏移 +100H…+1F0H 按优先级 0–15 决定；RINT=1：一律 +100H。FENMI +0E0H、FEINT +0F0H | ✔ HW-E pp.281-282（**既有 docs 未给出具体值，新事实**） |
| F-INT-7 | 每个 EI 通道的异常源码（EIIC 值）= `0x1000 + ch`（例：INTWDTA0 1009H、INTOSTM0 104AH、INTRCANGRECC 10BEH） | ✔ HW-E pp.282-286 Table 6.11 “Source Code” 列（**新事实，debug 时读 EIIC 用**） |
| F-INT-8 | FENMI/FEINT/EIINT(直接向量)/SYSERR/FPI 的 handler 前需插 SYNCP | ✔ HW-E p.281 CAUTION、p.256 |
| F-INT-9 | 通道号：INTWDTA0=9（表偏移 +024H）、INTOSTM0=74（+128H）、INTOSTM1=75（+12CH） | ✔ HW-E pp.282-283 |
| F-INT-10 | RS-CANFD：CAN0 ERR/REC(common FIFO)/TRX=183/184/185；CAN1=186/187/188；INTRCANGERR=189；**INTRCANGRECC（Rx FIFO0–7）=190**；CAN2=191/192/193；表偏移 +2DCH…+304H | ✔ HW-E p.285-286、p.792 Table 17.8 |
| F-INT-11 | OSTM3–7 走 FEINT，不占 EI 通道 | ✔ HW-E p.1544、p.282 |
| F-INT-12 | Flash sequencer end / error = EI379 / EI383 | ✔ HW-E p.290 |
| F-INT-13 | CAN 中断源为高电平（EICT=1、EIRF 只读，须清外设源）；OSTM0/1 为同步边沿 | ◐ handoff.md:284；EIRF 两种行为本身 ✔ p.267 |

### 4.3 时钟 / 写保护

| ID | 事实 | 状态 / 来源 |
| --- | --- | --- |
| F-CLK-1 | MainOSC 16 MHz → PLL → CLK_CPU 160 MHz；CLK_HSB 80 MHz；CLK_LSB 40 MHz；CLK_IOSC 8 MHz；WDTACLKI 8 MHz / 250 kHz；CLK_ADC 40/20 MHz | ✔ HW-E p.469 Table、p.470 框图 |
| F-CLK-2 | P1M-E 时钟控制器寄存器只有 CLKD2DIV/STAT、CLKD3DIV/STAT、CKSC2C/S、CKSC3C/S、CKSC8C/S（外部时钟输出与 ADC），**没有软件 PLL/MOSC 使能寄存器** | ✔ HW-E §12.3 目录（txt:887-900）、pp.471-481 |
| F-CLK-3 | CKSC2C=`0xFFF89080`，源 ID：4=CLK_LSB、5=CLK_CPU、6=CLK_IOSC（3=MainOSC 见 handoff）；EXTCLK0O 最高 20 MHz | ✔ HW-E p.476、p.469（3=MainOSC ◐） |
| F-CLK-4 | RS-CANFD 时钟：pclk=CLK_HSB 80 MHz（接口时钟，固定）；clkc=CLK_LSB 40 MHz；clk_xincan=CLK_MOSC 16 MHz；GCFG.DCS(bit4) 0=clkc、1=clk_xincan；>2 Mbps 时不可选 clk_xincan | ✔ HW-E p.791 Table 17.6/17.7、p.949 |
| F-CLK-5 | OSTM0/1 计数时钟 PCLK=CLK_HSB 80 MHz；IC0CKSEL0/1 `0xFFDD6000/6004`（16 位）可改为 TAUD/TAUJ 计数使能链路；OSTMnTSST 为计数启动信号 | ✔ HW-E p.1543；IC0CKSEL 地址 ◐（pp.1557-1560） |
| F-SYS-1 | 时钟控制器寄存器的写保护靠 **Slave Guard**（§31），不是 PROTCMD 式命令 | ✔ HW-E p.471 §12.3.1 |
| F-SYS-2 | HW-E 全文无 `PROTCMD/PROTS` 字样（F1x 系列常见的写保护命令寄存器在 P1M-E 不存在） | ✔ grep 0 命中 |
| F-SYS-3 | 写保护解锁序列（以 CLMA 为例）：①CLMAnPCMD 写 A5H → ②写新值 → ③写取反值 → ④再写新值；失败置 CLMAnPS.CLMAnPRERR；同模块其他寄存器写入或中断访问会导致失败。ECM（§32.3.5 p.2795）、Flash（§35.7.4.1 p.2871）有各自保护序列 | ✔ HW-E p.2764（ECM/Flash 页 ✘ 未逐页核）（**既有 docs 完全缺失**） |
| F-SYS-4 | RESF `0xFFF81000`（32 位只读）：bit0 POR、1 pin、2 CVM、3 SW system、5 ECM system、7 SW application、9 ECM application、10 Field BIST | ✔ HW-E pp.421-422 |
| F-SYS-5 | OPBT0 只读映射 `0xFFCD0030`：OPWDRUN[31]、OPWDOVF[27:25]、OPWDVAC[22]、OPWDMDS[21] | ✔ HW-E p.2884 |

### 4.4 RS-CANFD

| ID | 事实 | 状态 / 来源 |
| --- | --- | --- |
| F-CAN-1 | 外设名 **RS-CANFD**（模块 RSCFD0 / DS 中 “RSCANFD0”），单一基址 `0xFFD20000`，内含 CAN0–2；寄存器前缀 RSCFDnCFD…（FD 接口）/ RSCFDn…（Classical 接口）；引脚名 RSCAN0RXm/TXm | ✔ HW-E p.791 Table 17.5；DS-E p.23 |
| F-CAN-2 | GRMCFG `0xFFD204FC` RCMC(bit0)：0=Classical 接口、1=CAN FD 接口；只能在 global reset 中改；两种接口下 AFL/Rx FIFO/Tx buffer 窗口地址不同（Classical：AFL +0x500、RF +0xE00、TM +0x1000；FD：AFL +0x1000、RF +0x3000、TM +0x4000），且 FD 的 CmDCFG(+0x500+0x20m) 与 Classical AFL 窗口重叠 | ◐ handoff.md:96-108（HW-E pp.796-802、914-920）；build_hardware_index --check 地址自洽 ✔ |
| F-CAN-3 | 位时间公式 `bitrate = fCAN / [divider × (1+TSEG1+TSEG2)]`；寄存器填“物理值−1” | ✔ HW-E p.1092；编码由主机测试与 index assert 双重验证 |
| F-CAN-4 | 范围（Fig.17.17）：Classical TSEG1 4–16、TSEG2 2–8、SJW 1–4、总 8–25 Tq；FD nominal TSEG1 4–128、TSEG2 2–32、SJW 1–32、总 8–161；FD data TSEG1 2–16、TSEG2 2–8、SJW 1–8、总 5–25；图中要求 `TSEG1 > TSEG2 > SJW`，而 p.922 寄存器文字写 “SJW ≤ NTSEG2” —— **手册内部差异** | ✔ HW-E p.1092、p.922 |
| F-CAN-5 | fCAN=40 MHz 参考候选：Classical 500k = div4/15/4/3 → CFG `0x023E0003`；FD nominal 500k = div2/31/8/4 → NCFG `0x071E1801`；FD data 1M = div2/15/4/3 → DCFG `0x023E0001`；采样点均 80% | ✔ 本次手算 + 主机测试 + build_hardware_index.py:152-157 |
| F-CAN-6 | GAFLM：位=1 比较、0 不比较；GAFLIDEM=0 时必须同时令全部 GAFLIDM=0；标准数据帧精确匹配 mask=`0xC00007FF` | ✔ HW-E p.968（mask 值为推导） |
| F-CAN-7 | TMCp、TMSTSp 只能 8 位访问；TMTRF[2:1] 是发送结果；只在 TMTRF=00B 时置 TMTR；TMTRF 写 00B 清除 | ✔ HW-E pp.1018-1021 |
| F-CAN-8 | CAN RAM 复位后硬件初始化 3794 pclk 周期，期间 GSTS.GRAMINIT=1 | ✔ HW-E p.1090 |
| F-CAN-9 | GCTR 复位值 `0x00000005`（GSLPR=1, GMDC=01 → global stop） | ✔ HW-E p.952 |
| F-CAN-10 | RFCCx.RFE、RFIE 修改条件：RFIE 须在 RFE=0 时改；多数字段只在 global reset 改（RFE 单独在 operating 后置 1） | ✔ HW-E p.980（“RFE 必须另一次写”的流程 ◐ handoff.md:218） |
| F-CAN-11 | Rx RAM 预算：Classical ≤192 buffers / 3072 B（16 B/buffer）；FD ≤5376 B | ✔ HW-E p.1097 |
| F-CAN-12 | 普通 FD Tx buffer 最大 20 B payload；TMME=1 merge 模式三合一至 64 B（每通道 6 buffer 成为 merge area） | ✔ HW-E p.1078、pp.1107-1108 |
| F-CAN-13 | 100-pin CAN pin 候选：CAN0 RX P3_7/P2_0/P4_5、TX P3_8/P2_1/P4_6；CAN1 RX P3_12/P2_2/P4_2、TX P3_13/P2_3/P4_3；CAN2 RX P5_6、TX P5_5 | ✔ DS-E p.23（pin 名）；ALT 号（CAN2 RX ALT1、TX ALT6 等）◐ handoff.md:48-58（HW-E pp.151-154 文本表格错位） |
| F-CAN-14 | PIPC=1（直接 I/O 控制）只允许 Table 2.33 列出的 CSIH/CSIG/TSG3 等功能；CAN 不在列 → CAN 引脚 PIPC=0 | ✔ HW-E p.131（表前段已见，CAN 不在其中） |
| F-CAN-15 | PORT 基址 `0xFFC10000`，组步长 0x40；PM/PMC/PFC/PFCE/PFCAE/PIPC 16 位；PINV/PODC/PDSC/PUCC/PODCE 32 位；ALT `[PFCAE,PFCE,PFC]`：ALT1=000…ALT6=101 | ◐ handoff.md:60-80、agent-guide.md:313（HW-E pp.99-125） |

### 4.5 OSTM / WDTA / Flash

| ID | 事实 | 状态 / 来源 |
| --- | --- | --- |
| F-OSTM-1 | 基址 OSTM0 `0xFFDD8000`、OSTM1 `0xFFDD9000`、OSTM3–7 `0xFFD70000 + 0x40×(n−3)` | ✔ HW-E p.1543 |
| F-OSTM-2 | 寄存器：CMP +00(32)、CNT +04(32 RO)、TO +08、TOE +0C、TE +10(RO)、TS +14、TT +18、CTL +20（后 6 个 8 位）；CTL.MD1：0 interval 向下、1 free-running 向上；MD0 启动时中断 | ◐ agent-guide.md:96-105（HW-E pp.1551-1556）；代码与之一致 |
| F-OSTM-3 | Interval 周期 = (CMP+1)/f；80 MHz 1 ms → CMP=79999=0x1387F；32 位 80 MHz 回绕 53.6870912 s；16 位 1 ms OS tick 回绕 65.536 s | ✔ 算术（主机测试覆盖） |
| F-WDT-1 | WDTA0 基址 `0xFFD74000`；WDTE +0、EVAC +4、REF +8（只读）、MD +C，均 8 位；WDTE 写 ACH 触发，写其他值报错 | ✔ HW-E pp.1522, 1527-1531 |
| F-WDT-2 | 溢出周期 `2^(9+OVF)/WDTATCKI`，WDTATCKI 8 MHz（high-speed）或 250 kHz | ◐ agent-guide.md:212（HW-E pp.1534-1535）；时钟 ✔ p.1523、p.469 |
| F-FLS-1 | Data Flash BGO 允许擦写期间从 Code Flash 执行 | ◐ hardware-findings.md:192（HW-E pp.2857、2869） |

### 4.6 AUTOSAR SWS（本地可核）

| ID | 事实 | 状态 / 来源 |
| --- | --- | --- |
| F-SWS-1 | Can_Write（R22-11）：`Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)`，Service ID 0x06，返回 E_OK / E_NOT_OK / CAN_BUSY | ✔ SWS-CAN p.80-81 [SWS_Can_00233] |
| F-SWS-2 | Can_Write 必须保存 swPduHandle 直到调用 CanIf_TxConfirmation | ✔ SWS-CAN [SWS_Can_00276]（txt:2232） |
| F-SWS-3 | TxConfirmation 由 Tx ISR 或 Can_MainFunction_Write（polling）调用 | ✔ SWS-CAN [SWS_Can_00016] |
| F-SWS-4 | HTH 忙时 Can_Write 不取消在途帧、返回 CAN_BUSY | ✔ SWS-CAN [SWS_Can_00213]、[SWS_Can_00214] |
| F-SWS-5 | CAN Driver R22-11 API 全集：Can_Init/DeInit/SetBaudrate/SetControllerMode/GetControllerMode/GetControllerErrorState/DisableControllerInterrupts/EnableControllerInterrupts/CheckWakeup/Write/GetVersionInfo + MainFunction_Write/Read/BusOff/Wakeup/Mode | ✔ SWS-CAN “Service Name” 列表 |
| F-SWS-6 | MCU Driver R24-11 API：Mcu_Init/InitRamSection/InitClock/DistributePllClock/GetPllStatus/GetResetReason/GetResetRawValue/PerformReset/SetMode/GetVersionInfo/GetRamState | ✔ SWS-MCU txt:1276-1924 |
| F-SWS-7 | DCM 必须实现 UDS 0x10（[SWS_Dcm_00250]）；会话切换在发送确认时生效并更新 P2ServerMax/P2*ServerMax（[SWS_Dcm_00311]） | ✔ SWS-DCM txt:7008、7020 —— 可直接支撑 tutorial :399 的 `02 10 03 → 50 03…` 例子 |

---

## 5. Gap List：对照 claude_plan.md “推荐目录结构”（:1762-1897）

种子强度：**S**=可直接拆出主体内容；**P**=有部分素材需大量补写；**—**=无素材。

### 5.1 01-rh850/

| 目标章节 | 种子 | 来源（文件:行） | 主要缺口 |
| --- | --- | --- | --- |
| 01-rh850-overview | S | tutorial :18-53；DS-E p.2-3（F-DEV-*）；agent-guide :29 | 系列/核（G3M/G3MH/G4MH）对比、P1M-E 在家族中的定位 |
| 02-cpu-architecture | S | tutorial :22-46（F-CPU-*） | PSW 位、特权模式、CALLT/SDA/TDA、GHS ABI（✘ 需 G3M SW 手册） |
| 03-memory-map | S | handoff :26-36；hardware-findings :197-211；tutorial :150-163 | ECC 测试区、self/PE1 别名、I/O 区划分（F-MEM-7） |
| 04-startup-process | P | agent-guide G1/G4（:233-266）；tutorial :114-146；handoff :336-353 | 真实 reset 向量/RBASE、`crt0`/`reset.850` 代码路径、SYNCP、lockstep 寄存器初始化（HW-E pp.254-256） |
| 05-linker-script | P | agent-guide A2（:36-38）、G2（:240-252）；tutorial :148-161 | 任何真实 `.ld` 例子；MemMap.h 契约；GHS section 语法 |
| 06-interrupt-exception | S | hardware-findings :247-274；handoff :262-290；agent-guide B1-B4、G3 | F-INT-6（直接向量偏移）、F-INT-7（EIIC 源码）、FE/EI 级区别、OS Cat1/Cat2 包装 |
| 07-clock-system | S | handoff :38-42；hardware-findings :213-223；agent-guide A3/A4 | F-CLK-2（无软件 PLL）、写保护（F-SYS-1~3）、CLMA 时钟监视 |
| 08-peripheral-overview | P | DS-E p.2-3；tutorial :53 | 各外设基址总表、INTC1/INTC2 |

### 5.2 02-autosar-classic/

| 目标章节 | 种子 | 来源 | 主要缺口 |
| --- | --- | --- | --- |
| 01-classic-platform-overview | P | tutorial :55-72 | 无本地 Layered Architecture 文档 |
| 02-layered-architecture | P | tutorial :57, :70-72 | 同上 |
| 03-ecu-startup | P | tutorial :114-146（openAUTOSAR EcuM 观察） | EcuM SWS、StartupOne/Two、BswM（全缺） |
| 04-configuration-arxml | P | tutorial :426-455 | ECUC 参数定义、容器层级示例 |
| 05-generated-code | P | tutorial :428, :455-459 | 生成 C 结构示例（PB/PC/LT） |
| 06-os-task-isr | P | counter-design 全文；tutorial :216-237 | OS SWS 缺；Counter/Alarm/ScheduleTable、Cat1/2 |
| 07-mainfunction-scheduling | P | tutorial :239-245, :403-405 | SchM SWS、exclusive area、MainFunction 周期与超时换算 |

### 5.3 03-mcal/

| 目标章节 | 种子 | 来源 | 主要缺口 |
| --- | --- | --- | --- |
| 01-mcal-overview | P | tutorial :167-192；mcal-reference-guide :9-39 | — |
| 02-mcu-driver | P | mcal-reference-guide R1（:41-56）；F-SWS-6；F-CLK-2；F-SYS-4 | Mcu 配置容器；P1M-E 上 PLL API 的实际含义 |
| 03-port-driver | S | handoff :44-90；agent-guide I2/I3；mcal-reference-guide R2 | Port SWS 缺；JPORT |
| 04-dio-driver | P | mcal-reference-guide R3（:71-89）；PSR 原子写（handoff :65） | Dio SWS 缺 |
| 05-gpt-driver | S（硬件）/ P（AUTOSAR） | Ostm.c 全文；agent-guide C1-C6；mcal-reference-guide R4 | Gpt SWS 缺；通知/ISR 代码 |
| 06-icu-driver | — | 无 | 全缺（P1M-E TAUD/TAUJ 输入捕获） |
| 07-rh850-hardware-mapping | S | hardware-registers.json/csv；handoff 各表 | 汇总表格化 |

### 5.4 04-can-mcal/（第一阶段重点，素材最丰富）

| 目标章节 | 种子 | 来源 |
| --- | --- | --- |
| 01-can-hardware-basics | P | tutorial :196；handoff :90 | 需补 CAN 帧/仲裁/ACK/错误计数基础（无本地 ISO 11898 资料） |
| 02-rh850-can-peripheral | **S** | handoff §4-§5（:92-154）；F-CAN-1/2 |
| 03-can-clock-bit-timing | **S** | handoff §6（:156-178）；hardware-findings §3（:225-245）；Can_BitTiming.c；F-CAN-3~5 |
| 04-can-pin-transceiver | **S** | handoff §3（:44-90）；tutorial §08（:194-214）；agent-guide D1/D2 |
| 05-can-interrupt | **S** | handoff §10（:262-290）；tutorial :357-366；F-INT-10/13 |
| 06-can-controller-init | **S** | handoff §11（:292-311）；tutorial :302-312 |
| 07-hoh-hrh-hth | P | tutorial :249, :374-381；agent-guide D3（:161）| 需补 CanHardwareObject 配置、SWS 图 7-3（SWS-CAN p.80） |
| 08-can-configuration | P | tutorial :442-453 | Can 配置容器（CanController/CanHardwareObject/CanControllerBaudrateConfig） |
| 09-can-init-implementation | P | handoff §11 | Can_Init 代码 |
| 10-can-write-implementation | **S** | handoff §9（:239-260）；tutorial :316-336；F-SWS-1~4 |
| 11-can-rx-implementation | **S** | handoff §7-§8（:180-237）；tutorial :338-355 |
| 12-can-interrupt-implementation | P | handoff :237, :258 | ISR 代码、Can_MainFunction_* |
| 13-can-error-busoff | P | handoff :144-154（BOM、ERFL）；tutorial :259, :493 | CanSM 交互、SWS bus-off 要求 |
| 14-can-driver-from-scratch | P | 全部上述 + Ostm.c/Can_BitTiming.c 作为代码风格样板 | 需新写 Educational Implementation |
| 15-can-driver-debugging | **S** | handoff :387-397；tutorial :481-498；hardware-review :7-25（误区） |

### 5.5 05-can-stack / 06-dcm / 07-rte-swc / 08-integration / 09-real-project-preparation / reference

| 目录 | 种子 | 来源 | 评估 |
| --- | --- | --- | --- |
| 05-can-stack（CanIf/CanTp/PduR/Rx/Tx path） | P（薄） | tutorial :383-395（数据路径表）、:72、:409-424（FD 对各层影响）；openAUTOSAR 观察 :88-91 | **本地无 CanIf/CanTp/PduR SWS**——Phase 1 其他切片需补资料 |
| 06-dcm（14 章） | —（几乎空白） | 仅 tutorial :393, :397-405；SWS-DCM（R20-11）本地可用 | **最大缺口**，与项目最终目标最相关；需从 SWS-DCM 重建 DSL/DSD/DSP、会话、SecurityAccess、DID、Dem 交互 |
| 07-rte-swc（9 章） | — | 无 | 全缺；本地无 RTE SWS |
| 08-integration | P | handoff :378-397（分层验收）；tutorial :461-479（阶段 A-I）、:370-405（0x7E0/0x7DF/0x7E8 示例） | 04-f190-vin-demo、06-canoe-test 全缺 |
| 09-real-project-preparation | P | internal-agent-task :9-22；handoff :399-410（需绑定信息清单）；agent-guide 各“未知→核对方法”段；requirements-extracted 截图问题 | 改写为“到真实项目去哪里找”——这正是这些文档最适合的归宿 |
| reference/ | S | hardware-registers.*（rh850-autosar-mapping）；source-index（source-traceability）；online-references | autosar-api-map、dcm-configuration-map、glossary 全缺 |

---

## 6. rh850-autosar-tutorial-content.md 章节大纲（供重构为 Master Index）

文件现状：532 行，**无 H1**，20 个 H2 + 18 个 H3；含 7 个 HTML 交互占位 `<div>` 与 24 处未定义的 `#ref-rNN` 锚点。

| 行号 | 标题 | 建议去向（目标目录） |
| --- | --- | --- |
| 1 | ## 01 阅读路线与适用边界 {#scope} | 00-learning-roadmap.md（内容标记表 :7-12 可升级为 §17 四标签） |
| 18 | ## 02 RH850 架构：CPU、存储、总线与外设 {#architecture} | 01-rh850/01, 02 |
| 22 | ### CPU 真正在保存什么 | 01-rh850/02-cpu-architecture |
| 40 | ### 锁步、FPU、MPU 与 ECC 各自负责什么 | 01-rh850/02 |
| 47 | ### 字节序和寄存器访问宽度是两件事 | 01-rh850/02（字节序标 ✘ unverified locally） |
| 55 | ## 03 AUTOSAR Classic 分层与最小可运行集合 {#layers} | 02-autosar-classic/01, 02 |
| 61 | ### 按运行目标决定启用模块 | 02-autosar-classic/01 |
| 74 | ## 04 openAUTOSAR 实际提供了什么 {#reference} | 新增 reference/openautosar-gap.md 或并入 Phase-1 openAUTOSAR 研究文件 |
| 95 | ### 两条实现路线，必须选清楚 OS 和接口所有者 | 00-learning-roadmap / 09-real-project-preparation/01 |
| 114 | ## 05 从复位到第一个任务：启动是怎样接起来的 {#startup} | 01-rh850/04 + 02-autosar-classic/03 |
| 148 | ## 06 链接、平台类型、编译器与内存布局 {#memory} | 01-rh850/03, 05 |
| 167 | ## 07 MCAL 要实现哪些模块 {#mcal} | 03-mcal/01 |
| 183 | ### 每个驱动都要有的公共行为 | 03-mcal/01 |
| 194 | ## 08 Port、Dio、收发器：CAN 开始之前 {#pins} | 04-can-mcal/04 + 03-mcal/03, 04 |
| 216 | ## 09 OS、OSTM、中断和 SchM 的连接 {#os} | 02-autosar-classic/06, 07 + 03-mcal/05（**修正 :231 OSTM0/OSTM1 不一致**） |
| 233 | ### CPU 端口和中断包装 | 01-rh850/06 |
| 239 | ### SchM 的两种职责要分开 | 02-autosar-classic/07 |
| 247 | ## 10 CAN driver 必须实现的契约 {#can-driver} | 04-can-mcal/07, 09-12（补 SWS ID，标注 API 版本） |
| 269 | ## 11 P1M-E CAN 硬件配置：先选择模式，再选地址 {#can-config} | 04-can-mcal/02, 03 |
| 294 | ### 接收规则和 FIFO 的完整小配置 | 04-can-mcal/11 |
| 302 | ### 冷启动顺序 | 04-can-mcal/06 |
| 314 | ## 12 发送、接收、完成回调：驱动的核心循环 {#can-io} | 04-can-mcal/10, 11 |
| 316 | ### 发送路径：接受请求与完成请求分开 | 04-can-mcal/10（`CAN_OK`→按版本标注） |
| 338 | ### 接收路径：搬走报文，再弹出硬件 FIFO | 04-can-mcal/11 |
| 357 | ### 中断映射 | 04-can-mcal/05 |
| 370 | ## 13 从 CAN 到 UDS：一个完整配置例子 {#diagnostics} | 05-can-stack/06, 07 + 08-integration/05 |
| 374 | ### 四种编号不要混为一谈 | 04-can-mcal/07-hoh-hrh-hth |
| 383 | ### 必须闭合的数据路径 | 05-can-stack/06-can-rx-path, 07-can-tx-path |
| 397 | ### 一个可抓包验证的单帧示例 | 08-integration/05-uds-end-to-end（补 [SWS_Dcm_00250]/[SWS_Dcm_00311]） |
| 403 | ### 定时配置示例与陷阱 | 02-autosar-classic/07 + 05-can-stack/03 |
| 407 | ## 14 从 Classical 升级到 CAN FD，要改哪些层 {#fd} | 后续阶段（05-can-stack 附录） |
| 426 | ## 15 配置如何变成寄存器：构建与生成闭环 {#configuration} | 02-autosar-classic/04, 05 |
| 442 | ### 配置对象应包含什么 | 04-can-mcal/08 + reference/can-configuration-map |
| 457 | ### 编译成功以后仍要检查什么 | 01-rh850/05 |
| 461 | ## 16 实施与验收：让每一步有可观察结果 {#bringup} | 08-integration/01 |
| 481 | ## 17 常见失败：沿链路定位，而不是反复改波特率 {#debugging} | 04-can-mcal/15 + 08-integration/07 |
| 500 | ## 18 给内部 agent 的实施清单与交付物 {#agent} | 09-real-project-preparation（**去掉“内部 agent”语境**；:511 五问可放各章“常见问题”） |
| 513 | ## 19 源码证据、手册与后续阅读 {#sources} | reference/source-traceability |
| 525 | ## 20 P1M-E 硬件交接全文与寄存器检索 {#hardware-appendix} | 删除占位 div；链接到 reference/ 中的寄存器索引 |

重构建议（与 claude_plan.md:2061-2155 一致）：保留文件名，替换为 `# RH850 + AUTOSAR Classic Learning Guide` + 系统图 + Part I–VIII 链接；现有正文按上表迁出后，此文件不再承载技术正文，避免与各章节重复。

---

## 7. 给后续章节作者的要点（摘要）

1. **硬件事实可信度高**：本次抽查约 40 项 HW-E/DS-E 事实（§4 ✔ 项）全部与手册一致，未发现数值性错误。可放心以 handoff.md / hardware-findings.md 为种子，但引用时改为直接引手册页码。
2. **需修正的具体点**：agent-guide.md:71 的 EI184 措辞；tutorial :231 OSTM0/OSTM1 不一致；tutorial :333/:261 API 返回值未标版本；tutorial 的 24 个悬空锚点与 7 个 HTML 占位；mcal-reference-guide.md:54 应注明 P1M-E 无软件 PLL 寄存器。
3. **既有 docs 的新增事实建议**：直接向量偏移（F-INT-6）、EIIC 源码 0x1000+ch（F-INT-7）、EIC 复位值（F-INT-3）、P1M-E 写保护机制（F-SYS-1~3）、Fig.17.17 的 Classical 范围（F-CAN-4）。
4. **最大空白在 DCM/RTE/SWC/CanIf/CanTp/PduR**，且本地缺对应 SWS；`AUTOSAR_SWS_Diagnostics.pdf` 是 Adaptive Platform，不可用于 Classic DCM。
5. **语境**：所有“内部 agent / 内部工程”叙述须改为“教学项目 + 真实项目需确认”，截图中的 RTA-CAR 12.9.0 / RH850GHS 5.0.39 / 4.2.2 / C:\VMEPS 路径仅作“示例场景”。
6. **运行测试前注意**：`tools/run_host_tests.py` 会覆盖 `artifacts/host-build/results.txt`；只读复核请用 scratch 副本或 `build_hardware_index.py --check`。
