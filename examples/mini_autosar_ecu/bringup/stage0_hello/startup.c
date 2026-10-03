/* [Educational Implementation] Cortex-M33 startup for STM32L552: vector table + Reset_Handler.
 * Reset sequence: CPU loads MSP from vector[0], PC from vector[1] -> Reset_Handler
 * -> copy .data (FLASH LMA -> RAM VMA) -> zero .bss -> main(). Compare with RH850:
 * docs/01-rh850/04-startup-process.md (RH850 jumps to the reset vector; SP is set by software). */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);
void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

void Default_Handler(void) { for (;;) { } }   /* unexpected exception: stay here for the debugger */

__attribute__((section(".isr_vector"), used))
void (* const g_vectors[16 + 16])(void) = {
    (void (*)(void))&_estack,  /* 0: initial MSP */
    Reset_Handler,             /* 1: Reset */
    Default_Handler,           /* 2: NMI */
    Default_Handler,           /* 3: HardFault */
    Default_Handler,           /* 4: MemManage */
    Default_Handler,           /* 5: BusFault */
    Default_Handler,           /* 6: UsageFault */
    Default_Handler,           /* 7: SecureFault */
    0, 0, 0,                   /* 8-10: reserved */
    Default_Handler,           /* 11: SVCall */
    Default_Handler,           /* 12: DebugMonitor */
    0,                         /* 13: reserved */
    Default_Handler,           /* 14: PendSV */
    SysTick_Handler,           /* 15: SysTick */
    /* 16..: external IRQs (only the first 16 listed in this smoke test) */
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; ) { *dst++ = *src++; }   /* .data copy */
    for (uint32_t *dst = &_sbss; dst < &_ebss; ) { *dst++ = 0u; }        /* .bss clear */
    (void)main();
    for (;;) { }
}
