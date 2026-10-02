# RH850 HEX 烧录前离线检查 harness

本工具检查“给定 HEX/ELF 是否符合公司提供的器件布局和烧录计划”，用于发现型号/链接布局错误、危险地址写入、错误扇区擦除和证据文件失配。**它不连接 ECU、不执行 TRACE32/CMM、不下载、不擦除、不修改 option/security/OTP，也不运行目标应用。**

入口：[tools/flash_preflight.py](../../tools/flash_preflight.py)。只需 Python **3.10+** 标准库，不需要 gcc、RH850 编译器或调试器。

**不要使用我们教学器件 R7F701381 的内存范围替代公司的真实型号。** 本仓库只提供默认不能通过的模板，没有“通用 RH850 安全地址表”。公司 agent 的操作指令见 [AGENT_GUIDE.md](AGENT_GUIDE.md)。

## 1. 如何运行

先在公司本地环境准备同一次构建的 HEX、ELF，以及已核对的器件策略 `profile.json` 和烧录计划 `plan.json`：

```powershell
python -X utf8 tools/flash_preflight.py --hex D:/company/build/app.hex --elf D:/company/build/app.elf --profile D:/company/check/profile.json --plan D:/company/check/plan.json --out-dir D:/company/check/reports
$LASTEXITCODE
```

路径应替换为真实公司文件。工具根据每个 JSON 所在目录解析它引用的证据相对路径；不依赖当前工作目录。每次创建新的 UTC 时间戳目录，保存 `report.json` 和中文 `report.md`，不覆盖旧报告。

如果当前只有 HEX，也可以先运行：

```powershell
python -X utf8 tools/flash_preflight.py --hex D:/company/build/app.hex
```

它会校验 HEX、列出地址范围，但结果为 **BLOCKED**；不能把缺少 ELF/器件配置/计划的检查当作通过。

| 结果 | 退出码 | 解释 |
|---|---|---|
| `PASS_STATIC` | 0 | 支持范围内的静态检查通过；不代表真实 ECU 可安全烧录或恢复 |
| `FAIL` | 1 | 格式、布局、内容、擦除/写入范围或哈希等违反规则 |
| `BLOCKED` | 2 | 缺少输入、仍是模板、证据缺失或 agent 核查未完成 |
| `ERROR` | 3 | I/O 或报告保存失败 |

CI 必须将所有非零退出码视为不能继续。退出码 0 也不能直接触发自动烧录：本工具没有证明所有动态脚本、保护状态和板级条件。

## 2. 自动检查哪些内容？

### HEX 格式和地址

- ASCII、每条记录长度、校验和、EOF 完整性；拒绝 EOF 后还有记录。
- 支持 00/01/02/03/04/05 记录，正确应用扩展线性/段地址。
- 拒绝重复或重叠地址，即使重叠字节一致也不自动接受。
- 拒绝未知记录、重复入口、跨 64 KiB 窗口的单条 data 记录及地址越界；不猜测工具的回绕行为。
- HEX 全部实际字节必须落在公司允许写入范围内，并覆盖完整编程单元和规定的必要入口/向量字节。
- 数据全部为 `FF` 也视为实际 HEX 内容，不能忽略其地址。

### ELF 与 HEX 一致性

仅支持 **ELF32、little-endian、ET_EXEC**。校验 ELF machine 和完整 flags 是否与公司 profile 一致；没有默认猜测公司的 ABI。

使用 `PT_LOAD.p_paddr` 作为 Flash 装载地址，`p_vaddr/p_memsz` 检查运行时 Flash/RAM 布局，`p_filesz` 决定文件中的初始化字节。**公司 agent 必须先核实本项目工具链的 p_paddr 确实代表所需 Flash LMA。** 遇到供应商扩展、RAM-only 文件数据、overlay、不同地址约定或 ELF64 时应保持失败，不能强行修改地址让它通过。

