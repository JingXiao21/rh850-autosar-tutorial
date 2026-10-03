/*
 * Os_Core.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the kernel core of an AUTOSAR OS (SC1 subset): start-up/shutdown, the ready
 * queue + dispatcher decision, error handling with ErrorHook, Category 2 ISR wrapper and the hook calls.
 * Implemented: StartOS, ShutdownOS, GetActiveApplicationMode, OSErrorGetServiceId, fixed-priority ready queue
 * (one FIFO per priority + bitmap), preemption decision, Pre/PostTaskHook, StartupHook, ErrorHook,
 * ShutdownHook, stack-canary check, Cat2 ISR enter/exit.
 * NOT implemented: OS-Applications, ProtectionHook, timing protection, multicore, ScheduleTables.
 *
 * Spec (AUTOSAR_CP_SWS_OS R25-11, which defers task/scheduling semantics to OSEK/VDX OS 2.2.3):
 *   SWS_Os_00424  the first call of StartOS shall not return
 *   SWS_Os_00425  ShutdownOS: after ShutdownHook returns, disable all interrupts, endless loop
 *   SWS_Os_00071  PostTaskHook is not called during ShutdownOS
 *   SWS_Os_00067/00068  stack monitoring -> ShutdownOS(E_OS_STACKFAULT) when no ProtectionHook
 *   SWS_Os_00368/00369  Cat2 ISR ends with locked interrupts / unreleased resource -> fix up + ErrorHook
 *   SWS_Os_00069/00070/00239 handled in Os_Kernel_TaskReturn (task returned without TerminateTask)
 *   OSEK 4.2 task states, 4.6 scheduling policy: highest priority first, FIFO within a priority, a preempted
 *   task re-enters its queue at the HEAD (it continues before later-activated equal-priority tasks).
 *
 * How a task switch happens (identical for both ports):
 *   service (e.g. ActivateTask)  -> changes TCB/queue under lock
 *                                -> Os_Reschedule(): "is the best READY task more urgent than the running one?"
 *                                -> Os_Port_RequestDispatch()
 *   port, at its earliest legal point -> Os_Kernel_SelectNext(): pick the next task (this file)
 *                                     -> port saves/restores the CPU context.
 */
#include "Os_Internal.h"

/* ------------------------------------------------------------------ globals */
Os_TcbType Os_Tcb[OS_NUM_TASKS + 1u];
TaskType   Os_Running = INVALID_TASK;
boolean    Os_Started = FALSE;
uint8      Os_IsrDepth = 0u;
ISRType    Os_IsrIds[OS_MAX_ISR_NEST];
uint8      Os_MaxTaskPriority = 0u;

static AppModeType     s_appMode;
static OSServiceIdType s_errSvc;
static boolean         s_inErrorHook;
static boolean         s_inShutdown;
static StatusType      s_shutdownStatus = E_OK;
static boolean         s_yield;                      /* Schedule() allows a NON-preemptive task to be switched out once */
static uint32          s_isrSnap[OS_MAX_ISR_NEST];   /* interrupt nesting state at ISR entry (SWS_Os_00368) */

/* ------------------------------------------------------------------ ready queue
 * One small ring buffer per priority level; bit p of s_readyMask is set while queue p is not empty.
 * The highest READY priority is therefore "31 - clz(mask)" - a single instruction on Cortex-M. */
static TaskType s_q[OS_MAX_PRIORITY + 1u][OS_QUEUE_DEPTH];
static uint8    s_qHead[OS_MAX_PRIORITY + 1u];
static uint8    s_qCount[OS_MAX_PRIORITY + 1u];
static uint32   s_readyMask;

#define OS_QMASK  (OS_QUEUE_DEPTH - 1u)

static void q_push(TaskType t, uint8 prio, boolean front)
{
    uint8 n = s_qCount[prio];

    if (n >= OS_QUEUE_DEPTH) {
        return;                                   /* cannot happen: capacity is verified in os_check_config() */
    }
    if (front != FALSE) {                         /* preempted task: goes back to the head of its queue */
        s_qHead[prio] = (uint8)((s_qHead[prio] - 1u) & OS_QMASK);
        s_q[prio][s_qHead[prio]] = t;
    } else {                                      /* newly activated / woken task: tail */
        s_q[prio][(s_qHead[prio] + n) & OS_QMASK] = t;
    }
    s_qCount[prio] = (uint8)(n + 1u);
    s_readyMask |= (1u << prio);
}

static TaskType q_pop(uint8 prio)
{
    TaskType t = s_q[prio][s_qHead[prio]];

    s_qHead[prio] = (uint8)((s_qHead[prio] + 1u) & OS_QMASK);
    s_qCount[prio]--;
    if (s_qCount[prio] == 0u) {
        s_readyMask &= ~(1u << prio);
    }
    return t;
}

