# RH850/P1M-E 地址空间、Section 布局、MemMap 与 GHS 链接脚本速查

> 对象器件：R7F701381 = RH850/P1M-E（1 MB Code Flash 型号）
> 硬件依据：HW-E = `r01uh0585ej0120.pdf`（P1M-E User's Manual: Hardware Rev.1.20），页码为 PDF 页码
> 展开讲解：[01-rh850/03-memory-map.md](../01-rh850/03-memory-map.md)、[01-rh850/04-startup-process.md](../01-rh850/04-startup-process.md)、[01-rh850/05-linker-script.md](../01-rh850/05-linker-script.md)、[02-autosar-classic/05-generated-code.md](../02-autosar-classic/05-generated-code.md)
> **限制**：本仓库**没有** GHS 编译器/链接器手册，也**没有** AUTOSAR Memory Mapping SWS。下文的 GHS 语法、MemMap 宏名都标为 `[Conceptual]`，落地前必须对照项目所用的 GHS 版本手册（MULTI *Building Applications for Embedded RH850*）和 AUTOSAR release 确认。

---

## 1. 地址空间总表 `[RH850 Hardware]`

来源：HW-E Table 4.1（p.257）、Figure 4.1（p.259）。

| 起始 | 结束 | 区域 | 大小 | CPU 取指 | DMA | 链接脚本里怎么用 |
|---|---|---|---|---|---|---|
| `0000_0000` | `000F_FFFF` | **Code Flash user area** | 1 MB | ✅ | 读 | 代码、常量、`.data` 初值镜像、向量表（复位入口在 `0000_0000`，p.258） |
| `0100_0000` | `0100_7FFF` | Code Flash extended user area | 32 KB | ✅ | ❌ | 与 user area **不连续**，需单独定义 region；可放校准/标志等，DMA 不可见 |
| `0100_A000` | `0100_BFFF` | ECC test area | 8 KB | — | — | **不进链接脚本** |
| `1000_0000` | `1FFF_FFFF` | H-Bus I/O | 256 MB | — | 数据 | 外设寄存器，不链接 |
| `FEBE_0000` | `FEBF_FFFF` | Local RAM（PE1 别名） | 128 KB | — | ✅ | **与下一行是同一块物理 RAM**，只用于给 DMA/其他 master 的地址，**不要再分配一次** |
| `FEDE_0000` | `FEDF_FFFF` | **Local RAM（self）** | 128 KB | ✅ | ❌ | `.data/.bss/noinit/stack`，最快 |
| `FEEF_8000` | `FEF0_7FFF` | **Global RAM**（Bank A+B 连续） | 64 KB | ✅（1 wait） | ✅ | DMA 缓冲区、被其他 master 访问的数据 |
| `FF00_0000` | `FFFD_FFFF` | P-Bus I/O | ~16 MB | — | 数据 | Port、OSTM、RS-CANFD、WDTA、TAUx、Clock、Reset… |
| `FF20_0000` | `FF20_7FFF` | （P-Bus 内）**Data Flash** | 32 KB | — | 读 | **不进链接脚本**，由 Fls/Fee 管理 |
| `FFC0_A000` | `FFC0_A00F` | Backup Register BRAMDAT0–3 | 16 B | — | — | 任何复位都不清（p.2891），存复位原因 + 魔术字 |
| `FFFE_E000` | `FFFE_FFFF` | LPB self（INTC1、SEG、PEG、IPG） | 8 KB | — | ❌ | 仅 PE1 可访问，OS/中断配置使用 |
| `FFFF_5000` | `FFFF_FFFF` | P-Bus I/O（INTC2、DMAC、DTS…） | 44 KB | — | 数据 | 外设寄存器 |

三条硬规则（p.258–259）：

1. 能执行代码的只有 **Code Flash、Local RAM self、Global RAM**。
2. DMA 看不到 `FEDE_xxxx`，DMA 地址必须用 `FEBE_xxxx` 别名或放到 Global RAM。
3. Reserved / 手册未列出的 I/O 地址，访问结果 “operation is not guaranteed”（p.257）。

所有 RAM（除 I-Cache、Emulation RAM）带 ECC；LRAM/GRAM/DTS RAM/CSIH RAM 在复位时由**硬件清零并写好 ECC**（p.2890），可用 `STAC_LM0`（`FFF8_1520H`）、`STAC_GRAM`（`FFF8_1420H`）等对特定复位类型关闭（p.420、p.434）。Power On Reset 总是清零。