ELF 中的文件数据必须与 HEX 全量逐字节一致；不能只比较入口和文件大小。零文件大小的 BSS 不作为烧录字节，RAM VMA 与 Flash LMA 不同的初始化数据可以被正确识别。入口还必须位于允许的、已加载的 Flash 可执行区域；本版不支持 RAM 执行入口。

`hex_padding` 有两个模式：

- `none`：HEX 字节和 ELF Flash 数据完全一致。
- `ff_to_program_unit`：只允许在**包含 ELF 数据的最小编程单元内部**将缺失字节补为 FF；不允许把任意大间隙或整个 Flash 填满。应由公司布局策略明确选择，不是为通过检查临时开启。

### 擦除计划

Flash 擦除以扇区为单位，写入只有几个字节也可能影响整个扇区。工具使用公司提供的扇区几何信息检查：

1. 实际擦除区必须按扇区对齐，且完全位于允许擦除白名单。
2. 写入、擦除都不得涉及保护区域。
3. 写入的全部字节必须被声明擦除覆盖；本版不猜测现有 Flash 能否免擦直接写。
4. 每个被擦除的字节，必须由 HEX 恢复，或同时被 profile 与本次计划明确允许留空。
5. 本版不接受“工具会替我保存再恢复”作为擦除保护区的理由，也不支持未建模的读改写保留流程。

例子：HEX 只写 `0x2000–0x203F`，但实际擦除 `0x2000–0x20FF`，剩余 `0x2040–0x20FF` 不能默默消失；没有恢复内容或明确的留空允许，就报 `ERASE_COLLATERAL`。这些地址只是数学示例，不对应公司 ECU 的允许区域。

### 计划与证据绑定

严格匹配器件完整料号、silicon revision、板卡标识、工具版本、CPU selection、Flash 算法，以及 HEX/ELF/profile SHA-256。

同时核对实际算法文件、脚本、链接文件、MAP、器件读回记录、工具设置、恢复方案、review 记录的文件哈希。报告区分程序实际检查的 `machine_checks` 和公司 agent 声明的 `agent_attestations`。

**文件哈希只证明它是被引用的那个文件，不证明内容正确。** 本工具不会解析 MAP/linker/CMM 的全部语义，也不能认证一份读回日志或恢复声明的真实性。

## 3. 公司 profile：从器件和公司布局建立规则

复制 [profile.template.json](../../tools/flash_preflight_templates/profile.template.json)，在公司本地保存为 `profile.json`。模板的 `profile_status=template` 一定被阻止；只有真实资料核对完成后才由公司维护者/agent 将其改为 `company_verified`。

不能仅改状态字段，更不能从待检查 HEX 的地址反向“生成允许范围”。那会让错误固件给自己制定通过条件。

JSON 使用严格字段：拼错字段、未知字段、重复 key、空的关键列表、布尔值充当地址均拒绝。地址支持整数或 `0x...` 字符串，**JSON start/end 两端都包含**。

| 字段 | 如何填写 |
|---|---|
| `profile_id` | 公司维护的布局/版本标识 |
| `device` | 完整 `part_number`、实际 `silicon_revision`、含板卡修订/实例标识的 `board_id`，需能与实物及日志对应 |
| `tool` | 已核实的工具名称、版本、CPU selection、算法名称；不假设 CPU 字符串等于 MCU 料号 |
| `layout_evidence` | `{path, sha256}`，记录芯片手册版本/页码、板卡、Flash/RAM、保护区和擦除粒度的核查文件 |
| `algorithm_sha256` | 公司确认匹配该器件的实际 Flash 算法文件哈希 |
| `flash_banks` | 每项 `name/start/end/erase_block_size/program_unit`；同项内粒度均匀，不同粒度拆成不同 bank 段 |
| `ram_ranges` | 本工程允许使用的实际 RAM 范围；不要只写整个 RH850 地址空间 |
| `allowed_program_ranges` | 本次类型的应用镜像允许写入区域 |
| `allowed_erase_ranges` | 可实际擦除的完整扇区区域 |
| `allowed_blank_ranges` | 擦除后可以保持空白的区域；默认空列表，表示不允许附带清空 |
| `protected_ranges` | bootloader、恢复入口、标定/配对/密钥/保留数据、配置/安全/OTP 等禁止触碰的范围 |
| `entry_ranges` | 工程允许的 ELF 程序入口范围；不是任意代码地址 |
| `required_image_ranges` | 该镜像必须携带的入口、向量等字节；应用更新与完整启动镜像的要求不同 |
| `elf_machine/elf_flags` | 由已验证的工具链/ABI 资料确定；不能只复制错误 ELF 的值来让它通过 |
| `hex_padding` | `none` 或经公司确认的 `ff_to_program_unit` |

