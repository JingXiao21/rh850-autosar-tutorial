# 资料索引与环境基线

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**，下文“内部 agent 技术解说 / 解说交付”只是旧语境。PDF 来源与哈希记录仍有效；本地 AUTOSAR SWS 的 release 与身份核验见 [研究笔记 02 §0](reference/research/02-autosar-sws-notes.md)，源码追踪见 [source-traceability.md](reference/source-traceability.md)。当前教程入口见 [Master Index](rh850-autosar-tutorial-content.md)。

当前任务是内部 agent 技术解说；本文件中的工程/工具/板卡缺失记录用于限定工程结论，不阻塞解说交付。逐项回答见 [agent-guide.md](agent-guide.md)。

核查日期：2026-10-01。下列本地 PDF 已提取全文并保留 PDF 页号，索引缓存位于 `artifacts/pdf-text/`。原文表格的列位置以 PDF 页面为准，不能仅凭文本提取后的顺序判断。

## 一、硬件资料及适用范围

| ID | 资料 / 本地文件 | 版本 / 页数 | 本项目用途与边界 |
| --- | --- | --- | --- |
| DS-E | [RH850/P1M-E Datasheet](../r01ds0505ed0100-rh850p1m-e.pdf) | R01DS0505ED0100 Rev.1.00，2025-09-30；72 页 | 目标器件 R7F701381 的型号、封装、容量、引脚、电气参数依据 |
| DS-C | [RH850/P1x-C Datasheet](../REN_r01ds0506ed0100-rh850p1x-c_DST_20251218.pdf) | R01DS0506ED0100 Rev.1.00；122 页；正文日期 2025-12-19，与文件名的 20251218 不同 | §1.2 列出 P1M-C/P1H-C/P1H-CE；不包含 R7F701381。用于系列差异对照，不能作为 P1M-E 寄存器/内存配置依据 |
| HW-P1x | [RH850/P1x Hardware](../REN_r01uh0436ej0140-rh850p1x_MAH_20180330_1.pdf) | R01UH0436EJ0140 Rev.1.40，2018-03-30；2730 页 | P1M 对照资料；其 §17 是 RS-CAN，不能替代 P1M-E 的 RS-CANFD 章节 |
| HW-E | [RH850/P1M-E Hardware](../references/downloads/r01uh0585ej0120.pdf) | R01UH0585EJ0120 Rev.1.20，2018-03-23；3121 页 | 本次从 Renesas 官方下载 ZIP 并提取 PDF，作为目标寄存器、时钟、中断、内存布局的主要依据 |

HW-E 官方来源：[P1M-E Hardware Rev.1.20](https://www.renesas.com/en/document/mah/rh850p1m-e-group-users-manual-hardware-rev120)。下载返回 ZIP，内部文件为 `r01uh0585ej0120.pdf`。分发商镜像下载未成功，实际使用的是上述官方 ZIP 内的 PDF。

Renesas 的[技术更新 TN-RH8-B0468A/E 第 3 页](https://www.renesas.com/en/document/tcu/rh850-tsg3-issue-tn-rh8-b0468ae)分别把 R01UH0436EJ0140 对应到 P1M、R01UH0585EJ0120 对应到 P1M-E，进一步确认两份硬件手册的适用范围不同。

| ID | SHA-256 |
| --- | --- |
| DS-E | `71b80cf05abf256f4047c7c2d6fa706438f70440e5e2959f1ce83d18c7822aad` |
| DS-C | `dccf04a8f81fb77aa042b091e6d7324d1c5a37ed49bc7179016f5de834781ec2` |
| HW-P1x | `118cf8ea87a9e1e3b48bef4c1c52736b589f2c02ad84b040da97161f4683555b` |
| HW-E | `aaea89a7f5d9b029776945868d21728465d372223c41db05cbd728a0499a6e34` |

## 二、当前环境

| 检查对象 | 实际结果 | 影响 |
| --- | --- | --- |
| `C:\VMEPS\RTA-SK_VRTA_GCC_SingleCore_12.9.0_R3` | 路径不存在 | 不能读取当前配置、修改主工程、复现原链接错误 |
| `C:\VMEPS\int.bbm.vm-eps.plrp` | 路径不存在 | 无法核对参考工程的真实配置 |
| `C:\VMEPS\RTA-SK_RH850P1M_GH_9.1.1_INTERNAL` | 路径不存在 | 无法比对旧计数器实现及预编译库 |
| `C:\ETAS\RTA-CAR_12.9.0\RTA-OS_12.9.0\Targets\RH850GHS_5.0.39` | 路径不存在 | 四个回调的准确契约、ISR 命名、IPL 映射和 variant 支持仍待验证 |
| `ccrh850` / `gbuild` | PATH 中未找到 | 尚不能交叉编译 RH850 ELF；这不等于已全盘确认没有安装工具 |
| 主机 `gcc` | 可用：`C:\D_disk\software\TDM_GCC\bin\gcc.exe` | 已用于参考代码主机测试；不是 RH850 编译器 |
| Python | 3.12.10 | 用于资料索引和测试构建脚本 |
| PDF 工具 | 项目 `.venv` 安装 PyMuPDF 1.28.2 | 对应版本记录在 `tools/requirements.txt` |
| 板卡、原理图、BOM、调试器连接 | 当前未提供 | 实际引脚、收发器、Option Byte 值及运行测量待补齐 |

## 三、复现索引

在仓库根目录执行 PowerShell：

```powershell
python -m venv .venv
& .\.venv\Scripts\python.exe -m pip install -r tools/requirements.txt
& .\.venv\Scripts\python.exe tools/pdf_extract.py r01ds0505ed0100-rh850p1m-e.pdf REN_r01ds0506ed0100-rh850p1x-c_DST_20251218.pdf REN_r01uh0436ej0140-rh850p1x_MAH_20180330_1.pdf references/downloads/r01uh0585ej0120.pdf
```

主机测试使用 `python tools/run_host_tests.py`，无需 PDF 依赖。当前没有主工程构建日志，不能把参考测试日志当成原 RTA 工程的构建基线。