---

## 2. 一个合理的 P1M-E 项目 Section 布局 `[Conceptual]`

下面是**单应用、无 bootloader** 的推荐布局。地址边界全部来自 HW-E Table 4.1；内部切分是建议值，真实项目以 bootloader、OS、安全分区需求为准。

### 2.1 Code Flash（`0000_0000`–`000F_FFFF`）

| 地址（建议） | Section | 内容 | 依据 / 约束 |
|---|---|---|---|
| `0000_0000`–`0000_01FF` | `.reset` / `.exvect` | 复位入口 + 直接向量（FENMI `+0E0`、FEINT `+0F0`、EIINT `+100…+1F0`） | RBASE/EBASE 低 9 位为 0 → **512 B 对齐**（p.205）；向量偏移见 [06-interrupt-exception.md](../01-rh850/06-interrupt-exception.md) |
| `0000_0200`–`0000_07FF` | `.intvect` | 表引用方式的中断地址表，384 × 4 B = 1536 B | INTBP 低 9 位为 0 → **512 B 对齐**（p.206） |
| `0000_0800`… | `.text` | 启动代码、OS、MCAL、BSW、RTE、SWC 代码 | |
| … | `.rodata` / `.CONST_*` | `const`、字符串、`*_PBcfg.c` / `*_Lcfg.c` 配置表 | |
| … | `.ROM.data`、`.ROM.sdata`、`.ROM.ramfunc` | `.data`、`.sdata`、RAM 函数的**初值镜像（LMA）** | 启动代码复制到 RAM |
| … | `.secinfo`、`.fixaddr`、`.fixtype`、`.syscall` | GHS 运行库/启动代码需要的表（复制表、清零表等） | 名字与是否必需**以 GHS 手册为准** |
| 末尾 | （可选）应用校验区 | CRC、版本号、有效标志 | 由 bootloader 方案决定 |

**有 bootloader 时**：一般是 `0000_0000` 起放 bootloader（自带向量），应用从某个扇区边界开始，应用自己的 EBASE/INTBP 指向应用区。分界地址、应用头格式**需在真实项目确认**；分界必须对齐 Code Flash 擦除块（块大小见 HW-E §35，需逐型号确认）。

### 2.2 Local RAM self（`FEDE_0000`–`FEDF_FFFF`，128 KB）

推荐把**栈放在 RAM 低端**：RH850 栈向下增长，溢出时冲向低地址（区外 reserved 区或 MPU 保护区），而不是悄悄踩坏 `.bss`。

| 地址（建议） | Section | 内容 | 启动时处理 |
|---|---|---|---|
| `FEDE_0000`–`FEDE_27FF` | `.stack`（系统/启动栈，10 KB） | 启动栈、ISR 栈（若 OS 用独立栈则另配） | 不需要初始化 |
| 其后 | OS 任务栈 / OS-Application 分区数据 | 由 RTA-OS 等生成 | 按 OS 要求 |
| 其后 | `.tdata` / `.sdata` / `.data` | 有初值变量（VAR_INIT） | 从 `.ROM.*` 复制 |
| 其后 | `.tbss` / `.sbss` / `.bss` | 零初始化变量（VAR_CLEARED） | 清零（硬件已清，软件仍应清，防止关闭 STAC 后失效） |
| 其后 | `.ramfunc` | Flash 自编程函数等（从 RAM 执行） | 从 Flash 复制；**末尾多留 48 B 并初始化**（预取，p.256） |
| 高端固定处，如 `FEDF_F000`–`FEDF_FFFF` | `.noinit` / `.bss.noinit` | 跨复位保留数据（复位计数、错误日志） | **不复制、不清零**；配合 STAC_* + 魔术字/CRC |

> 旧截图例子把 10 KB 栈放在 `FEDF_D800`–`FEDF_FFFF`（RAM 高端）。这也能工作，但溢出时会直接覆盖紧邻的数据段，排查困难。两种放法都要配合 OS stack monitoring 或 MPU 区域。

### 2.3 Global RAM（`FEEF_8000`–`FEF0_7FFF`，64 KB）

| Section | 内容 |
|---|---|
| `.gram_dma` / `.bss.DMA` | DMA 源/目标缓冲区（ADC、SPI 等） |
| `.gram_shared` | 需要被其他 bus master 访问的数据 |
| （可选）`.gram_ramfunc` | 慢一些，但也可执行代码 |

