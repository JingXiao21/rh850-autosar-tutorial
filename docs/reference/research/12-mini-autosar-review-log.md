# 12 mini_autosar_ecu 与 docs/14 代码走读系列：终审日志

日期：2026-10-03。范围：`examples/mini_autosar_ecu/` 与 `docs/14-mini-autosar-project/`（README + 01..09）。方法：脚本化核对（脚本在会话 scratchpad，不入库）+ 人工抽查。

## 1. 干净重建

`python examples/mini_autosar_ecu/tools/run_mini_autosar.py --clean`，再无参数全量运行两次（含改动后的最终一次）：**`OK=89 FAIL=0 SKIP=0`**（最终运行 2026-10-03 00:43:34）。

## 2. 文档 ↔ 代码

| 项 | 数量 | 结果 |
|---|---|---|
| `path:line` / `:a-b` 引用（含只写 `:行号` 继承上一个文件的） | 1042 | 越界 0；符号抽查 184 条"可疑"，逐条人工看过，均为"引用的符号在描述里而不在被引用的那一行"等误报；唯一真错误是 1 个文件名（见下） |
| 代码围栏内的引文行（1265 行，≥18 字符，排除 mermaid/命令行） | 1265 | 与当前源码 / 日志 / map / `size` `nm` `readelf` `objdump` 输出逐行比对；其余未匹配项均为"文档里注明的实验副本输出"（第 03 / 05 / 08 章的练习、改过配置的运行）或手写示例，不是引文漂移 |
| 日志行时间戳 | 223 | 见 §6 抖动 |
| 日志 / 报告行号引用（`uart_b.log 第 N 行` 等） | 8 | 全部核对；`07` 第 62 行改为"第 62–65 行" |
| 路径存在性（反引号里的文件名） | 全部 | 仅剩 `Can_Hw_Rh850.c`、`Os_Port_Rh850.c`、`Tick_Accumulator.c`（第 09 章"将要新建"的文件）、`p1me_image_check.py`（10-boot-debug/05 的伪代码）等预期不存在项 |

修复：
- `01` §实验 3：`mcal/can/Can_Hw_Sim.c` 不存在，改为 `sim/host/Can_Hw_Sim.c`。
- Renode 时间戳快照刷新（`01` 的 `LightCtl cmd=2` 行与 `t=1500126us`、`06` §6 两个 0x101 收发块与"跨机器时间戳"段落、`07` §4/§5 的 6 行、`09` §3.2 的 summary 摘录）。
- `07` 第 62 行引用精确化。

## 3. 跨章一致性

核对并确认一致：
- **`Rte_Start` 由谁调用**：第 04 章 §4.7 与第 07 章 §5.3 都如实写"RTE SWS 说 EcuM（SWS_Rte_CONSTR_09035 p.805；§4.6.1.2 p.460），BswM SWS 有 `BswMRteStart`（ECUC_BswM_01073 p.169），本项目按 BswM 方案"。04 补充了对 §4.6.1.2 p.460 与 07 §5.3 的交叉引用。
- **OSEK `Schedule` 语义**：只让给严格更高优先级的就绪任务（`Os_Sched_Yield`），与 `Os_Task.c` 的 DEVIATION 注释一致。
- **`SetVoltage` 单位**：微伏（µV），与 `Adc.c`、`DESIGN.md` §11.2/§17、`run_mini_autosar.py:390` 一致（仅第 09 章提及）。
- **FDCAN `RXGFC`**：LSS = bits[20:16]（标准过滤器数），LSE = bits[27:24]；代码 `(nf<<16)|ANFS=2|ANFE=2|RRFS|RRFE` 与第 06 章表格、`Can_Hw_Stm32.c` 头注释一致；gdb 读出 `0x0003002b` 与代码相符。
- **任务名与优先级**：LightEcu：`Task_Init` 10 / `Task_BswMain` 4 / `Task_LightCtl` 3 / `Task_LightAct` 2；SensorEcu：`Task_Init` 10 / `Task_BswMain` 3 / `Task_Swc10ms` 2；与 `gen/*/Os_Cfg.c` 一致（`prio=6` 仅出现在明确标注的实验里）。
- **链条**：README → 01 → … → 09 的 `Next` 全部正确；`Prerequisite` 不与 `Next` 冲突。README 原建议顺序"01→02→04→03"与第 04 章的前置（03）矛盾，改为严格 01→09，快速路线改为"02 对照表 → 03 §3–§5 → 04"。README 阅读表删除"作者：本批 / 另一 agent"一列（过程残留）。
- **链接**：10 个文档里 223 个相对链接（含指向 Part I–XI 的）全部能解析，0 断链。

