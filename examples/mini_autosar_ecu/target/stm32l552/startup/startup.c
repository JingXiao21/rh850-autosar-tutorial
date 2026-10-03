/*
 * startup.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the MCU start-up code (Mcu_Startup / "Startup.s" + vector table of the MCAL or
 * compiler vendor; EcuM only takes over afterwards in EcuM_Init, see AUTOSAR_CP_SWS_ECUStateManager 7.3).
 * Implemented: Cortex-M33 vector table for ALL STM32L552 interrupts, Reset_Handler (VTOR, .data copy, .bss
 * clear, jump to main).  The OS port provides the real handlers (PendSV_Handler, SysTick_Handler via
 * Mini_Time_Target.c, Os_Cm33_IrqEntry); anything not provided falls back to Default_Handler (weak alias).
 * Reset sequence (Armv8-M): the CPU loads MSP from vector[0] and PC from vector[1]; there is no C runtime yet,
 * so Reset_Handler must be written without relying on initialised data.  Compare with RH850, where the reset
 * vector points at assembly that sets the stack pointer first (docs/01-rh850/04-startup-process.md).
 *
 * Linker symbols (target/stm32l552/linker/stm32l552_autosar.ld):
 *   _estack       top of the 4 KB main stack (MSP: Reset, ISR, PendSV, idle phase before the first task)
 *   _sidata       load address (FLASH) of .data      _sdata/_edata  its run address (RAM)
 *   _sbss/_ebss   .bss (incl. .bss.rte, .bss.com)    NOT cleared: .os_stack (painted by the OS), .noinit (RAM2)
 */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Every handler name below is a weak alias of Default_Handler; a strong definition elsewhere wins at link time. */
#define WEAK_DEFAULT  __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void)          WEAK_DEFAULT;
void HardFault_Handler(void)    WEAK_DEFAULT;
void MemManage_Handler(void)    WEAK_DEFAULT;
void BusFault_Handler(void)     WEAK_DEFAULT;
void UsageFault_Handler(void)   WEAK_DEFAULT;
void SecureFault_Handler(void)  WEAK_DEFAULT;
void SVC_Handler(void)          WEAK_DEFAULT;
void DebugMon_Handler(void)     WEAK_DEFAULT;
void PendSV_Handler(void)       WEAK_DEFAULT;     /* strong: os/port/cm33/Os_Port_Cm33.c (context switch) */
void SysTick_Handler(void)      WEAK_DEFAULT;     /* strong: os/port/cm33/Mini_Time_Target.c (1 kHz tick) */
void Os_Cm33_IrqEntry(void)     WEAK_DEFAULT;     /* strong: os/port/cm33/Os_Port_Cm33.c (Cat2 ISR dispatcher) */

/* Unexpected exception or interrupt: stay here so that a debugger / Renode shows where we are. */
void Default_Handler(void)
{
    for (;;) { }
}

/* Number of external interrupt lines of the STM32L552 (IRQ 0 .. 108; 39 = FDCAN1_IT0, see RM0438 table
 * "vector table"). All of them are routed to ONE C function: the OS reads IPSR to learn which one fired
 * and looks the handler up in the generated Os_Config.isrs[] table (DESIGN 6.4 "common IRQ dispatcher"). */
#define STM32L552_NUM_IRQ   109u
#define NUM_VECTORS         (16u + STM32L552_NUM_IRQ)

typedef void (*vector_fn)(void);

/* 16 core entries + 109 external ones = 500 bytes; VTOR needs the table aligned to the next power of two (512). */
__attribute__((section(".isr_vector"), used, aligned(512)))
const vector_fn g_vectors[NUM_VECTORS] = {
    (vector_fn)&_estack,        /*  0: initial MSP                                             */
    Reset_Handler,              /*  1: Reset                                                   */
    NMI_Handler,                /*  2: NMI                                                     */
    HardFault_Handler,          /*  3: HardFault                                               */
    MemManage_Handler,          /*  4: MemManage                                               */
    BusFault_Handler,           /*  5: BusFault                                                */
    UsageFault_Handler,         /*  6: UsageFault                                              */
    SecureFault_Handler,        /*  7: SecureFault (Armv8-M Security Extension, unused)        */
    0, 0, 0,                    /*  8-10: reserved                                             */
    SVC_Handler,                /* 11: SVCall (unused: tasks run privileged, no syscalls)      */
    DebugMon_Handler,           /* 12: DebugMonitor                                            */
    0,                          /* 13: reserved                                                */
    PendSV_Handler,             /* 14: PendSV  -> OS context switch                            */
    SysTick_Handler,            /* 15: SysTick -> OS tick                                      */
    /* external interrupts 0..108: all go to the OS dispatcher (designated initialiser fills the rest) */
    [16 ... NUM_VECTORS - 1u] = Os_Cm33_IrqEntry,
};

/* First code that runs after reset (the CPU has already loaded SP). */
void Reset_Handler(void)
{
    uint32_t *src;
    uint32_t *dst;

    /* Point VTOR at our table (reset value 0 is only an alias of FLASH on this chip). SCB->VTOR = 0xE000ED08 */
    *(volatile uint32_t *)0xE000ED08u = (uint32_t)(uintptr_t)&g_vectors[0];

    /* .data: initialised globals live in FLASH (load address) and must be copied to RAM */
    src = &_sidata;
    for (dst = &_sdata; dst < &_edata; ) {
        *dst++ = *src++;
    }
    /* .bss: zero-initialised globals */
    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0u;
    }

    (void)main();
    for (;;) { }                /* main() must not return on an ECU; stay here if it does */
}