RS-CANFD 报文在外设自带的 CAN RAM 里（p.2889），**CAN 驱动通常不需要 Global RAM 缓冲区**。

### 2.4 不进链接脚本的区域

Data Flash（Fee/Fls 管理）、所有 I/O 区、ECC test area、Option Bytes（通过烧录工具/Flash 序列设置，不在 CPU 地址表中，p.2881）、BRAMDAT（用寄存器定义直接访问）。

---

## 3. 哪些区域写入有风险

| 风险等级 | 区域 / 寄存器 | 后果 | 依据 |
|---|---|---|---|
| 🔴 可能“锁死”或需要重新烧录 | **Option Bytes**（OPBT0/OPBT2） | 决定 WDTA0 启动方式/使能/溢出时间、JP0 端口功能等；JP0 也是调试器和 Flash 编程接口引脚（DCUTCK/LPD/FPDT 等，p.72、p.87）。配置错误可能导致上电即看门狗复位、调试器连不上 | p.2881–2886 |
| 🔴 | **Code Flash 自编程**（FACI 命令区 + FLMDCNT） | 擦写正在执行的扇区会使程序跑飞；编程代码必须先复制到 Local RAM 执行；FLMD0 高电平才允许擦写（这本身是防误写设计）；写完后要清 I-Cache | p.2869–2870、p.255 |
| 🔴 | bootloader 区 / 向量区 / 应用校验区 | 下一次复位无法启动 | 项目约定 |
| 🟠 只能写一次（Power On Reset 后） | `WDTA0MD`（WDTAnMD）、`CVMDEW` | 写错后直到下次上电都改不回来；WDTA 配错 → 不断复位 | p.1531、p.1535、p.445、p.449 |
| 🟠 带解锁序列的保护寄存器 | CLMA（`CLMAnPCMD` 写 `A5H`…）、ECM（`ECMPCMD*`）、FLMDCNT（`FLMDPCMD`） | 序列错误 → 写入无效并置 PRERR；CLMA 使能后 CLME 只能由复位清除（p.2759） | p.2764、p.2795、p.2870 |
| 🟠 | Slave Guard / P-Bus Guard 保护的时钟、复位寄存器 | 写入被拦截并产生 guard 错误 | p.471（时钟）、p.420（复位） |
| 🟠 | LPB 区 INTC1（EIC0–31）、INTC2 EIC、INTBP | 改变中断优先级/屏蔽或向量 → OS 调度错乱；这些通常归 OS 所有 | p.265、p.206 |
| 🟠 | Data Flash（`FF20_xxxx`） | 绕过 Fee 直接擦写会破坏 NvM block 和 Fee 管理信息 | 架构约束 |
| 🟡 静默数据破坏 | 栈溢出（Local RAM） | 覆盖 `.bss`/OS 数据，症状随机 | 需 OS stack check / MPU（每 PE 16 区 MPU，p.189） |
| 🟡 | `FEBE_xxxx` 与 `FEDE_xxxx` 被当成两块 RAM 分配 | 同一物理字节被两个变量使用 | p.259 |
| 🟡 | DMA 目标用了 `FEDE_xxxx` | DMA 访问违规 | p.259 |
| 🟡 | noinit 区被启动代码清零 / 关闭 STAC 后未初始化 RAM 被读 | 保留数据丢失 / ECC 错误 | p.2890、p.434 |
| 🟡 | RS-CANFD 在 `GSTS.GRAMINIT=1` 时配置 | CAN RAM 尚未初始化，配置无效 | p.1090、p.821 |
| 🟡 | Reserved / 未列出地址 | 行为不保证 | p.257 |

---

## 4. Classic AUTOSAR MemMap 应该怎么做

### 4.1 机制 `[AUTOSAR Standard]`（宏名按公认 R4.x 约定，需以项目 release 的 MemMap SWS 确认）

```text
模块源码里的 START/STOP 宏
   → #include "<Msn>_MemMap.h"
   → MemMap 头文件把宏翻译成 GHS #pragma
   → 目标文件里出现带模块名的 section
   → 链接脚本把这些 section 放进 FLASH / LRAM / GRAM / NOINIT 区
```

模块源码（BSW 厂商提供，**集成者不改**）：

