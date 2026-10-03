/*
 * Os_Resource.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Resource management of the AUTOSAR OS (GetResource, ReleaseResource, RES_SCHEDULER),
 * based on the OSEK Priority Ceiling Protocol.  Spec: AUTOSAR_CP_SWS_OS R25-11 chapter 7.9.21 (resource
 * handling), SWS_Os_00801 (strict LIFO release, E_OS_NOFUNC on violation), SWS_Os_00851..00854
 * (RES_SCHEDULER blocks all other tasks); the rest is OSEK/VDX OS 2.2.3 chapter 8 + 13.4.
 * Implemented: task-level resources with ceiling priority (generator computes the ceiling), RES_SCHEDULER
 * (id 0, ceiling = highest task priority), nesting up to OS_MAX_HELD_RESOURCES.
 * NOT implemented: resources taken by Cat2 ISRs (GetResource from an ISR returns E_OS_CALLEVEL),
 * internal resources, spinlocks.
 *
 * Why it works: while a task holds a resource its dynamic priority is raised to the ceiling = the highest
 * base priority of any task that uses the resource.  No such task can then preempt it, so the critical
 * section is mutually exclusive without disabling interrupts and without priority inversion/deadlock.
 *      priority
 *         ^     Task_Ctl(3) ........  <-- ceiling of Res_EA_Odo = 3
 *         |     Task_Act(2) ==GetResource==>raised to 3 ==ReleaseResource==> back to 2
 */
#include "Os_Internal.h"

static TaskType s_resOwner[OS_NUM_RESOURCES + 1u];       /* [0] = RES_SCHEDULER; INVALID_TASK = free */

void Os_Resource_Init(void)
{
    uint8 i;

    for (i = 0u; i <= OS_NUM_RESOURCES; i++) {
        s_resOwner[i] = INVALID_TASK;
    }
}

static uint8 res_ceiling(ResourceType id)
{
    return (id == RES_SCHEDULER) ? Os_MaxTaskPriority : Os_Config.resources[id - 1u].ceilingPriority;
}

StatusType GetResource(ResourceType ResID)
{
    StatusType st = E_OK;
    uint32 lk;
    Os_TcbType *tcb;

    if (ResID > (ResourceType)Os_Config.numResources) {
        st = E_OS_ID;
    } else if (Os_InTaskContext() == FALSE) {
        st = E_OS_CALLEVEL;
    } else {
        tcb = &Os_Tcb[Os_Running];
        if ((s_resOwner[ResID] != INVALID_TASK) ||                         /* already occupied (also by the caller) */
            (Os_Config.tasks[Os_Running].priority > res_ceiling(ResID))) { /* task not covered by the ceiling */
            st = E_OS_ACCESS;
        } else if (tcb->resCount >= OS_MAX_HELD_RESOURCES) {
            st = E_OS_LIMIT;
        }
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_GetResource, st);
        return st;
    }
    tcb = &Os_Tcb[Os_Running];
    lk = Os_Port_DisableAll();
    tcb->resId[tcb->resCount] = ResID;
    tcb->resPrio[tcb->resCount] = tcb->curPriority;       /* remember the priority to restore on release */
    tcb->resCount++;
    s_resOwner[ResID] = Os_Running;
    if (res_ceiling(ResID) > tcb->curPriority) {
        tcb->curPriority = res_ceiling(ResID);            /* priority ceiling: now nobody that uses it can preempt us */
    }
    Os_Port_RestoreAll(lk);
    return E_OK;
}

/* pop the top of the holder's stack (caller holds the lock) */
static void res_pop(Os_TcbType *tcb)
{
    tcb->resCount--;
    s_resOwner[tcb->resId[tcb->resCount]] = INVALID_TASK;
    tcb->curPriority = tcb->resPrio[tcb->resCount];
}

StatusType ReleaseResource(ResourceType ResID)
{
    StatusType st = E_OK;
    uint32 lk;
    Os_TcbType *tcb;

    if (ResID > (ResourceType)Os_Config.numResources) {
        st = E_OS_ID;
    } else if (Os_InTaskContext() == FALSE) {
        st = E_OS_CALLEVEL;
    } else {
        tcb = &Os_Tcb[Os_Running];
        if ((s_resOwner[ResID] != Os_Running) ||                          /* not held by the caller ... */
            (tcb->resId[tcb->resCount - 1u] != ResID)) {                  /* ... or not the most recent one: LIFO violated */
            st = E_OS_NOFUNC;                                             /* SWS_Os_00801 */
        }
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_ReleaseResource, st);
        return st;
    }
    tcb = &Os_Tcb[Os_Running];
    lk = Os_Port_DisableAll();
    res_pop(tcb);
    Os_Port_RestoreAll(lk);
    Os_Reschedule();                                       /* priority dropped: a waiting higher task may run now */
    return E_OK;
}

/* SWS_Os_00070: a task that ends while still holding resources gets them released by the OS. */
void Os_Resource_ReleaseAll(TaskType t)
{
    uint32 lk;

    if (t >= (TaskType)OS_NUM_TASKS) {
        return;
    }
    lk = Os_Port_DisableAll();
    while (Os_Tcb[t].resCount != 0u) {
        res_pop(&Os_Tcb[t]);
    }
    Os_Port_RestoreAll(lk);
}
