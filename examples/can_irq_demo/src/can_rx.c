#include "can_rx.h"
#include "rh850_can_regs.h"
#include <string.h>

void CanRx_Init(CanRxDriver *driver, CanIo io,
                CanRxIndication indication, void *upper_context)
{
    memset(driver, 0, sizeof(*driver));
    driver->io = io;
    driver->indication = indication;
    driver->upper_context = upper_context;
}

void CanRx_Fifo0IsrBody(CanRxDriver *driver)
{
    CanIo *io = &driver->io;
    driver->isr_calls++;

    /* 教学约束：只有 FIFO0 开中断，只有本函数消费 FIFO0，不允许自嵌套。
     * 为展示清标志竞争，这里一直处理到稳定为空；未实现量产 ISR 时间预算。 */
    for (;;) {
        uint32_t status = io->read32(io->context, REG_RFSTS0);
        if ((status & RFSTS_RFMLT) != 0u) {
            driver->loss_seen = true;  /* 保留硬件丢帧标志，另交错误路径处理。 */
        }

        if ((status & RFSTS_RFEMP) == 0u) {
            CanRxFrame frame = {0};
            uint32_t id = io->read32(io->context, REG_RFID0);
            uint32_t ptr = io->read32(io->context, REG_RFPTR0);
            uint32_t fd = io->read32(io->context, REG_RFFDSTS0);
            uint32_t words[2];
            uint8_t dlc = (uint8_t)(ptr >> 28);
            unsigned int i;

            /* 当前槽位只配置了 8 字节，绝不能读取其存储大小以外的寄存器。
             * 整条数据先拷贝到局部变量，再推进硬件 FIFO 读指针。 */
            words[0] = io->read32(io->context, REG_RFDF0_0);
            words[1] = io->read32(io->context, REG_RFDF1_0);
            frame.id = id & UINT32_C(0x7FF);
            frame.hrh = (uint16_t)((ptr >> 16) & UINT32_C(0xFFF));
            frame.timestamp = (uint16_t)ptr;
            frame.controller = 0u;
            /* Classical 的 DLC=9..15 仍只有 8 字节，不能用 DLC 直接越界复制。 */
            frame.length = (dlc > 8u) ? 8u : dlc;
            for (i = 0u; i < frame.length; ++i) {
                frame.data[i] = (uint8_t)(words[i / 4u] >> (8u * (i % 4u)));
            }

            /* HW-E p.983：只能在 RFE=1 且非空时写 FF；CPU 不写硬件写指针。 */
            io->write32(io->context, REG_RFPCTR0, UINT32_C(0xFF));
            if (((fd & RFFDSTS_FDF) != 0u) ||
                ((id & (RFID_IDE | RFID_RTR)) != 0u)) {
                driver->rejected_format++; /* 本课不交付 FD、扩展帧或远程帧。 */
            } else {
                driver->delivered++;
                driver->indication(driver->upper_context, &frame);
            }
            continue;
        }

        /* RFIF 和 RFMLT 均为 W0C：清 RFIF 写 0，保留 RFMLT 写 1。
         * 只写 0x4，保留位写 0；不可 ~RFIF，也不可读改写吞掉并发丢帧事件。 */
        io->write32(io->context, REG_RFSTS0, RFSTS_RFMLT);
        (void)io->read32(io->context, REG_RFSTS0); /* 同一寄存器 dummy read */
        io->syncp(io->context);                  /* 目标机必须是真实 SYNCP */

        /* 空判断与清 RFIF 之间可能又来一帧。即使 RFIF 被刚才的写清掉，
         * RFEMP 仍为 0，本循环也会读到它。不要只凭 RFIF=0 就退出。 */
        status = io->read32(io->context, REG_RFSTS0);
        if ((status & RFSTS_RFMLT) != 0u) {
            driver->loss_seen = true;
        }
        if (((status & RFSTS_RFEMP) != 0u) && ((status & RFSTS_RFIF) == 0u)) {
            return;
        }
    }
}