```c
/* [AUTOSAR API] 形态示例 */
#define CAN_START_SEC_VAR_CLEARED_8
#include "Can_MemMap.h"
static uint8 Can_ControllerState[CAN_CONTROLLER_CNT];
#define CAN_STOP_SEC_VAR_CLEARED_8
#include "Can_MemMap.h"

#define CAN_START_SEC_CODE
#include "Can_MemMap.h"
void Can_MainFunction_Write(void) { /* ... */ }
#define CAN_STOP_SEC_CODE
#include "Can_MemMap.h"
```

常用分类（R4.1 以后的命名；R4.0 用 `8BIT/16BIT/32BIT`、`NOINIT`、`ZERO_INIT` 等旧名，升级时要注意）：

| 宏分类 | 含义 | 放到哪里 |
|---|---|---|
| `_CODE` | 普通代码 | Flash `.text` |
| `_CODE_FAST` / `_CALLOUT_CODE` | 需要快速执行的代码 / callout | Flash（或 RAM function） |
| `_CONST_<ALIGN>` | 常量、配置 | Flash `.rodata` |
| `_CONFIG_DATA_<ALIGN>` | post-build 配置 | Flash 的独立区（便于单独替换） |
| `_VAR_INIT_<ALIGN>` | 有初值变量 | LRAM `.data`（Flash 中有镜像） |
| `_VAR_CLEARED_<ALIGN>` | 启动时清零 | LRAM `.bss` |
| `_VAR_NO_INIT_<ALIGN>` | 不初始化 | LRAM `.bss.noinit`（启动不清） |
| `_VAR_POWER_ON_INIT/CLEARED_<ALIGN>` | 只在上电时初始化 | 与 STAC_* 配合的 noinit 区 |
| `_VAR_FAST_*` | 高频访问变量 | LRAM 或 SDA/TDA |

`<ALIGN>` = `BOOLEAN` / `8` / `16` / `32` / `PTR` / `UNSPECIFIED`。按对齐分开的目的，是让链接器把相同对齐的变量放在一起，减少填充浪费。

### 4.2 集成者写的 `<Msn>_MemMap.h`（GHS 版） `[Conceptual]`

```c
/* Can_MemMap.h —— 集成者负责；GHS pragma 语法需按 GHS 手册确认 */
#define MEMMAP_ERROR

#if defined(CAN_START_SEC_CODE)
  #undef  CAN_START_SEC_CODE
  #undef  MEMMAP_ERROR
  #pragma ghs section text=".text.CAN"
#elif defined(CAN_STOP_SEC_CODE)
  #undef  CAN_STOP_SEC_CODE
  #undef  MEMMAP_ERROR
  #pragma ghs section text=default

#elif defined(CAN_START_SEC_VAR_CLEARED_8)
  #undef  CAN_START_SEC_VAR_CLEARED_8
  #undef  MEMMAP_ERROR
  #pragma ghs section bss=".bss.CAN_VAR_CLEARED_8"
#elif defined(CAN_STOP_SEC_VAR_CLEARED_8)
  #undef  CAN_STOP_SEC_VAR_CLEARED_8
  #undef  MEMMAP_ERROR
  #pragma ghs section bss=default

#elif defined(CAN_START_SEC_VAR_NO_INIT_32)
  #undef  CAN_START_SEC_VAR_NO_INIT_32
  #undef  MEMMAP_ERROR
  #pragma ghs section bss=".bss.noinit.CAN"
#elif defined(CAN_STOP_SEC_VAR_NO_INIT_32)
  #undef  CAN_STOP_SEC_VAR_NO_INIT_32
  #undef  MEMMAP_ERROR
  #pragma ghs section bss=default

#elif defined(CAN_START_SEC_CONST_UNSPECIFIED)
  #undef  CAN_START_SEC_CONST_UNSPECIFIED
  #undef  MEMMAP_ERROR
  #pragma ghs section rodata=".rodata.CAN"
#elif defined(CAN_STOP_SEC_CONST_UNSPECIFIED)
  #undef  CAN_STOP_SEC_CONST_UNSPECIFIED
  #undef  MEMMAP_ERROR
  #pragma ghs section rodata=default
#endif

#if defined(MEMMAP_ERROR)
  #error "Can_MemMap.h: unknown or unbalanced memory section"
#endif
```

### 4.3 MemMap 实践要点 `[Real Project Consideration]`

