# RH850/P1M-E MCAL 参考实现：首批基础组件

给内部 agent 的七模块完整设计解说见 [MCAL 参考案例](../../docs/mcal-reference-guide.md)，计数器方案见 [K1 解说](../../docs/counter-design.md)。解说交付已完成；本目录 README 只记录实际 C 代码的实现范围。

目标器件：R7F701381。硬件依据为 Renesas `R01UH0585EJ0120 Rev.1.20`，详见[核查报告](../../docs/hardware-findings.md)。当前代码提供可测试的基础实现；还没有完整 AUTOSAR MCAL API、目标启动工程或可烧录镜像。

## 当前内容

| 文件 | 已实现 | 尚未覆盖 |
| --- | --- | --- |
| `mcal/gpt/Ostm.c` | OSTM0/1 停止状态下配置 PCLK、间隔/自由运行模式、启停、比较值更新、计数读取及精确周期换算 | 完整 Gpt API、通知、EIC/ISR、TAUD/TAUJ 时钟选择和资源仲裁 |
| `mcal/can/Can_BitTiming.c` | P1M-E CAN FD 时序参数检查、速率/采样点计算、NCFG/DCFG 编码，检查两阶段 BRP 相同及 TDC prescaler 限制 | Can 控制器驱动、收发、过滤、CAN 上层回调及网络时序验证 |
| `integration/Tick_Accumulator.c` | 原始 32 位上计数到 16 位/1 ms tick 的连续累计，保留不足 1 ms 的余数 | 完整 `Os_Cbk_Now/Set/State/Cancel`，最长空闲期间维护策略和 OS 并发保护 |
| `platform/Rh850_Mmio.c` | volatile 8/16/32 位硬件访问实现，可用测试模型替换 | GHS 启动、编译器/平台屏障确认、向量表、内存保护和链接脚本 |

Mcu/Port/Dio/Fls/Wdg 仍处于资料核查阶段。以上辅助组件不计作七个模块已全部完成。

## 主机测试

在仓库根目录执行：

```powershell
python tools/run_host_tests.py
```

需要 PATH 上的主机 GCC。脚本使用 C99、`-O2 -Wall -Wextra -Werror -pedantic`，构建产物和完整日志位于 `artifacts/host-build/`。不会调用原生 MMIO 后端，不会访问目标板寄存器。

本次结果：5 组测试通过，包括 100000 次计数采样。检查范围：

- OSTM0/1 的访问宽度、初始化顺序、运行中拒绝重配置、通道隔离和有界停止等待。
- 周期转换的整除约束、零值、32 位比较值上下界和乘法溢出边界。
- CAN 时钟来源、位域编码、两阶段 prescaler、TDC 限制、非法参数和保守 SJW 策略。
- 非零初值、计数余数、硬件 32 位回绕及 OS 16 位回绕，重复读取不推进 tick。

## 目标板集成前置条件

1. 提供现有工程和 RH850GHS 5.0.39 的生成头文件、端口说明，核对当前工具链 ABI 和回调签名。
2. 明确 OSTM0/1 所有权；截图说 Gpt 已用 OSTM0，但 OSTM1 是否空闲仍未知。参考示例没有默认抢占任何通道。
3. `Ostm_InitPclk` 会将该通道切换到 PCLK，因此不得把它直接插入使用 CK0/20 MHz 的现有配置。其调用前必须停表、屏蔽中断、保证 TSST 为低并串行化访问。
4. 建立 EIC/ISR 的初始化、挂起标志操作和临界区。`Ostm_SetCompare` 只写 CMP，不保证已经过去的匹配值能补发中断。
5. `Ostm_Stop` 会使自由运行计数器在下次启动时从 0 重新计数；它不等价于 OS 的 alarm Cancel。
6. `Tick_Accumulator` 要求采样间隔小于 `2^32 / counter_hz` 秒，且所有读写串行化。该模块无法从两次原始计数值判断是否漏过完整一圈；集成层必须保证调用周期，不能仅依赖临时的 `Now` 查询。
7. 核对主机测试没有覆盖的实际硬件时序、ISR 延迟、首次启动周期、外设访问顺序和 GHS 生成代码。

## CAN 候选参数

在 DCS=0、fCAN=40 MHz、CAN FD 接口模式成立时，可调用：

```c
Can_Timing nominal = {2, 31, 8, 4};
Can_Timing data = {2, 15, 4, 3};
Can_FdTimingResult timing;
bool ok = Can_ComputeFdTiming(40000000U, &nominal, &data, false, &timing);
```

结果为 500 kbit/s / 1 Mbit/s、两阶段采样点 80%，NCFG=`0x071E1801`、DCFG=`0x023E0001`。这些只是计算结果；函数不写寄存器。输入为物理分频比和 Tq 数，不能传入已减 1 的寄存器编码。

数据阶段 SJW 暂取 3 是为了同时满足手册图中的严格不等式；寄存器文字允许等号的差异记录在核查报告中。最终配置还需确认适用勘误、总线/收发器延迟、TDC 及当前 MCAL 的参数语义。
