/*
 * Os_Port_Host.c  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the processor-specific "port" part of an AUTOSAR OS (not standardised); here a
 * simulation port so that the SAME kernel, RTE and BSW sources run on a PC.  Spec context: DESIGN.md 6.5.
 *
 * Idea
 *   * Every task gets its own Windows FIBER = its own C stack that can be suspended in the middle of a
 *     function (e.g. inside WaitEvent) and resumed later.  SwitchToFiber() is the context switch.
 *     No threads, no preemption by the host OS -> every run is 100 % deterministic.
 *   * The main thread becomes the IDLE task (TCB index OS_NUM_TASKS).  The idle loop is also the "clock":
 *     when nothing is READY it advances VIRTUAL time by up to 1 ms, delivers the kernel tick, runs the
 *     simulator step hooks (CAN script, rest-bus ...) and delivers simulated Cat2 interrupts.
 *   * Tasks can "consume CPU time" with Os_HostBurn(us): time advances, ticks/ISRs become due and may
 *     preempt the burning task - this makes preemption visible on the host.
 *   * "Interrupts" are function calls on the current fiber stack at well defined DELIVERY POINTS (idle loop,
 *     Os_HostBurn, kernel service return, unmasking).  While an ISR/tick runs, Os_Port_InIsr() is TRUE and
 *     dispatch requests are only recorded; the switch happens when the ISR returns - the same observable
 *     behaviour as PendSV on the Cortex-M33.
 * Not simulated: real asynchronous preemption in the middle of a C statement.
 */
#include <stdlib.h>                /* exit; the Windows fiber calls are wrapped in Os_PortFiber_Host.c */
#include "Os_Port.h"
#include "Os_CfgTypes.h"
#include "SimTime.h"
#include "Os_PortHost.h"
#include "HostFiber.h"

#define HOST_FIBER_STACK_BYTES   (256u * 1024u)
#define HOST_MAX_ZOMBIES         8u
#define OS_HOST_IDLE             ((TaskType)OS_NUM_TASKS)

static void    *s_fiber[OS_NUM_TASKS + 1u];       /* one fiber per task + the idle (= main) fiber */
static void    *s_curFiber;                       /* fiber that is executing right now */
static void    *s_zombie[HOST_MAX_ZOMBIES];       /* fibers of finished tasks that could not be deleted yet */

static uint32   s_inIsr;                          /* >0 while a simulated tick/ISR runs */
static boolean  s_locked;                         /* DisableAll state (PRIMASK analogue) */
static boolean  s_osMasked;                       /* SuspendOS state (BASEPRI analogue) */
static boolean  s_dispatchPending;                /* a switch was requested while it was not allowed (PendSV pending) */
static boolean  s_delivering;                     /* guard against recursive delivery */
static boolean  s_tickRunning;                    /* Os_Port_StartTick was called */
static uint32   s_tickPending;                    /* ticks that are due but not yet delivered (masked) */
static uint64   s_nextTickUs;                     /* virtual time of the next 1 ms boundary */
static boolean  s_inited;

/* ------------------------------------------------------------------ fibers */

/* DeleteFiber on the running fiber would end the thread: postpone until we run on another fiber. */
static void fiber_reap(void)
{
    uint32 i;

    for (i = 0u; i < HOST_MAX_ZOMBIES; i++) {
        if ((s_zombie[i] != NULL) && (s_zombie[i] != s_curFiber)) {
            HostFiber_Delete(s_zombie[i]);
            s_zombie[i] = NULL;
        }
    }
}

static void fiber_retire(void *f)
{
    uint32 i;

    if (f == NULL) {
        return;
    }
    if (f != s_curFiber) {
        HostFiber_Delete(f);
        return;
    }
    for (i = 0u; i < HOST_MAX_ZOMBIES; i++) {
        if (s_zombie[i] == NULL) {
            s_zombie[i] = f;
            return;
        }
    }
}

/* Entry point of every task fiber: run the task, and if it returns without TerminateTask -> E_OS_MISSINGEND. */
static void host_fiber_main(unsigned arg)
{
    TaskType t = (TaskType)arg;

    fiber_reap();
    Os_Config.tasks[t].entry();
    Os_Kernel_TaskReturn();
    for (;;) { }
}

/* The dispatcher: ask the kernel who runs next and switch fibers if that is not us. */
static void host_switch(void)
{
    TaskType prev;
    TaskType next = Os_Kernel_SelectNext(&prev);
    void *target = s_fiber[next];

    if (target != s_curFiber) {
        s_curFiber = target;
        HostFiber_Switch(target);                    /* returns when some other fiber switches back to us */
        fiber_reap();
    }
}

static void host_dispatch_pending(void)
{
    if ((s_dispatchPending != FALSE) && (s_inIsr == 0u) && (s_locked == FALSE) && (s_osMasked == FALSE)) {
        s_dispatchPending = FALSE;
        host_switch();
    }
}

/* ------------------------------------------------------------------ simulated interrupts */

static void host_run_isr(uint16 irq)
{
    uint8 i;

    for (i = 0u; i < Os_Config.numIsrs; i++) {
        if (Os_Config.isrs[i].irqNumber == irq) {
            s_inIsr++;
            Os_Kernel_IsrEnter((ISRType)i);
            Os_Config.isrs[i].handler();
            Os_Kernel_IsrExit();
            s_inIsr--;
            return;
        }
    }                                             /* no ISR configured for this line: target would hit Default_Handler */
}