1. **每个模块一个 `<Msn>_MemMap.h`**（Can、CanIf、Dcm、Rte、每个 SWC…），由一个公共的 `Compiler_Cfg`/MemMap 模板生成或包含。厂商 MCAL 通常自带 MemMap 头，但其中的 pragma 是集成者要检查的。
2. **`MEMMAP_ERROR` 守卫必须保留**：START/STOP 不配对、宏名拼错时编译直接报错，否则变量会悄悄落进默认段。
3. **section 名要能被链接脚本通配**：例如统一用 `.bss.<MODULE>_*`、`.bss.noinit.*`，链接脚本就能用 `*(.bss.noinit.*)` 一次收集。
4. **noinit 三件套必须一致**：MemMap 把变量放进 noinit 段 → 链接脚本给该段 `NOCLEAR` 且不在清零表里 → STAC_* 对需要保留的复位类型关闭硬件清零。
5. **安全分区（OS-Application / ASIL 分区）**：每个分区的变量用独立 section 名，链接脚本把它们放进按 MPU 粒度对齐的连续区域，OS 再用 MPU 保护。
6. **map 文件检查**：每次集成后检查 `.bss`/`.data` 里是否有不带模块前缀的“野变量”，那通常是 MemMap 漏包。
7. **DCM 升级时**：新版本 DCM 可能新增或改名 section 宏（如 R4.0 → R4.x 的命名变化），必须同步更新 `Dcm_MemMap.h` 和链接脚本，否则编译报 `MEMMAP_ERROR` 或变量放错区。

---

## 5. GHS 链接脚本（`.ld`）应该怎么写 `[Conceptual]`

**这份脚本没有在任何 GHS 版本上验证过。** 地址来自 HW-E Table 4.1；关键字（`MEMORY`、`SECTIONS`、`> region`、`ROM()`、`ALIGN()`、`PAD()`、`NOCLEAR`、`CLEAR`）按 GHS 链接器常见写法给出，落地时以项目 GHS 版本的手册和厂商/OS 提供的示例 `.ld` 为准。

```ld
/* rh850_p1me_app.ld —— [Conceptual] 教学示意，非 production */

DEFAULTS {
    stack_reserve = 10K
}

MEMORY {
    iROM      : ORIGIN = 0x00000000, LENGTH = 1024K   /* Code Flash user area (p.257) */
    iROM_EXT  : ORIGIN = 0x01000000, LENGTH = 32K     /* Code Flash extended area, DMA 不可见 */
    iRAM      : ORIGIN = 0xFEDE0000, LENGTH = 124K    /* Local RAM self (栈在低端) */
    iRAM_NI   : ORIGIN = 0xFEDFF000, LENGTH = 4K      /* Local RAM 高端: noinit */
    gRAM      : ORIGIN = 0xFEEF8000, LENGTH = 64K     /* Global RAM A+B */
    /* 不定义 FEBE_0000 别名区, 不定义 Data Flash / I/O 区 */
}

SECTIONS {
    /* ---------- Code Flash ---------- */
    .reset          ALIGN(512)          : > iROM      /* 复位入口 + EBASE 直接向量 (p.205) */
    .intvect        ALIGN(512)          : > .         /* INTBP 地址表 384 x 4B (p.206) */
    .text                               : > .
    .text.CAN                           : > .         /* MemMap 产生的模块代码段 */
    .text.DCM                           : > .
    .rodata                             : > .
    .rodata.CAN                         : > .
    .rodata.DCM                         : > .
    .secinfo                            : > .         /* GHS 复制/清零表 (以手册为准) */
    .fixaddr                            : > .
    .fixtype                            : > .
    .syscall                            : > .
    .ROM.tdata      ROM(.tdata)         : > .         /* 初值镜像 (LMA) */
    .ROM.sdata      ROM(.sdata)         : > .
    .ROM.data       ROM(.data)          : > .
    .ROM.ramfunc    ROM(.ramfunc)       : > .

    .calib          ALIGN(4)            : > iROM_EXT  /* 可选: 校准/有效标志 */

    /* ---------- Local RAM self ---------- */
    .stack          ALIGN(8) PAD(stack_reserve) : > iRAM  /* 低端, 向下增长 */
    .tdata          ALIGN(4)            : > .         /* EP(r30) 相对 */
    .tbss           ALIGN(4) CLEAR      : > .
    .sdata          ALIGN(4)            : > .         /* GP(r4) 相对 */
    .sbss           ALIGN(4) CLEAR      : > .
    .data           ALIGN(4)            : > .
    .bss            ALIGN(4) CLEAR      : > .         /* 收集 .bss.* (除 noinit) */
    .ramfunc        ALIGN(4) PAD(48)    : > .         /* 预取保护 48B (p.256) */

    .bss.noinit     ALIGN(4) NOCLEAR    : > iRAM_NI   /* 收集 .bss.noinit.* ; 启动不清 */

    /* ---------- Global RAM ---------- */
    .gram_dma       ALIGN(32) CLEAR     : > gRAM      /* DMA 缓冲 (p.259) */
    .gram_shared    ALIGN(4)  CLEAR     : > .

    /* ---------- 启动代码使用的符号 (名称以 GHS crt0 / 项目启动文件为准) ---------- */
    __stack_bottom = addr(.stack);
    __stack_top    = endaddr(.stack);         /* r3 初值 */
    __gp           = addr(.sdata) + 0x8000;   /* r4 初值, SDA 基址 */
    __ep           = addr(.tdata);            /* r30 初值, TDA 基址 */
    __ghs_ramfunc_end = endaddr(.ramfunc);
}
```

