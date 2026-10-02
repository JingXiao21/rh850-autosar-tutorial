/* 目标机移植参考，只提供访问层，不含 reset、链接脚本、OS 或 CAN 初始化。
 * 不加入主机可执行文件；默认构建仅生成一个未链接的语法检查对象。
 * 阅读 README 的目标机前置条件后，才能与真实 RH850 工程集成。 */
#include "rh850_mmio_port.h"
#include "rh850_can_regs.h"
#include <stddef.h>

#if !defined(CAN_IRQ_DEMO_TARGET_PORT)
#error "This MMIO port must only be selected explicitly. Read the target integration contract."
#endif

/* 由所选 RH850 工具链/OS 端口实现，必须包含真实 SYNCP 及编译器排序约束。
 * 不提供空实现，以免把主机编译成功误当成目标端同步已经实现。 */
extern void Rh850_Port_Syncp(void);

static uint32_t target_read32(void *context, uint32_t address)
{
    (void)context;
    return *(volatile const uint32_t *)(uintptr_t)address;
}

static void target_write32(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static void target_syncp(void *context)
{
    (void)context;
    Rh850_Port_Syncp();
}

CanIo CanRx_CreateTargetIo(void)
{
    CanIo io;
    io.context = NULL;
    io.read32 = target_read32;
    io.write32 = target_write32;
    io.syncp = target_syncp;
    return io;
}