static void host_deliver_ticks(void)
{
    while (s_tickPending != 0u) {
        s_tickPending--;
        s_inIsr++;
        Os_Kernel_TickHandler();
        s_inIsr--;
    }
}

static boolean host_may_deliver(void)
{
    return (boolean)((s_delivering == FALSE) && (s_inIsr == 0u) && (s_locked == FALSE) && (s_osMasked == FALSE));
}

/* Deliver everything that is due and allowed, then perform a pending task switch. */
static void host_deliver(void)
{
    uint16 irq;

    if (host_may_deliver() != FALSE) {
        s_delivering = TRUE;
        host_deliver_ticks();
        while (SimTime_TakePendingIsr(&irq) != FALSE) {
            host_run_isr(irq);
        }
        s_delivering = FALSE;
    }
    host_dispatch_pending();
}

/* ------------------------------------------------------------------ virtual time */

/* A 1 ms boundary was reached: kernel tick first, then the simulator (CAN script, rest-bus), then ISRs. */
static void host_on_boundary(void)
{
    s_nextTickUs += Os_Config.tickDurationUs;
    s_tickPending++;
    if (host_may_deliver() != FALSE) {
        s_delivering = TRUE;
        host_deliver_ticks();
        s_delivering = FALSE;
    }
    SimTime_RunStepHooks(SimTime_GetUs());
    if ((SimTime_GetEndUs() != 0u) && (SimTime_GetUs() >= SimTime_GetEndUs())) {
        ShutdownOS(E_OK);                         /* --run-ms reached: orderly end of the simulation */
    }
}

static void host_advance(uint32 us)
{
    SimTime_AdvanceUs(us);
    if ((s_tickRunning != FALSE) && (SimTime_GetUs() >= s_nextTickUs)) {
        host_on_boundary();
    }
    host_deliver();
}

/* "Consume CPU time" inside a task (or anywhere): advances virtual time, ticks/ISRs may preempt the caller. */
void Os_HostBurn(uint32 us)
{
    while (us > 0u) {
        uint32 step = us;

        if (s_tickRunning != FALSE) {
            uint32 gap = (uint32)(s_nextTickUs - SimTime_GetUs());
            if (gap < step) {
                step = gap;
            }
        }
        us -= step;
        host_advance(step);
    }
}

/* ------------------------------------------------------------------ Os_Port.h implementation */

void Os_Port_Init(void)
{
    if (s_inited == FALSE) {
        s_inited = TRUE;
        s_fiber[OS_HOST_IDLE] = HostFiber_ConvertThread();   /* the main thread is the idle task from now on */
        s_curFiber = s_fiber[OS_HOST_IDLE];
    }
}

void Os_Port_StartTick(void)
{
    uint64 period = Os_Config.tickDurationUs;

    s_nextTickUs = ((SimTime_GetUs() / period) + 1u) * period;
    s_tickRunning = TRUE;
}

/* (Re)build the context of task t: a brand-new fiber that starts at the task entry function. */
void Os_Port_InitTaskContext(TaskType t)
{
    if (t == OS_HOST_IDLE) {
        return;                                   /* the idle fiber is the main fiber */
    }
    fiber_retire(s_fiber[t]);
    s_fiber[t] = HostFiber_Create(HOST_FIBER_STACK_BYTES, host_fiber_main, (unsigned)t);
    if (s_fiber[t] == NULL) {
        ShutdownOS(E_OS_STACKFAULT);
    }
}

/* Task context: switch right now.  ISR/tick context or interrupts masked: remember and switch later
 * (the target pends PendSV, which also waits until the ISR ends / BASEPRI is lowered). */
void Os_Port_RequestDispatch(void)
{
    if ((s_inIsr != 0u) || (s_locked != FALSE) || (s_osMasked != FALSE)) {
        s_dispatchPending = TRUE;
        return;
    }
    s_dispatchPending = FALSE;
    host_switch();
}

void Os_Port_Idle(void)
{
    /* idle loop body: nothing is READY, so let virtual time run up to the next 1 ms boundary */
    if (s_tickRunning != FALSE) {
        host_advance((uint32)(s_nextTickUs - SimTime_GetUs()));
    } else {
        host_advance(1000u);
    }
}

/* Leave StartOS: dispatch the autostarted tasks, afterwards the idle loop runs the clock forever. */
void Os_Port_StartFirstTask(void)
{
    s_locked = FALSE;                             /* StartOS locked interrupts; the first task starts unlocked */
    s_dispatchPending = TRUE;
    host_dispatch_pending();
    for (;;) {
        Os_Port_Idle();
    }
}

void Os_Port_Halt(void)
{
    Trace_HostFlush();
    exit((int)Os_Kernel_GetShutdownStatus());     /* 0 = E_OK; otherwise the E_OS_* code */
}

uint32 Os_Port_DisableAll(void)
{
    uint32 prev = (s_locked != FALSE) ? 1u : 0u;

    s_locked = TRUE;
    return prev;
}

void Os_Port_RestoreAll(uint32 prev)
{
    s_locked = (prev != 0u) ? TRUE : FALSE;
}

void Os_Port_SetOsMask(boolean masked)
{
    s_osMasked = masked;
}

boolean Os_Port_InIsr(void)
{
    return (boolean)(s_inIsr != 0u);
}

void Os_Port_InterruptPoint(void)
{
    host_deliver();
}

boolean Os_Port_StackCheck(TaskType t)
{
    (void)t;
    return TRUE;                                  /* fibers have guard pages: nothing to check */
}

uint32 Os_Port_StackUsage(TaskType t)
{
    (void)t;
    return 0u;
}
