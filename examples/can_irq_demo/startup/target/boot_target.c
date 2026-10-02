/* 从 CRT 完成到 EcuM 的目标端 C 入口参考。不能在 PC 上调用此文件函数。
 * 无真实 BSP/EcuM 的实现时无法链接，避免生成“看似可烧录”的空工程。 */
#include "boot_target.h"

#if !defined(CAN_IRQ_DEMO_TARGET_BOOT)
#error "Select this target entry explicitly after reading startup/README.md."
#endif

/* 对应标准 EcuM API；真实集成工程应包含其发行版的 EcuM.h。 */
extern void EcuM_Init(void);

volatile uint32_t Boot_DataProbe = UINT32_C(0x13579BDF);
volatile uint32_t Boot_BssProbe;
volatile BootTargetStatus Boot_TargetStatus;

static void Boot_Fatal(uint32_t reason)
{
    Boot_TargetStatus.failure = reason;
    Boot_TargetStatus.stage = BOOT_TARGET_FAILED;
    Boot_BspFatal(reason);
    for (;;) { /* BSP 意外返回时也不能继续初始化或标记 OS 接管。 */ }
}

void Boot_EntryC(void)
{
    /* 此函数不是 reset handler！SP/GP/EP、RAM/ECC、CRT 均须先就绪。
     * volatile 探针用于运行时观察；不能证明每个段都已被正确初始化。 */
    Boot_TargetStatus.stage = BOOT_TARGET_CRT_CHECK;
    Boot_TargetStatus.data_probe = Boot_DataProbe;
    Boot_TargetStatus.bss_probe = Boot_BssProbe;
    if (Boot_DataProbe != UINT32_C(0x13579BDF) || Boot_BssProbe != 0u) {
        Boot_Fatal(1u);
    }

    Boot_TargetStatus.stage = BOOT_TARGET_SNAPSHOT;
    /* 原因先保存，后续 Mcu/项目代码才可按其流程清除。这里不写 RESF/OPBT。 */
    Boot_TargetStatus.resf = *(volatile const uint32_t *)(uintptr_t)UINT32_C(0xFFF81000);
    Boot_TargetStatus.opbt0 = *(volatile const uint32_t *)(uintptr_t)UINT32_C(0xFFCD0030);
    Boot_TargetStatus.wdta0md = *(volatile const uint8_t *)(uintptr_t)UINT32_C(0xFFD7400C);
    if (!Boot_BspValidateBeforeEcuM()) {
        Boot_Fatal(2u);
    }

    Boot_TargetStatus.stage = BOOT_TARGET_ENTER_ECUM;
    EcuM_Init(); /* 真实 EcuM 内部完成 StartPreOS，并调用真实 StartOS。 */
    Boot_Fatal(3u); /* 正常路径不返回。调用 EcuM 不等于 OS 已经接管。 */
}
