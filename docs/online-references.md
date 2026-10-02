# 网上参考项目与文档评估

> **[Legacy 参考资料 — 上一阶段产物，不再维护]** 本文件来自项目上一阶段（“给内部 agent 的技术解说交付”）。**本仓库是个人学习用的教育项目**，文中“内部 agent 解说已交付”等表述只是旧语境，不构成对任何真实项目的指令；网上资源仅作适用性评估与后续核对入口。当前教程入口见 [Master Index](rh850-autosar-tutorial-content.md)。

检索日期：2026-10-01。筛选重点是 P1M-E / R7F701381、可追溯来源，以及与 GHS + RTA-OS 的差异。以下项目仅做适用性评估，本次未导入第三方项目代码。

| 参考资源 | 可参考内容 | 适用性 / 当前获取状态 |
| --- | --- | --- |
| [Renesas P1M-E 硬件手册 Rev.1.20](https://www.renesas.com/en/document/mah/rh850p1m-e-group-users-manual-hardware-rev120) | 寄存器、内存图、中断、OSTM、RS-CANFD、WDTA | 最直接；已下载官方 ZIP 内 PDF，作为本次实现依据 |
| [R7F701381EAFP 官方器件页](https://www.renesas.com/en/products/rh850-p1m-e/part-details/r7f701381eafp) | G3M、FPU、100 引脚、1024 KB Code Flash、32 KB Data Flash、192 KB RAM | 与 DS-E 核对一致；完整实物后缀仍需核对 |
| [Renesas MCAL 官方发布页](https://www.renesas.com/en/software-tool/renesas-mcal) | P1M-E MCAL v4.07.00 的获取入口，模块示例与培训资料 | 优先获取与当前工程版本一致的包；页面提供下载入口，所列 P1M-E 包要求登录，本次未取得包内容 |
| [Renesas 支持人员的 P1M-E SampleApp 说明](https://community.renesas.com/mcu/rh850/f/rh850-rl78f/33645/re-rucg-tool-driver-generation-for-rh850-p1m-e/184266) | `common_family/make/ghs` 下的 `SampleApp.bat`，模块示例源码及生成目录 | 官方支持给出的实例针对 ADC / 701375；可以参考生成流程，不能把示例参数直接当成 701381 或本项目配置 |
| [同一讨论中的版本说明](https://community.renesas.com/mcu/rh850/f/rh850-forum/33645/re-rucg-tool-driver-generation-for-rh850-p1m-e/120024) | 特定 P1M-E 4.07.00 示例的 AUTOSAR 版本参数说明 | 支持人员说明该实例使用 4.0.3；截图称 4.2.2，需以当前实际包 Release Notes/API 头文件确定兼容性 |
| [adityagarg870/renesas-rh850-p1m-embedded-platform](https://github.com/adityagarg870/renesas-rh850-p1m-embedded-platform) | P1M 的启动、时钟、定时、SchM、SPI、收发器分层思路 | 使用 CS+/CC-RH，目标是 P1M；不是 GHS/RTA-OS/P1M-E 的可直接替换 MCAL。[License](https://github.com/adityagarg870/renesas-rh850-p1m-embedded-platform/blob/main/License)限定教育用途并排除商业使用，因此仅作为结构阅读线索 |
| [dinguluer/Renesas_FreeRTOS](https://github.com/dinguluer/Renesas_FreeRTOS) | RH850 上 RTOS 移植、启动及定时器对接的组织方式 | README 指向 F1KM-S1 等平台，不能复制地址/中断到 P1M-E；仓库 API 的顶层 license 字段为空，尚未逐文件确认许可；未导入 |
| [Renesas CC-RH 启动代码示例](https://tool-support.renesas.com/autoupdate/support/onlinehelp/csp/V8.15.00/CS%2B.chm/Compiler-CCRH.chm/Output/ccrh08c0300y.html) | `boot.asm` / `cstart.asm`，异常向量、表参考方式、RAM 初始化的示例结构 | 编译器是 CC-RH；GHS 汇编、段名称、ABI 和栈模型需单独适配 |
| [ETAS 官方 RTA-OS3.0 Reference Guide](https://etas.tech/download-center-files/products_RTA_Software_Products/RTA-OS3.0_Reference_Guide.pdf) | 旧版硬件计数器概念与回调名称的检索线索 | 公开搜索能定位旧文档，但下载本次返回 HTML；未用于确认 5.0.39/12.9.0 的签名和行为 |
| [Renesas G3M 分支指令限制技术更新](https://www.renesas.com/en/document/tcu/additional-restrictions-location-rh850-g3m-branch-instructions-tn-rh8-b0373a) | G3M 适用芯片的指令布局限制 | 列出 P1M-E，加入后续编译器补丁/Release Notes 核查；本次未完成生成指令检查 |

本次找到的最合适路线是：用官方 P1M-E 硬件手册建立参考实现，以匹配版本的官方 MCAL 示例对照生成和集成方式。尚未找到并验证一套可直接替代当前 RTA-CAR 12.9.0 + RH850GHS 5.0.39 工程的公开完整项目。

## 解说补充检索

| 资源 | 已核实信息 | 使用边界 |
| --- | --- | --- |
| [ETAS RH850GHS 5.0.39 安装包页](https://www.etas.com/ww/ko/downloads/software-downloads-overview/rta-os-rh850ghs-product-installer/) | 官方列出 5.0.39、ZIP、6.4 MB、2024-11-27；点击下载跳转 FlexNet 登录 | 找到版本入口不等于已取得包内文档；本次未确认 variant、回调签名或 Interrupt_XXXX 命名 |
| [GHS RH850 工具说明](https://www.ghs.com/products/RH850_development.html) | 列出 gsrec 转 Motorola S-record，以及 ghexfile 十六进制转换工具 | 可回答工具方向；精确命令参数、支持格式和地址选项取自实际安装版本帮助 |
| [AUTOSAR Watchdog Driver R23-11](https://www.autosar.org/fileadmin/standards/R23-11/CP/AUTOSAR_CP_SWS_WatchdogDriver.pdf) | §8.3.3 解释 SetTriggerCondition 毫秒额度与驱动内部换算 | 用于解释触发额度和硬件周期的差别；不冒充本项目 AUTOSAR 4.2.2/厂商实现的完整规范 |

本次内部 agent 解说已交付；网上资源用于支持解释和给出后续核对入口，不要求下载受登录限制的软件包才能完成文档。
