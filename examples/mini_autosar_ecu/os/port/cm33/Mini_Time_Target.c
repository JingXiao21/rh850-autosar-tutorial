/*
 * Mini_Time_Target.c  (TARGET BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none directly; the nearest real concepts are the Gpt driver's free-running timer
 * and the OS counter driver ("OsDriver", SysTick) that feeds the system counter.
 * Owns the Cortex-M33 SysTick: Mini_Time_Init() programs it for 1 kHz, SysTick_Handler counts milliseconds and
 * calls Mini_Time_TickHook().  The OS port overrides the weak hook with the kernel tick handler; the bare-metal
 * rest-bus image (no OS) overrides it with its own periodic function.  This file depends on NOTHING from the OS.
 *
 * Mini_Time_GetUs() = ms * 1000 + elapsed SysTick cycles / (MINI_CPU_CLOCK_HZ / 1e6).  It is monotonic and also
 * valid inside ISRs, even while SysTick is masked (a wrap that has not been serviced yet is detected through
 * the ICSR.PENDSTSET flag).  Before Mini_Time_Init() every getter returns 0.
 * Registers (ARMv8-M ARM, B3.3 SysTick): CSR 0xE000E010, RVR 0xE000E014, CVR 0xE000E018; ICSR 0xE000ED04.
 */
#include "Mini_Time.h"
#include "Mini_Cfg.h"
#include "MemMap.h"

#define REG32(a)        (*(volatile uint32 *)(a))
#define SYST_CSR        REG32(0xE000E010u)     /* bit0 ENABLE, bit1 TICKINT, bit2 CLKSOURCE(1=core clock), bit16 COUNTFLAG */
#define SYST_RVR        REG32(0xE000E014u)     /* reload value (24 bit) */
#define SYST_CVR        REG32(0xE000E018u)     /* current value; any write clears it */
#define SCB_ICSR        REG32(0xE000ED04u)     /* bit26 PENDSTSET: SysTick exception is pending */

#define SYST_RELOAD     ((MINI_CPU_CLOCK_HZ / 1000u) - 1u)          /* 79999 for 80 MHz -> 1 ms */
#define CYCLES_PER_US   (MINI_CPU_CLOCK_HZ / 1000000u)              /* 80 */

static volatile uint32 s_ms;
static volatile boolean s_running;

void Mini_Time_Init(void)
{
    if (s_running != FALSE) {
        return;                                    /* idempotent: StartOS and main() may both call it */
    }
    SYST_CSR = 0u;                                 /* stop while reprogramming */
    SYST_RVR = SYST_RELOAD;
    SYST_CVR = 0u;                                 /* clear the counter and COUNTFLAG */
    s_ms = 0u;
    s_running = TRUE;
    SYST_CSR = 0x7u;                               /* CLKSOURCE=core clock | TICKINT | ENABLE */
}

uint32 Mini_Time_GetMs(void)
{
    return (s_running != FALSE) ? s_ms : 0u;
}

uint32 Mini_Time_GetUs(void)
{
    uint32 ms;
    uint32 cvr;
    uint32 pend1;
    uint32 pend2;
    uint32 ms2;

    if (s_running == FALSE) {
        return 0u;
    }
    do {                                           /* retry if the tick handler ran while we were sampling */
        ms = s_ms;
        pend1 = SCB_ICSR & (1uL << 26);
        cvr = SYST_CVR;
        pend2 = SCB_ICSR & (1uL << 26);
        ms2 = s_ms;
    } while ((ms != ms2) || (pend1 != pend2));
    if (pend2 != 0u) {
        ms++;                                      /* SysTick wrapped but its handler has not counted it yet */
    }
    return (ms * 1000u) + ((SYST_RELOAD - cvr) / CYCLES_PER_US);
}

/* Weak default: nobody wants the tick.  The OS port (Os_Port_Cm33.c) or the rest-bus image overrides it. */
__attribute__((weak)) void Mini_Time_TickHook(void)
{
}

/* SysTick exception: the vector table entry (startup.c) resolves to this strong definition. */
MINI_CODE_FAST void SysTick_Handler(void)
{
    s_ms++;
    Mini_Time_TickHook();
}
