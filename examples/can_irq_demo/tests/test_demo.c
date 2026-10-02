#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static unsigned int passed;
static void pass(const char *name)
{
    printf("PASS %02u %s\n", ++passed, name);
}

int main(void)
{
    Demo demo;
    BusFrame frame = Demo_Frame(0x10u);
    uint32_t first;
    unsigned int i;

    Demo_Init(&demo, false);
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(demo.driver.isr_calls == 0u && demo.hw.count == 1u); /* 硬件先入队 */
    CHECK(SimCpu_RunOne(&demo.cpu));
    CHECK(demo.cpu.eiic == UINT32_C(0x10BE) && demo.cpu.pc == UINT32_C(0x1000));
    CHECK(demo.received_count == 1u && demo.received[0].length == 8u);
    CHECK(demo.received[0].id == UINT32_C(0x7E0) && demo.received[0].hrh == 1u);
    CHECK(demo.received[0].timestamp == 1u && demo.received[0].controller == 0u);
    CHECK(memcmp(demo.received[0].data, frame.data, 8u) == 0);
    CHECK(!SimHw_Irq(&demo.hw) && demo.hw.count == 0u && demo.hw.syncs != 0u);
    CHECK(!SimCpu_RunOne(&demo.cpu));
    pass("frame snapshot, HRH, endian, EIIC and return; no duplicate interrupt");

    Demo_Init(&demo, false);
    frame.id = UINT32_C(0x7E1);
    CHECK(!SimHw_Receive(&demo.hw, &frame));
    frame.id = UINT32_C(0x7E0); frame.extended = true;
    CHECK(!SimHw_Receive(&demo.hw, &frame));
    frame.extended = false; frame.remote = true;
    CHECK(!SimHw_Receive(&demo.hw, &frame));
    CHECK(demo.hw.filtered == 3u && !SimHw_Irq(&demo.hw));
    pass("AFL rejects other IDs, extended IDs and remote frames");

    Demo_Init(&demo, false);
    frame = Demo_Frame(0x20u); frame.protocol_valid = false;
    CHECK(!SimHw_Receive(&demo.hw, &frame));
    CHECK(demo.hw.invalid == 1u && demo.hw.count == 0u);
    pass("protocol-invalid input cannot reach the FIFO");

    Demo_Init(&demo, false); frame = Demo_Frame(0x10u);
    demo.cpu.eic190 |= EIC_EIMK;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK((SimCpu_ReadEic190(&demo.cpu) & EIC_EIRF) != 0u);
    CHECK(!SimCpu_RunOne(&demo.cpu) && demo.hw.count == 1u);
    demo.cpu.eic190 &= (uint16_t)~EIC_EIMK;
    CHECK(SimCpu_RunOne(&demo.cpu) && demo.received_count == 1u);
    pass("EIMK masks delivery, but EIRF and FIFO retain the request");

    Demo_Init(&demo, false);
    demo.cpu.psw_id = true;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(!SimCpu_RunOne(&demo.cpu));
    demo.cpu.psw_id = false;
    CHECK(SimCpu_RunOne(&demo.cpu));
    pass("PSW.ID delays CPU acceptance");

    Demo_Init(&demo, false);
    demo.cpu.pmr = UINT16_C(1) << 5;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(!SimCpu_RunOne(&demo.cpu));
    demo.cpu.pmr = 0u;
    CHECK(SimCpu_RunOne(&demo.cpu));
    pass("priority mask delays acceptance of EIP5");

    Demo_Init(&demo, false);
    SimCpu_BindCan(&demo.cpu, WRONG_EI);
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(SimCpu_RunOne(&demo.cpu));
    CHECK(demo.cpu.default_entries == 1u && demo.driver.isr_calls == 0u);
    CHECK(demo.hw.count == 1u && SimHw_Irq(&demo.hw));
    CHECK(SimCpu_RunOne(&demo.cpu) && demo.cpu.default_entries == 2u);
    SimCpu_BindCan(&demo.cpu, CAN_RX_EI);
    CHECK(SimCpu_RunOne(&demo.cpu) && demo.received_count == 1u);
    pass("wrong EI90 binding leaves level asserted; fixing EI190 recovers");

    Demo_Init(&demo, false);
    demo.cpu.eic190 &= (uint16_t)~EIC_EITB;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(SimCpu_RunOne(&demo.cpu) && demo.received_count == 1u);
    pass("direct-vector model dispatches using the same channel identity");

    Demo_Init(&demo, false);
    CHECK(SimHw_Receive(&demo.hw, &frame));
    first = SimHw_Read32(&demo.hw, REG_RFDF0_0);
    CHECK(first == UINT32_C(0x13121110));
    CHECK(SimHw_Read32(&demo.hw, REG_RFDF0_0) == first && demo.hw.count == 1u);
    SimHw_Write32(&demo.hw, REG_RFPCTR0, UINT32_C(0xFF));
    CHECK(demo.hw.count == 0u && SimHw_Irq(&demo.hw));
    CHECK(SimCpu_RunOne(&demo.cpu) && !SimHw_Irq(&demo.hw));
    CHECK(demo.received_count == 0u);
    pass("reading does not pop; popping does not clear RFIF; empty IRQ is cleared");

    Demo_Init(&demo, false);
    for (i = 0u; i < 9u; ++i) {
        frame = Demo_Frame((uint8_t)i);
        CHECK(SimHw_Receive(&demo.hw, &frame) == (i < 8u));
    }
    CHECK(demo.hw.lost == 1u && demo.hw.count == 8u);
    CHECK((SimHw_Read32(&demo.hw, REG_RFSTS0) & RFSTS_RFFLL) != 0u);
    CHECK(SimCpu_RunOne(&demo.cpu));
    CHECK(demo.received_count == 8u && demo.driver.loss_seen);
    for (i = 0u; i < 8u; ++i) { CHECK(demo.received[i].data[0] == i); }
    CHECK((demo.hw.flags & RFSTS_RFMLT) != 0u && !SimHw_Irq(&demo.hw));
    CHECK(demo.hw.last_status_write == UINT32_C(4));
    pass("burst order, newest-frame loss and W0C preservation of RFMLT");

    Demo_Init(&demo, false); frame = Demo_Frame(0x10u);
    demo.hw.inject_before_clear = true;
    demo.hw.injected_frame = Demo_Frame(0x80u);
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(SimCpu_RunOne(&demo.cpu));
    CHECK(demo.received_count == 2u && demo.received[1].data[0] == 0x80u);
    CHECK(demo.driver.isr_calls == 1u && demo.hw.count == 0u && !SimHw_Irq(&demo.hw));
    pass("arrival immediately before RFIF clear is not stranded");

    Demo_Init(&demo, false); frame.fd = true;
    CHECK(SimHw_Receive(&demo.hw, &frame)); /* IDE/RTR 规则不检查 FDF。 */
    CHECK(SimCpu_RunOne(&demo.cpu));
    CHECK(demo.driver.rejected_format == 1u && demo.received_count == 0u);
    CHECK(demo.hw.count == 0u && !SimHw_Irq(&demo.hw));
    pass("8-byte FD frame is identified from RFFDSTS and not indicated as Classical");

    Demo_Init(&demo, false); frame = Demo_Frame(0x30u); frame.dlc = 15u;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(SimCpu_RunOne(&demo.cpu));
    CHECK(demo.received[0].length == 8u && demo.received[0].data[7] == 0x37u);
    pass("Classical DLC15 is limited to eight data bytes");

    Demo_Init(&demo, false); frame = Demo_Frame(0x40u); frame.dlc = 3u;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(SimHw_Read32(&demo.hw, REG_RFDF0_0) == UINT32_C(0x00424140));
    CHECK(SimCpu_RunOne(&demo.cpu) && demo.received[0].length == 3u);
    CHECK(demo.received[0].data[3] == 0u);
    pass("short payload does not expose bytes beyond DLC");

    Demo_Init(&demo, false); frame.dlc = 0u;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK(SimCpu_RunOne(&demo.cpu) && demo.received[0].length == 0u);
    pass("zero-length data frame is delivered");

    Demo_Init(&demo, false); frame = Demo_Frame(0x50u);
    /* 测试夹具直接设置模型初始条件，绝非演示真机在 RFE=1 时修改 RFIE。 */
    demo.hw.rfcc &= ~RFCC_RFIE;
    CHECK(SimHw_Receive(&demo.hw, &frame));
    CHECK((demo.hw.flags & RFSTS_RFIF) != 0u && !SimHw_Irq(&demo.hw));
    CHECK(!SimCpu_RunOne(&demo.cpu) && demo.hw.count == 1u);
    pass("RFIE disabled: storage and RFIF remain, no interrupt request output");

    printf("ALL %u TESTS PASSED\n", passed);
    return 0;
}
