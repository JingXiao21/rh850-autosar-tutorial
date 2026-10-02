#include "sim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void Demo_CanIf_RxIndication(void *context, const CanRxFrame *frame)
{
    Demo *demo = context;
    unsigned int i;
    assert(demo->received_count < SIM_CAPTURE);
    demo->received[demo->received_count++] = *frame; /* 复制，不保留 ISR 栈指针。 */
    Sim_Log(&demo->hw, "[CanIf-like callback] controller=%u HRH=%u id=0x%03lX len=%u",
            (unsigned int)frame->controller, (unsigned int)frame->hrh,
            (unsigned long)frame->id, (unsigned int)frame->length);
    if (demo->hw.trace) {
        printf("[APPLICATION copy] data=");
        for (i = 0u; i < frame->length; ++i) {
            printf("%02X ", (unsigned int)frame->data[i]);
        }
        putchar('\n');
    }
}

void Demo_Init(Demo *demo, bool trace)
{
    CanIo io;
    memset(demo, 0, sizeof(*demo));
    SimHw_Init(&demo->hw, trace);
    /* 仅配置 PC 模型，不是完整 Can_Init！真实模式切换、时钟、引脚见 README。
     * 模型此时认为全局已 reset，省略等待 RAM 初始化与真实模式状态机。 */
    SimHw_Write32(&demo->hw, REG_GRMCFG, 1u);
    SimHw_Write32(&demo->hw, REG_GAFLCFG0, UINT32_C(0x01000000)); /* CAN0 一条规则 */
    SimHw_Write32(&demo->hw, REG_GAFLECTR, AFL_WRITE_ENABLE);   /* page0 写使能 */
    SimHw_Write32(&demo->hw, REG_GAFLID0, UINT32_C(0x7E0));
    SimHw_Write32(&demo->hw, REG_GAFLM0, UINT32_C(0xC00007FF)); /* 比较 IDE/RTR/ID */
    SimHw_Write32(&demo->hw, REG_GAFLP0_0, UINT32_C(0x00010000)); /* label=HRH1 */
    SimHw_Write32(&demo->hw, REG_GAFLP1_0, 1u);                 /* 只去 FIFO0 */
    SimHw_Write32(&demo->hw, REG_GAFLECTR, 0u);
    SimHw_Write32(&demo->hw, REG_RFCC0, RFCC_RFIM | RFCC_DEPTH8 | RFCC_RFIE);
    demo->hw.operating = true; /* 模拟进入全局 operating，不代表真实硬件操作。 */
    SimHw_Write32(&demo->hw, REG_RFCC0, RFCC_RFIM | RFCC_DEPTH8 | RFCC_RFIE | RFCC_RFE);
    io.context = &demo->hw;
    io.read32 = SimHw_Read32;
    io.write32 = SimHw_Write32;
    io.syncp = SimHw_Syncp;
    CanRx_Init(&demo->driver, io, Demo_CanIf_RxIndication, demo);
    SimCpu_Init(&demo->cpu, &demo->hw, &demo->driver);
    Sim_Log(&demo->hw, "[SETUP model] FD register interface, Classical payload; AFL0 -> FIFO0 -> EI190");
}

BusFrame Demo_Frame(uint8_t first_byte)
{
    BusFrame frame = {0};
    unsigned int i;
    frame.id = UINT32_C(0x7E0);
    frame.dlc = 8u;
    frame.protocol_valid = true;
    for (i = 0u; i < 8u; ++i) {
        frame.data[i] = (uint8_t)(first_byte + i);
    }
    return frame;
}
