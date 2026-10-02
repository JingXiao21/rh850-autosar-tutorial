#include "sim.h"
#include <stdio.h>

int main(void)
{
    Demo demo;
    BusFrame frame = Demo_Frame(0x10u);
    unsigned int i;

    puts("\n=== 1. One frame: hardware FIFO -> EI190 -> wrapper -> ISR -> callback ===");
    Demo_Init(&demo, true);
    (void)SimHw_Receive(&demo.hw, &frame);
    (void)SimCpu_RunOne(&demo.cpu);

    puts("\n=== 2. EIMK blocks CPU entry, not hardware reception ===");
    Demo_Init(&demo, true);
    demo.cpu.eic190 |= EIC_EIMK;
    (void)SimHw_Receive(&demo.hw, &frame);
    (void)SimCpu_RunOne(&demo.cpu);
    puts("[USER] unmask EI190 now");
    demo.cpu.eic190 &= (uint16_t)~EIC_EIMK;
    (void)SimCpu_RunOne(&demo.cpu);

    puts("\n=== 3. Wrong EI90 binding: default handler, then fix EI190 ===");
    Demo_Init(&demo, true);
    SimCpu_BindCan(&demo.cpu, WRONG_EI);
    (void)SimHw_Receive(&demo.hw, &frame);
    (void)SimCpu_RunOne(&demo.cpu);
    SimCpu_BindCan(&demo.cpu, CAN_RX_EI);
    (void)SimCpu_RunOne(&demo.cpu);

    puts("\n=== 4. Arrival during RFIF clear: recheck FIFO, do not strand the frame ===");
    Demo_Init(&demo, true);
    demo.hw.inject_before_clear = true;
    demo.hw.injected_frame = Demo_Frame(0x80u);
    (void)SimHw_Receive(&demo.hw, &frame);
    (void)SimCpu_RunOne(&demo.cpu);

    puts("\n=== 5. Nine arrivals / depth eight: discard newest, preserve RFMLT ===");
    Demo_Init(&demo, true);
    for (i = 0u; i < 9u; ++i) {
        frame = Demo_Frame((uint8_t)i);
        (void)SimHw_Receive(&demo.hw, &frame);
    }
    (void)SimCpu_RunOne(&demo.cpu);
    printf("[RESULT] delivered=%lu, model exact lost=%lu, driver loss_seen=%u\n",
           (unsigned long)demo.driver.delivered, (unsigned long)demo.hw.lost,
           (unsigned int)demo.driver.loss_seen);

    puts("\n=== 6. Direct-vector alternative: EIIC software dispatch ===");
    Demo_Init(&demo, true);
    demo.cpu.eic190 &= (uint16_t)~EIC_EITB;
    (void)SimHw_Receive(&demo.hw, &frame);
    (void)SimCpu_RunOne(&demo.cpu);
    return 0;
}