例如 `flash_banks` 一项的语法形态为：

```json
{
  "name": "某个均匀粒度的 Flash 区段",
  "start": "0x00000000",
  "end": "0x0000FFFF",
  "erase_block_size": 8192,
  "program_unit": 256
}
```

**以上仅是字段语法示例，不能直接用来决定公司芯片的几何信息。** 实际芯片可能有不同容量、扇区、扩展用户区、配置区和地址别名。

保护区不得与允许写入/擦除/留空区重叠。白名单外的所有地址默认禁止，因此即使某安全区没有显式列在保护列表中，也不能因它出现在 HEX 中而自动开放。

## 4. 本次烧录计划 plan

复制 [plan.template.json](../../tools/flash_preflight_templates/plan.template.json)，按真实脚本及工具选项填写。

| 字段 | 必须满足 |
|---|---|
| `device/tool` | 与 profile 完全一致，也与本次实物/会话匹配 |
| `image_sha256/elf_sha256/profile_sha256` | 本次实际三个文件的哈希 |
| `operations` | 本版严格为 `["erase", "program", "verify"]`；有其他操作则不在支持范围内 |
| `erase_mode` | 必须为 `explicit_sectors`，即已经展开并声明实际扇区；不能保留 `unknown/auto/mass` |
| `erase_ranges` | 实际全部擦除范围，包括工具隐式触发的擦除；跨不同几何 bank 要拆开 |
| `program_ranges` | 实际全部写入范围，须与 HEX 数据覆盖范围一致 |
| `blank_after_erase_ranges` | 擦除后不恢复的范围，且必须在 profile 的 `allowed_blank_ranges` 内 |
| `run_after_programming` | 必须为 false；本工具不评价应用运行安全 |
| `review` | 全部项目真实核查后设为 true；未完成则保持 false/BLOCKED |
| `artifacts` | 证据列表；每项为 `role/path/sha256` |

TRACE32 的 `FLASH.AUTO`、`FLASH.ReProgram` 等可能隐式擦除扇区。不能只摘录脚本中显式写了 `FLASH.Erase` 的行。若脚本还会恢复 HEX 以外的数据，当前模型不能假装这些写入不存在，必须保持阻断，使用公司批准且能完整表示的更新流程。

必需的 artifact role：

- `device_readback`：实际器件/板卡识别、必要 option/保护状态的只读核查记录。
- `linker`、`map`：本次构建实际使用的链接配置及 MAP。
- `flash_script`：实际入口脚本。
- `flash_algorithm`：实际算法文件，须同时匹配 profile 的算法哈希。
- `tool_settings`：包括擦除、选项写入、断开动作、自动运行等设置的导出记录。
- `recovery`：公司恢复路径、可用接口、凭据/备份可用性的核查记录；不要把密码或密钥明文写进报告。
- `review`：本次脚本和布局人工/agent 核查结论、依据、未决项。
- `script_dependency`：可重复，用于脚本调用的其他脚本和影响操作的配置文件。完整性由 agent 核查，不由工具自动遍历 CMM。

`all_script_dependencies_listed` 等 review 字段只是 agent 声明。不要把一堆 true 当作替代源文件核查的证据。