### 5.1 写链接脚本时逐项检查

| 检查项 | 原因 |
|---|---|
| `iROM` 长度是 1024K，不是 2048K | R7F701381 是 1 MB 型号（DS-E p.2）；旧截图写成 2048K 是错的 |
| `.reset`、`.intvect` 512 B 对齐，地址与 RBASE/EBASE/INTBP 设置一致 | p.205–206 |
| 所有 `.data` 类段都有对应 `ROM()` 镜像，并在 GHS 复制表中 | 否则变量初值全是 0 |
| `.bss` 类段带 `CLEAR`，noinit 段带 `NOCLEAR` 且不在清零表中 | 否则 noinit 被清，或 `.bss` 有随机值 |
| GP/EP 符号与编译选项（`-sda`、`-tda`、`-large_sda` 等）一致 | 不一致时所有 SDA/TDA 访问都读错地址；选项含义以 GHS 手册为准 |
| 没有任何段落在 `FEBE_xxxx` 别名区 | 与 `FEDE_xxxx` 是同一 RAM（p.259） |
| DMA 缓冲在 `gRAM` | DMA 看不到 `FEDE_xxxx`（p.259） |
| RAM 代码末尾留 48 B 并初始化 | 预取可能读未初始化 RAM 触发 ECC（p.256） |
| OS 生成的段（任务栈、OS 数据、分区）已放置且对齐满足 MPU 要求 | RTA-OS 等 OS 会给出它要求的段名，**需在真实项目确认** |
| map 文件里没有落进默认段的“野”变量、没有未放置的 orphan section | MemMap 漏包或链接脚本漏写 |

### 5.2 启动代码与链接脚本的配合（顺序）

```text
Reset (0000_0000)
 → 给所有 GPR 赋定值（lock-step: 读未定义寄存器会触发比较错误, p.250）
 → r3 = __stack_top, r4 = __gp, r30 = __ep
 → 按 GHS 复制表把 .ROM.* 复制到 .tdata/.sdata/.data/.ramfunc
 → 按清零表清 .tbss/.sbss/.bss/.gram_*（跳过 .bss.noinit）
 → 检查 noinit 区魔术字/CRC，读取 RESF/BRAMDAT
 → 设置 EBASE/INTBP
 → main() → EcuM_Init() → Mcu_Init / Port_Init / Can_Init … → StartOS
```

详细过程见 [04-startup-process.md](../01-rh850/04-startup-process.md)。

---

## 6. 需要在真实项目确认的事项

| 项目 | 去哪里找 |
|---|---|
| GHS 版本、链接器关键字、`.secinfo` 等必需段、crt0 符号名 | GHS 安装目录手册、项目 `.ld` 与 crt0/startup 源码 |
| 编译选项（`-sda`、`-large_sda`、`-tda`、`-zda`）的实际含义 | GHS 编译器手册、构建脚本 |
| bootloader 分界、应用头、校验区格式 | bootloader 规范与 bootloader 的 `.ld` |
| RTA-OS 要求的段名、栈布局、MPU 分区 | RTA-OS 端口文档与生成的 OS 链接片段 |
| MCAL/BSW 自带的 MemMap 头与段名 | 各模块 `*_MemMap.h`、集成手册 |
| Code Flash 擦除块大小与扇区边界 | HW-E §35 及 Flash Memory 手册 |
| Option Bytes 当前值 | 烧录工具读出，或启动时读 OPBT0（p.434 说明读取时机） |