/* highest priority that has a READY entry, 0 if none (idle) */
static uint8 q_best(void)
{
    return (s_readyMask == 0u) ? 0u : (uint8)(31u - (uint32)__builtin_clz(s_readyMask));
}

void Os_Ready_Enqueue(TaskType t)
{
    q_push(t, Os_Tcb[t].curPriority, FALSE);
}

/* ------------------------------------------------------------------ error handling */

/* Every failing service ends here: remember which service failed (OSErrorGetServiceId) and run ErrorHook. */
void Os_ReportError(OSServiceIdType svc, StatusType err)
{
    s_errSvc = svc;
#if OS_USE_ERRORHOOK
    if (s_inErrorHook == FALSE) {                 /* services used inside ErrorHook must not recurse */
        s_inErrorHook = TRUE;
        ErrorHook(err);
        s_inErrorHook = FALSE;
    }
#else
    TRACE(TRACE_CAT_OS, "ERROR svc=%u err=%u", (unsigned)svc, (unsigned)err);
#endif
}

OSServiceIdType OSErrorGetServiceId(void)
{
    return s_errSvc;
}

AppModeType GetActiveApplicationMode(void)
{
    return s_appMode;
}

StatusType Os_Kernel_GetShutdownStatus(void)
{
    return s_shutdownStatus;
}

boolean Os_InTaskContext(void)
{
    return (boolean)((Os_Started != FALSE) && (Os_Port_InIsr() == FALSE) && (Os_Running < (TaskType)OS_NUM_TASKS));
}

/* ------------------------------------------------------------------ scheduling decision */

/* Should the running context be switched out right now?  Called with the TCB already updated. */
static boolean os_need_dispatch(void)
{
    const Os_TcbType *run;
    uint8 best = q_best();

    if (Os_Started == FALSE) {
        return FALSE;                             /* still inside StartOS: nothing runs yet */
    }
    if ((Os_Running == INVALID_TASK) || (Os_Running == OS_IDLE_TASK)) {
        return (boolean)(best != 0u);             /* idle (or first dispatch): any READY task wins */
    }
    run = &Os_Tcb[Os_Running];
    if (run->state != RUNNING) {
        return TRUE;                              /* running task just terminated / went WAITING */
    }
    if (Os_Config.tasks[Os_Running].schedule == OS_SCHED_NON) {
        return FALSE;                             /* non-preemptive: only Schedule/Terminate/WaitEvent give up the CPU */
    }
    return (boolean)(best > run->curPriority);    /* strictly higher: equal priority never preempts (FIFO) */
}

void Os_Reschedule(void)
{
    uint32 lk = Os_Port_DisableAll();
    boolean need = os_need_dispatch();

    Os_Port_RestoreAll(lk);
    if (need != FALSE) {
        Os_Port_RequestDispatch();                /* target: pends PendSV; host: switches fiber now (task) or at ISR exit */
    }
    Os_Port_InterruptPoint();                     /* host: deliver simulated interrupts that became due */
}

/* Schedule(): OSEK lets a (non-)preemptive task offer the CPU to strictly higher priority READY tasks. */
void Os_Sched_Yield(void)
{
    uint32 lk = Os_Port_DisableAll();
    boolean need = (boolean)((Os_Running < (TaskType)OS_NUM_TASKS) && (q_best() > Os_Tcb[Os_Running].curPriority));

    if (need != FALSE) {
        s_yield = TRUE;
    }
    Os_Port_RestoreAll(lk);
    if (need != FALSE) {
        Os_Port_RequestDispatch();
    }
}

/* Dispatcher - called by the port (PendSV on target, SwitchToFiber helper on host) with interrupts locked.
 * Decides who runs next and does all kernel-side bookkeeping:
 *   - the leaving task: RUNNING -> READY (preempted, re-queued at the head) or stays WAITING/SUSPENDED
 *   - the entering task: READY -> RUNNING, fresh context if it starts a new activation
 * Returns the TCB index of the next task (OS_IDLE_TASK if nothing is READY); *prevOut = previous task. */
