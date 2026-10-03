/*
 * Os_Task.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Task management of the AUTOSAR OS (ActivateTask, TerminateTask, ChainTask,
 * Schedule, GetTaskID, GetTaskState).  The R25-11 Os SWS only adds multicore/protection rules here and
 * otherwise defers the behaviour to OSEK/VDX OS 2.2.3 chapter 13.2 (task management); the status codes
 * used below are the STANDARD + the AUTOSAR extended ones (E_OS_DISABLEDINT, SWS_Os_00093).
 * Implemented: multiple activation (OsTaskActivation), task state machine, run-to-completion of basic
 * tasks, events reset on activation, ChainTask as atomic terminate+activate.
 * NOT implemented: ActivateTaskAsyn, cross-core activation, OS-Application access checks.
 *
 * Task state machine (OSEK 4.2), "activations" counts running + queued instances:
 *
 *   SUSPENDED --Activate--> READY --dispatch--> RUNNING --Terminate/Chain/return--> SUSPENDED
 *                              ^                  |   \--(activations left)--> READY (new instance)
 *                              |   preempt        v
 *                              +---------------- (higher priority READY) ;  RUNNING --WaitEvent--> WAITING --SetEvent--> READY
 */
#include "Os_Internal.h"

#define OS_LOCK()    uint32 os_lk = Os_Port_DisableAll()
#define OS_UNLOCK()  Os_Port_RestoreAll(os_lk)

/* ActivateTask core, shared with the alarm expiry action and StartOS autostart. Caller holds the lock.
 * OSEK 13.2.3.1: E_OS_LIMIT when the maximum number of activations is already reached. */
StatusType Os_ActivateInternal(TaskType t)
{
    Os_TcbType *tcb;
    const Os_TaskCfgType *cfg;
    uint8 limit;

    if (t >= (TaskType)Os_Config.numTasks) {
        return E_OS_ID;
    }
    tcb = &Os_Tcb[t];
    cfg = &Os_Config.tasks[t];
    limit = (cfg->extended != FALSE) ? 1u : cfg->maxActivations;   /* extended tasks cannot queue activations */
    if (tcb->activations >= limit) {
        return E_OS_LIMIT;
    }
    tcb->activations++;
    if (tcb->state == SUSPENDED) {                 /* first (non-queued) activation: make a fresh READY instance */
        tcb->state = READY;
        tcb->started = FALSE;
        tcb->eventsSet = 0u;                       /* OSEK: events are cleared when the task is activated */
        tcb->eventsWaited = 0u;
        tcb->curPriority = cfg->priority;
    }
    Os_Ready_Enqueue(t);                           /* every activation = one FIFO entry (activation order is kept) */
    return E_OK;
}

StatusType ActivateTask(TaskType TaskID)
{
    StatusType st;
    OS_LOCK();

    st = Os_ActivateInternal(TaskID);
    OS_UNLOCK();
    if (st != E_OK) {
        Os_ReportError(OSServiceId_ActivateTask, st);
        return st;
    }
    Os_Reschedule();                               /* a more urgent task preempts the caller right here */
    return E_OK;
}

/* Bookkeeping when the RUNNING task ends one activation (TerminateTask, ChainTask, body returned).
 * Caller holds the lock and calls Os_Port_RequestDispatch() afterwards. */
void Os_Task_Finish(void)
{
    Os_TcbType *tcb = &Os_Tcb[Os_Running];

    if (MINI_TRACE_OS_SWITCH == STD_ON) {
        TRACE(TRACE_CAT_OS, "TASK_END %s", Os_Config.tasks[Os_Running].name);
    }
    tcb->activations--;
    tcb->eventsSet = 0u;
    tcb->eventsWaited = 0u;
    tcb->curPriority = Os_Config.tasks[Os_Running].priority;
    if (tcb->activations > 0u) {                   /* queued activation: READY again, entry is already in the queue */
        tcb->state = READY;
        tcb->started = FALSE;                      /* ... and starts from the top of its entry function */
    } else {
        tcb->state = SUSPENDED;
        tcb->started = FALSE;
    }
}

/* Common precondition check of the services that give up the CPU (Terminate/Chain/Schedule/WaitEvent). */
static StatusType os_check_blocking_call(void)
{
    if (Os_InTaskContext() == FALSE) {
        return E_OS_CALLEVEL;                      /* from ISR2, hook or before StartOS */
    }
    if (Os_Interrupt_IsLocked() != FALSE) {
        return E_OS_DISABLEDINT;                   /* SWS_Os_00093 (only enforced for the blocking services here) */
    }
    if (Os_Tcb[Os_Running].resCount != 0u) {
        return E_OS_RESOURCE;                      /* OSEK: a task must not end/wait/schedule while holding a resource */
    }
    return E_OK;
}

StatusType TerminateTask(void)
{
    StatusType st = os_check_blocking_call();

    if (st != E_OK) {
        Os_ReportError(OSServiceId_TerminateTask, st);
        return st;
    }
    {
        OS_LOCK();
        Os_Task_Finish();
        OS_UNLOCK();
    }
    Os_Port_RequestDispatch();                     /* leaves this context for good */
    for (;;) { }                                   /* target: wait for PendSV; host never gets here */
}

StatusType ChainTask(TaskType TaskID)
{
    StatusType st = os_check_blocking_call();

    if ((st == E_OK) && (TaskID >= (TaskType)Os_Config.numTasks)) {
        st = E_OS_ID;
    }
    if (st == E_OK) {                              /* chaining to another task needs a free activation slot */
        const Os_TaskCfgType *cfg = &Os_Config.tasks[TaskID];
        uint8 limit = (cfg->extended != FALSE) ? 1u : cfg->maxActivations;

        if ((TaskID != Os_Running) && (Os_Tcb[TaskID].activations >= limit)) {
            st = E_OS_LIMIT;
        }
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_ChainTask, st);
        return st;
    }
    {
        OS_LOCK();
        Os_Task_Finish();                          /* terminate first ... (self-chain: slot is free again) */
        (void)Os_ActivateInternal(TaskID);         /* ... then activate, both inside one critical section */
        OS_UNLOCK();
    }
    Os_Port_RequestDispatch();
    for (;;) { }
}

StatusType Schedule(void)
{
    StatusType st = os_check_blocking_call();

    if (st != E_OK) {
        Os_ReportError(OSServiceId_Schedule, st);
        return st;
    }
    Os_Sched_Yield();                              /* DEVIATION from DESIGN 6.2 text: OSEK semantics, strictly higher priority only */
    return E_OK;
}

StatusType GetTaskID(TaskRefType TaskID)
{
    if (TaskID == NULL_PTR) {
        Os_ReportError(OSServiceId_GetTaskID, E_OS_VALUE);
        return E_OS_VALUE;
    }
    /* "the task that is running"; INVALID_TASK at ISR2 level, in the idle loop and before StartOS */
    *TaskID = ((Os_IsrDepth == 0u) && (Os_Running < (TaskType)Os_Config.numTasks)) ? Os_Running : INVALID_TASK;
    return E_OK;
}

StatusType GetTaskState(TaskType TaskID, TaskStateRefType State)
{
    if (TaskID >= (TaskType)Os_Config.numTasks) {
        Os_ReportError(OSServiceId_GetTaskState, E_OS_ID);
        return E_OS_ID;
    }
    if (State == NULL_PTR) {
        Os_ReportError(OSServiceId_GetTaskState, E_OS_VALUE);
        return E_OS_VALUE;
    }
    *State = Os_Tcb[TaskID].state;
    return E_OK;
}
