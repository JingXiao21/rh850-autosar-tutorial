/*
 * startup_vec.c  (target/renode/common)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (the vendor startup code / "Mcal startup" of a real project, cf. docs/01-rh850/04-startup-process.md).
 * Bare-metal startup used ONLY by the Renode bring-up programs in target/renode/{probe,smoke}. It differs from
 * bringup/stage0_hello/startup.c in that the vector table covers all 64 STM32L552 external IRQs, so that
 * FDCAN1_IT0 (IRQ 39, vector index 16+39 = 55) can be routed to a handler. The OS-based images use the startup
 * of agent A (target/stm32l552/startup) instead.
 * Weak aliases: every IRQ handler defaults to Default_Handler; the program overrides what it needs
 * (SysTick_Handler, FDCAN1_IT0_IRQHandler).
 */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);
void Reset_Handler(void);
void Default_Handler(void);

void Default_Handler(void) { for (;;) { } }

#define WEAK_HANDLER(name) void name(void) __attribute__((weak, alias("Default_Handler")))
WEAK_HANDLER(SysTick_Handler);
WEAK_HANDLER(FDCAN1_IT0_IRQHandler);
WEAK_HANDLER(FDCAN1_IT1_IRQHandler);

#define NUM_EXT_IRQ 64u
__attribute__((section(".isr_vector"), used))
void (* const g_vectors[16u + NUM_EXT_IRQ])(void) = {
    (void (*)(void))&_estack, Reset_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    0, 0, 0, Default_Handler, Default_Handler, 0, Default_Handler, SysTick_Handler,
    /* IRQ 0..38 default, 39 = FDCAN1_IT0, 40 = FDCAN1_IT1, rest default (GNU range designators) */
    [16u + 0u ... 16u + 38u] = Default_Handler,
    [16u + 39u] = FDCAN1_IT0_IRQHandler,
    [16u + 40u] = FDCAN1_IT1_IRQHandler,
    [16u + 41u ... 16u + NUM_EXT_IRQ - 1u] = Default_Handler,
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; ) { *dst++ = *src++; }
    for (uint32_t *dst = &_sbss; dst < &_ebss; ) { *dst++ = 0u; }
    (void)main();
    for (;;) { }
}