TaskType Os_Kernel_SelectNext(TaskType *prevOut)
{
    TaskType prev = Os_Running;
    TaskType next;
    uint8 best = q_best();

    *prevOut = prev;

    if ((prev != INVALID_TASK) && (prev != OS_IDLE_TASK)) {
        Os_TcbType *p = &Os_Tcb[prev];

        if (Os_Port_StackCheck(prev) == FALSE) {
            ShutdownOS(E_OS_STACKFAULT);          /* SWS_Os_00068 (no ProtectionHook in this OS) */
        }
        if (p->state == RUNNING) {
            if ((best <= p->curPriority) ||
                ((Os_Config.tasks[prev].schedule == OS_SCHED_NON) && (s_yield == FALSE))) {
                s_yield = FALSE;
                return prev;                      /* stale request: the running task keeps the CPU */
            }
            p->state = READY;                     /* preempted */
            q_push(prev, p->curPriority, TRUE);
        }
#if OS_USE_POSTTASKHOOK
        PostTaskHook();                           /* Os_Running is still the leaving task here */
#endif
    } else if ((prev == OS_IDLE_TASK) && (best == 0u)) {
        return prev;
    }
    s_yield = FALSE;

    if (best == 0u) {
        next = OS_IDLE_TASK;
    } else {
        next = q_pop(best);
        Os_Tcb[next].state = RUNNING;
        if (Os_Tcb[next].started == FALSE) {      /* first run of this activation: build the initial context */
            Os_Tcb[next].started = TRUE;
            Os_Port_InitTaskContext(next);
            if (MINI_TRACE_OS_SWITCH == STD_ON) {
                TRACE(TRACE_CAT_OS, "TASK_START %s prio=%u", Os_Config.tasks[next].name,
                      (unsigned)Os_Config.tasks[next].priority);
            }
        }
    }
    Os_Running = next;
#if OS_USE_PRETASKHOOK
    if (next != OS_IDLE_TASK) {
        PreTaskHook();
    }
#endif
    return next;
}

/* ------------------------------------------------------------------ names / diagnostics */
const char *Os_GetTaskName(TaskType TaskID)
{
    if (TaskID < (TaskType)Os_Config.numTasks) {
        return Os_Config.tasks[TaskID].name;
    }
    return (TaskID == OS_IDLE_TASK) ? "Idle" : "?";
}

uint32 Os_GetTaskStackUsage(TaskType TaskID)
{
    return (TaskID < (TaskType)Os_Config.numTasks) ? Os_Port_StackUsage(TaskID) : 0u;
}

/* ------------------------------------------------------------------ task returned from its entry function */

/* SWS_Os_00052/00069/00070/00239: terminate the task, call ErrorHook(E_OS_MISSINGEND), release its resources
 * and re-enable interrupts.  Called by the port when the task body returns (target: via the LR of the
 * initial stack frame; host: end of the fiber function).  Never returns. */
void Os_Kernel_TaskReturn(void)
{
    uint32 lk;

    Os_ReportError(OSServiceId_TerminateTask, E_OS_MISSINGEND);
    Os_Resource_ReleaseAll(Os_Running);
    (void)Os_Interrupt_Unwind(0u);
    lk = Os_Port_DisableAll();
    Os_Task_Finish();
    Os_Port_RestoreAll(lk);
    Os_Port_RequestDispatch();
    for (;;) { }
}

/* ------------------------------------------------------------------ Category 2 ISR wrapper */

/* Called by the port right before the user ISR body runs (the "ISR2 prologue" of OSEK). */
void Os_Kernel_IsrEnter(ISRType id)
{
    if (Os_IsrDepth < OS_MAX_ISR_NEST) {
        Os_IsrIds[Os_IsrDepth] = id;
        s_isrSnap[Os_IsrDepth] = Os_Interrupt_Snapshot();
    }
    Os_IsrDepth++;
    if (MINI_TRACE_OS_SWITCH == STD_ON) {
        TRACE(TRACE_CAT_OS, "ISR_ENTER %s", Os_Config.isrs[id].name);
    }
}

/* ... and right after it: repair leftovers (SWS_Os_00368) and reschedule - an ISR that activated a more
 * urgent task makes the port switch as soon as the outermost ISR has finished. */
void Os_Kernel_IsrExit(void)
{
    ISRType id = INVALID_ISR;

    if (Os_IsrDepth == 0u) {
        return;
    }
    Os_IsrDepth--;
    if (Os_IsrDepth < OS_MAX_ISR_NEST) {
        id = Os_IsrIds[Os_IsrDepth];
        if (Os_Interrupt_Unwind(s_isrSnap[Os_IsrDepth]) != FALSE) {
            Os_ReportError(OSServiceId_IsrExit, E_OS_DISABLEDINT);
        }
    }
    if ((MINI_TRACE_OS_SWITCH == STD_ON) && (id != INVALID_ISR)) {
        TRACE(TRACE_CAT_OS, "ISR_EXIT %s", Os_Config.isrs[id].name);
    }
    Os_Reschedule();
}

/* ------------------------------------------------------------------ configuration sanity check */

