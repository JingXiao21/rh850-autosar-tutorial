# 给公司内部 agent：HEX 烧录前检查指令

请在公司内部环境执行以下工作。**只做离线分析和公司允许的只读核实，不连接后自动下载、不执行擦除/写入/安全配置、不启动应用。**

目标是检查真实公司 HEX 是否符合实物器件和允许的更新边界。禁止从待测 HEX 反向放宽 profile，禁止修改校验器或忽略非零退出码来获得通过。

## 1. 收集同次构建及真实目标信息

取得完整 MCU 料号、silicon revision、板卡修订和实例标识，核实实物与已获得的只读识别记录一致。不要沿用教学项目 R7F701381 的容量、地址或工具 CPU 名称。

收集实际 HEX、ELF、MAP、链接配置、TRACE32/烧录入口脚本、所有调用依赖、Flash 算法文件及工具设置导出。确认是否有 app-only 更新、bootloader 分区、保留/标定/NvM/配对/密钥区，以及公司允许的恢复方式。

使用公司现有验证流程核实镜像头、CRC/签名、安全启动及版本回退要求，将结果写入 review 证据；本 harness 不替代专有的镜像认证工具。不适用时也应给出依据。

对于不能读取或不能导出的信息，保留缺口并给出 BLOCKED；不要填假值。恢复记录说明凭据/备份的可用性即可，不向本仓库或公开报告复制秘密。

## 2. 建立独立于 HEX 的 profile

根据该器件手册、公司内存分区和已验证烧录流程，填写 `tools/flash_preflight_templates/profile.template.json` 的本地副本。

布局证据要说明每个 Flash/RAM 范围、扇区边界、编程单元、保护区、入口和向量要求的依据。非均匀扇区分成多个均匀 bank 描述。需要排除的保留区不能只写在文字里而未体现在白名单/保护区中。

确认 ELF32 little-endian 的 machine/flags、PT_LOAD 的 p_paddr 是所需 Flash LMA。检查 MAP/linker 中的栈、BSS、数据段、OS 保留区及用户自定义段；程序不会替你解析专有链接器语法。

只有资料实际核对完成，才能将 `profile_status` 设置为 `company_verified`。保存来源说明及 SHA-256。策略与 harness 应来自公司受控基线，而不是为了本次待测镜像临时扩大。

## 3. 审核真实脚本及 GUI 配置，生成 plan

逐项核实：实际 CPU selection 和算法、显式及隐式擦除、所有 program/load 操作、option/security/OTP/ID/复位向量配置操作、断开时动作、下载后自动运行、脚本宏/参数/条件分支/依赖文件。

不能因为 HEX 没有安全区记录就认定脚本不会改安全配置。也不能因为脚本没有显式整片擦除命令，就忽略 FLASH.AUTO/ReProgram 的扇区擦除。

将实际操作规范化到本地 `plan.json`：填写完整擦除范围、全部写入范围和明确允许留空的字节。若工具会恢复 HEX 以外内容、运行任意目标脚本或更新受保护区域，而本版模型无法表达，就保持阻断，不伪造一个更简单的计划。

绑定 HEX/ELF/profile 和每份证据的 SHA-256。`review` 的每个 true 都必须有核查依据；`run_after_programming` 保持 false。本工具只支持受限的 erase/program/verify 计划。

## 4. 运行 harness

```powershell
python -X utf8 tools/flash_preflight.py --hex <实际HEX> --elf <同次ELF> --profile <公司profile.json> --plan <本次plan.json> --out-dir <公司内部报告目录>
```

同时记录退出码。只有 `PASS_STATIC` / 0 表示静态一致性检查通过；仍不能把它写成“ECU 不会损坏”或“保证可以恢复”。任何 FAIL/BLOCKED/ERROR 都不得进入测试烧录步骤。

遇到错误优先修正构建、布局或烧录流程。若怀疑 parser 不支持公司合法格式，提交最小脱敏案例和对应工具链规范，先扩展并验证检查器；不要关闭检查。

## 5. 请返回给用户的结果

请输出：

1. `PASS_STATIC / FAIL / BLOCKED / ERROR` 和退出码。
2. 实际器件、板卡、工具、CPU selection、算法标识。
3. harness、profile、HEX、ELF 的 SHA-256；报告所在路径。
4. HEX 写入范围、完整扇区擦除范围、擦除后留空范围，以及它们与保护区的关系。
5. 程序实际检查通过的项目，与你人工/agent 核查的项目，分别列出。
6. 剩余不确定性：恢复口/认证/备份、动态脚本、硬件状态、应用运行行为。

公司的真实 HEX、算法、脚本、认证信息和报告留在内部环境，不需要推送到这个公开教学仓库。
