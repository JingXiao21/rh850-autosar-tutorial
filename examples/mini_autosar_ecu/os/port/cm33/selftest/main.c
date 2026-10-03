/*
 * main.c  (OS-only target smoke test, Cortex-M33 / Renode)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (an integration test of the OS alone, without EcuM/RTE/BSW).
 * Scenario (times are ms since StartOS; the trace shows [t_us] stamps):
 *   t=0    Task_Ext (prio 3) and Task_Low (prio 1) are autostarted. Ext runs first, blocks in WaitEvent;
 *          Low runs, takes Res_X (ceiling 5) and burns CPU until t=30.
 *   t=20   Alarm_Hi activates Task_Hi (prio 5)... but Low holds Res_X (ceiling 5): Hi must NOT run yet.
 *   t=30   Low releases Res_X: priority drops, Hi (5) preempts at once (ReleaseResource is a rescheduling point).
 *   t=50   Alarm_Ev -> SetEvent(Ext, Ev1): Ext (3) preempts the running Low (1).
 *   t~55   Low pends a software interrupt: Cat2 ISR sets Ev2 for Ext; Ext runs as soon as the ISR has returned.
 *   t=100  and t=150: Alarm_Ev again; at the third Ev1 Ext prints the verdict and calls ShutdownOS(E_OK).
 * Alarm_Cb (every 10 ms) is an alarm callback that just counts.
 */
#include "Os.h"
#include "Trace.h"
#include "Mini_Time.h"

static volatile unsigned g_cbCount;

#define SIM(...)  Trace_Log(TRACE_CAT_SIM, __VA_ARGS__)

/* busy wait until the system time reaches `ms` (interrupts and higher priority tasks may run meanwhile) */
static void burn_until_ms(unsigned ms)
{
    while (Mini_Time_GetUs() < (ms * 1000u)) {
        /* CPU is busy */
    }
}

TASK(Task_Low)
{
    SIM("LOW start");
    (void)GetResource(Res_X);
    SIM("LOW got Res_X, burning until 30 ms");
    burn_until_ms(30u);
    SIM("LOW releasing Res_X (Hi was activated at 20 ms and must run NOW)");
    (void)ReleaseResource(Res_X);
    SIM("LOW resumed after Hi");
    burn_until_ms(56u);
    SIM("LOW pends IRQ %u (Cat2 ISR sets Ev2 for Task_Ext)", (unsigned)SELFTEST_IRQ);
    *(volatile uint32 *)0xE000E200u = (1uL << SELFTEST_IRQ);       /* NVIC_ISPR0: set pending */
    __asm volatile ("dsb \n isb" : : : "memory");
    SIM("LOW after IRQ (Ext already ran)");
    burn_until_ms(130u);
    SIM("LOW end, callback count=%u", g_cbCount);
    (void)TerminateTask();
}

TASK(Task_Hi)
{
    SIM("HI run");
    (void)TerminateTask();
}

TASK(Task_Ext)
{
    EventMaskType ev;
    unsigned n1 = 0u;

    SIM("EXT start");
    for (;;) {
        (void)WaitEvent(Ev1 | Ev2);
        (void)GetEvent(Task_Ext, &ev);
        (void)ClearEvent(ev);
        SIM("EXT woken ev=0x%x", (unsigned)ev);
        if ((ev & Ev1) != 0u) {
            n1++;
            if (n1 == 3u) {
                SIM("SELFTEST PASS cb=%u stack_low=%u ext=%u hi=%u", g_cbCount,
                    (unsigned)Os_GetTaskStackUsage(Task_Low), (unsigned)Os_GetTaskStackUsage(Task_Ext),
                    (unsigned)Os_GetTaskStackUsage(Task_Hi));
                ShutdownOS(E_OK);
            }
        }
    }
}

ISR(Isr_Sw)
{
    SIM("ISR Isr_Sw id=%u", (unsigned)GetISRID());
    (void)SetEvent(Task_Ext, Ev2);
}

ALARMCALLBACK(Alarm_Cb)
{
    g_cbCount++;
}

/* ---- hooks */
void StartupHook(void)             { SIM("StartupHook"); }
void ErrorHook(StatusType e)       { SIM("ErrorHook err=%u svc=%u", (unsigned)e, (unsigned)OSErrorGetServiceId()); }
void ShutdownHook(StatusType e)    { SIM("ShutdownHook err=%u", (unsigned)e); }
void PreTaskHook(void)             { }
void PostTaskHook(void)            { }

int main(void)
{
    Trace_Init(NULL_PTR);
    StartOS(OSDEFAULTAPPMODE);
    return 0;
}