/* The kernel trusts the generated tables; this catches generator mistakes early (educational luxury). */
static StatusType os_check_config(void)
{
    uint8 need[OS_MAX_PRIORITY + 1u];
    uint8 i;
    uint8 p;
    uint8 r;

    if ((Os_Config.numTasks != (uint8)OS_NUM_TASKS) || (Os_Config.numResources != (uint8)OS_NUM_RESOURCES) ||
        (Os_Config.numAlarms != (uint8)OS_NUM_ALARMS) || (Os_Config.numCounters != (uint8)OS_NUM_COUNTERS) ||
        (Os_Config.numIsrs != (uint8)OS_NUM_ISRS)) {
        return E_OS_VALUE;                        /* Os_Cfg.h and Os_Cfg.c disagree */
    }
    for (p = 0u; p <= OS_MAX_PRIORITY; p++) {
        need[p] = 0u;
    }
    for (i = 0u; i < Os_Config.numTasks; i++) {
        p = Os_Config.tasks[i].priority;
        if ((p == 0u) || (p > OS_MAX_PRIORITY) || (Os_Config.tasks[i].maxActivations == 0u)) {
            return E_OS_VALUE;                    /* priority 0 is the idle task; > 31 does not fit the bitmap */
        }
        need[p] = (uint8)(need[p] + Os_Config.tasks[i].maxActivations);
        if (p > Os_MaxTaskPriority) {
            Os_MaxTaskPriority = p;
        }
    }
    /* A task that holds a resource sits in the queue of the CEILING priority when it is preempted:
     * reserve one slot per lower-priority task at every ceiling (RES_SCHEDULER included). */
    for (r = 0u; r <= Os_Config.numResources; r++) {
        uint8 ceil = (r == 0u) ? Os_MaxTaskPriority : Os_Config.resources[r - 1u].ceilingPriority;

        if (ceil > OS_MAX_PRIORITY) {
            return E_OS_VALUE;
        }
        for (i = 0u; i < Os_Config.numTasks; i++) {
            if (Os_Config.tasks[i].priority < ceil) {
                need[ceil]++;
            }
        }
    }
    for (p = 0u; p <= OS_MAX_PRIORITY; p++) {
        if (need[p] > OS_QUEUE_DEPTH) {
            return E_OS_LIMIT;
        }
    }
    return E_OK;
}

/* ------------------------------------------------------------------ StartOS / ShutdownOS */

/* StartOS: set up the kernel, autostart objects, call StartupHook, then hand over to the port which never
 * returns (SWS_Os_00424).  Sequence = DESIGN section 9. */
void StartOS(AppModeType Mode)
{
    uint8 i;
    StatusType st;

    (void)Os_Port_DisableAll();                   /* the port re-enables interrupts when the first task starts */
    s_appMode = Mode;

    st = os_check_config();
    if (st != E_OK) {
        Os_ReportError(OSServiceId_StartOS, st);
        ShutdownOS(st);
    }

    for (i = 0u; i <= OS_NUM_TASKS; i++) {
        Os_Tcb[i].state = SUSPENDED;
        Os_Tcb[i].activations = 0u;
        Os_Tcb[i].curPriority = (i < OS_NUM_TASKS) ? Os_Config.tasks[i].priority : 0u;
        Os_Tcb[i].started = FALSE;
        Os_Tcb[i].resCount = 0u;
        Os_Tcb[i].eventsSet = 0u;
        Os_Tcb[i].eventsWaited = 0u;
    }
    Os_Tcb[OS_IDLE_TASK].state = READY;            /* the idle task is always ready, never queued */
    Os_Resource_Init();

    Os_Port_Init();                                /* NVIC/SysTick priorities, stack paint, host fiber environment */
    TRACE(TRACE_CAT_OS, "STARTOS mode=%u", (unsigned)Mode);
    Os_Tcb[OS_IDLE_TASK].started = TRUE;
    Os_Port_InitTaskContext(OS_IDLE_TASK);

    for (i = 0u; i < Os_Config.numTasks; i++) {   /* autostart tasks of this application mode */
        if ((Os_Config.tasks[i].autostartModes & (1uL << Mode)) != 0u) {
            (void)Os_ActivateInternal(i);
        }
    }
    Os_Alarm_Init(Mode);                           /* counters := 0 and autostart alarms */

#if OS_USE_STARTUPHOOK
    StartupHook();
#endif
    Os_Port_StartTick();
    Os_Started = TRUE;
    Os_Port_StartFirstTask();                      /* never returns */
    for (;;) { }
}

/* ShutdownOS: ShutdownHook, then lock interrupts and stop (SWS_Os_00425).  PostTaskHook is NOT called
 * (SWS_Os_00071).  Callable before StartOS and from any context. */
void ShutdownOS(StatusType Error)
{
    (void)Os_Port_DisableAll();
    if (s_inShutdown == FALSE) {
        s_inShutdown = TRUE;
        s_shutdownStatus = Error;
#if OS_USE_SHUTDOWNHOOK
        ShutdownHook(Error);
#else
        TRACE(TRACE_CAT_OS, "SHUTDOWN err=%u", (unsigned)Error);
#endif
    }
    Os_Port_Halt();
    for (;;) { }
}