## 4. 规范引用（R25-11）

对全部 123 处 `SWS_*/ECUC_*` ID 出现做 ID→PDF 页码核对（ID 必须在被引用页上出现），另外手工 grep 了 ≥ 15 条无 ID 的页码引用（RTE p.134/183/590/766/805/806、§4.6.1.2 p.460；Os p.58/79/80/91/109/110；EcuM p.37/38/41–42/112/137/138；BswM p.69/169；Com p.105/107；PduR p.86；CanIf p.85；CAN Driver p.56/74/75）。**检查 ≥ 138 处，错误 0，修改 0**。脚本把"同一行多个页码"误报的 9 条（如 `SWS_Rte_04560` 在 p.134）经人工确认均正确。

## 5. Mermaid

`node check.mjs docs/14-mini-autosar-project`：20 个图块，初始 bad=1（`README.md` 架构图的 `-. 带点的中文标签 .->` 里 `Os_Cfg.c` 的"."触发词法错误），三条虚线边的标签加引号后 **bad=0**。

## 6. 教学代码质量抽查

- 所有手写 `.c/.h`（不含 `gen/`、`tests/`）文件头都有 `[Educational Implementation]` 与真实对应物说明；仅 `target/restbus/Mcu_Cfg.h`、`Port_Cfg.h` 缺标记，已补上。
- 所有 `gen/**/*` 文件头第一段都含 "GENERATED … do not edit"。
- 未发现 TODO/FIXME；`Schedule`、`SetVoltage`、`Rte_Start` 的注释与行为一致。

## 7. 新手路径

README 新增"§0 30 分钟第一次运行"：`setup_toolchains` → `run_mini_autosar`（`OK=89 FAIL=0`）→ 打开 `uart_b.log` → 读 `gen/LightEcu/Rte_Tasks.c` → `gdb_session.txt`（及 `--step gdb` 重跑）→ `analysis/LightEcu.md`，每步给时间、预期现象与下一步该读的章节。

## 8. 遗留事项

1. **Renode 时间戳不是逐位可复现的**：同一份固件连续跑，`TASK_START` 之类的 µs 值会差几到几十 µs（本次一次干净重建与随后的重跑就出现了差异）。`summary.txt` 的 PASS/FAIL 判据是顺序与阈值，不受影响。文档已在 README §0、01、04、06、07、09 加了"看行序，不要逐位比较"的说明；引文里的数字只对"某一次运行"成立，下次重跑后与文档可能差几 µs。
2. 第 03 / 08 / 05 章里的"实验副本输出"（改 `Res_X` 天花板、`_stack_size`、`.mydata` 孤儿段、缺 `Can.h` 等）无法由仓库里的产物复现，只能按文中步骤自行重做。
3. 第 04 章 §7 的带注释 trace 块把"日志 + 源码位置注释"放在同一行，不能逐行与日志文件做自动 diff；已改为靠上面的时间戳脚本核对"日志部分"。
4. 第 09 章 §7 提到的 RH850 目标文件（`Can_Hw_Rh850.c` 等）是移植建议，仓库里并不存在，属预期。