`bootloader_integrity_requirements_verified` 要求核实公司的镜像头、CRC/签名、安全启动及版本回退规则。本工具不执行这些专有校验；请在 `review` 证据中记录公司验证工具的结果或说明不适用依据。

Windows 生成单个文件 SHA-256：

```powershell
(Get-FileHash -Algorithm SHA256 -LiteralPath D:/company/build/app.hex).Hash.ToLowerInvariant()
```

profile 内容修改后，其 SHA-256 也会变化，计划必须重新绑定并重新核查。不要在构建进程仍会修改文件时运行检查。公司 CI 应从独立受控位置提供 profile 和 harness，固定它们的哈希；harness 自身指纹也写入报告，避免 agent 为“通过”而修改校验器或策略。

## 5. 失败后怎么办？

| 代码 | 下一步 |
|---|---|
| `WRITE_OUTSIDE_ALLOWLIST` | 对照完整料号、链接器/HEX 地址；不要扩大白名单掩盖错误 |
| `ERASE_OUTSIDE_ALLOWLIST` | 检查整片擦除、错误容量或 bootloader 共扇区问题 |
| `ERASE_COLLATERAL` | 明确扇区内其他数据如何保留；不能默认其可丢弃 |
| `PROGRAM_UNIT` | 核实烧录工具补齐方式、镜像生成策略及真实编程粒度 |
| `HEX_ELF_MISMATCH` | 核实同次构建、LMA、填充和 HEX 转换步骤 |
| `ELF_VMA/ELF_LMA` | 检查 RAM/Flash 范围和工具链地址约定；不能盲改 p_paddr |
| `DEVICE_MISMATCH/TOOL_MISMATCH` | 纠正型号、板卡、CPU 选择或算法版本 |
| `EVIDENCE_HASH/INPUT_HASH` | 输入发生变化，重新生成完整核查记录 |
| `PROFILE_UNVERIFIED/REVIEW_INCOMPLETE` | 完成真实依据和恢复条件核查，不能只改字段 |

遇到工具不支持的合法格式，也保持失败。应先独立验证并扩展 parser、加入对应回归测试，再更新 harness 版本；不能用“忽略错误继续烧录”参数，本工具也没有该参数。

## 6. 测试与明确限制

运行测试：

```powershell
python -X utf8 -m unittest discover -s tests/flash_preflight -v
```

测试在临时目录生成**虚构器件**的 HEX/ELF/计划，包括成功和大量失败用例，不分发可用于 ECU 的示例固件。它们证明解析和检查逻辑在这些情形下正确，不证明公司 profile 正确或真实 ECU 可恢复。

本版不支持自动解读专有 MAP/linker/CMM 语法、不支持 ELF overlay、不验证引脚电气/时钟/机器码、不探测 OTP/认证状态、不验证恢复口可连接，也不提供受保护区更新模式。文件最大 64 MiB，镜像有效负载及 ELF load 数据最大 8 MiB；超出保持阻断，需要受控扩展。

HEX 范围与 ELF 验证不能代替芯片身份核实。公司的实际保护状态或连接条件变化后，即使文件哈希没变，也必须重新获得匹配该 ECU 的核查证据。

## 7. 依据

Intel HEX 记录结构和扩展地址参考 [Arm/Keil 格式说明](https://www.keil.com/support/docs/1584/_hlp_hexfile.htm)。ELF program header、PT_LOAD 和文件/内存大小的区别参考 [System V ABI Program Header](https://refspecs.linuxfoundation.org/elf/gabi4%2B/ch5.pheader.html)；嵌入式 LMA 的具体用法仍需核实公司工具链。

TRACE32 的隐式扇区擦除与恢复行为参考 [Lauterbach FLASH.AUTO 说明](https://support.lauterbach.com/kb/articles/how-can-i-patch-code-located-in-flash-memory) 和 [F 命令参考](https://www2.lauterbach.com/pdf/general_ref_f.pdf)。本工具采用保守策略，不把工具存在恢复功能当成可擦除受保护扇区的保证。
