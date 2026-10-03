/*
 * Os_Event.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Event control of the AUTOSAR OS (SetEvent, ClearEvent, GetEvent, WaitEvent) for
 * EXTENDED tasks.  R25-11 Os SWS (chapter 7.9.12 event setting, 7.9.18 waiting for events): behaviour is
 * OSEK/VDX OS 2.2.3 chapter 13.5; the SWS adds a spinlock check for WaitEvent (not applicable here, no
 * spinlocks) and SWS_Os_00093 (E_OS_DISABLEDINT).  Implemented: per-task 32-bit event mask, blocking WaitEvent,
 * wake-up by SetEvent from task/ISR/alarm, events cleared at (re)activation.
 *
 * Semantics (OSEK 13.5):
 *   SetEvent(t, m)  : t |= m; if t is WAITING for one of them -> READY (queued behind equal priority tasks)
 *   WaitEvent(m)    : returns at once if (set & m) != 0, else the caller becomes WAITING (releases the CPU)
 *   ClearEvent(m)   : only for the calling extended task; GetEvent reads without clearing.
 * Status codes: E_OS_ID (bad task), E_OS_ACCESS (not an extended task), E_OS_STATE (target SUSPENDED),
 *               E_OS_CALLEVEL (ClearEvent/WaitEvent from ISR), E_OS_RESOURCE (WaitEvent holding a resource).
 */
#include "Os_Internal.h"

/* SetEvent core, also used by the alarm action SETEVENT.  Caller holds the lock. */
StatusType Os_SetEventInternal(TaskType t, EventMaskType mask)
{
    Os_TcbType *tcb;

    if (t >= (TaskType)Os_Config.numTasks) {
        return E_OS_ID;
    }
    if (Os_Config.tasks[t].extended == FALSE) {
        return E_OS_ACCESS;                        /* basic tasks own no events */
    }
    tcb = &Os_Tcb[t];
    if (tcb->state == SUSPENDED) {
        return E_OS_STATE;                         /* events of a suspended task are meaningless */
    }
    tcb->eventsSet |= mask;
    TRACE(TRACE_CAT_OS, "EVENT_SET %s mask=0x%x", Os_Config.tasks[t].name, (unsigned)mask);
    if ((tcb->state == WAITING) && ((tcb->eventsSet & tcb->eventsWaited) != 0u)) {
        tcb->eventsWaited = 0u;
        tcb->state = READY;                        /* started stays TRUE: it resumes inside WaitEvent */
        Os_Ready_Enqueue(t);
    }
    return E_OK;
}

StatusType SetEvent(TaskType TaskID, EventMaskType Mask)
{
    StatusType st;
    uint32 lk = Os_Port_DisableAll();

    st = Os_SetEventInternal(TaskID, Mask);
    Os_Port_RestoreAll(lk);
    if (st != E_OK) {
        Os_ReportError(OSServiceId_SetEvent, st);
        return st;
    }
    Os_Reschedule();                               /* the woken task may preempt the caller */
    return E_OK;
}

StatusType ClearEvent(EventMaskType Mask)
{
    StatusType st = E_OK;

    if (Os_InTaskContext() == FALSE) {
        st = E_OS_CALLEVEL;
    } else if (Os_Config.tasks[Os_Running].extended == FALSE) {
        st = E_OS_ACCESS;
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_ClearEvent, st);
        return st;
    }
    {
        uint32 lk = Os_Port_DisableAll();

        Os_Tcb[Os_Running].eventsSet &= ~Mask;
        Os_Port_RestoreAll(lk);
    }
    return E_OK;
}

StatusType GetEvent(TaskType TaskID, EventMaskRefType Event)
{
    StatusType st = E_OK;

    if (TaskID >= (TaskType)Os_Config.numTasks) {
        st = E_OS_ID;
    } else if (Os_Config.tasks[TaskID].extended == FALSE) {
        st = E_OS_ACCESS;
    } else if (Os_Tcb[TaskID].state == SUSPENDED) {
        st = E_OS_STATE;
    } else if (Event == NULL_PTR) {
        st = E_OS_VALUE;
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_GetEvent, st);
        return st;
    }
    *Event = Os_Tcb[TaskID].eventsSet;
    return E_OK;
}

StatusType WaitEvent(EventMaskType Mask)
{
    StatusType st = E_OK;
    Os_TcbType *tcb;

    if (Os_InTaskContext() == FALSE) {
        st = E_OS_CALLEVEL;
    } else if (Os_Config.tasks[Os_Running].extended == FALSE) {
        st = E_OS_ACCESS;
    } else if (Os_Tcb[Os_Running].resCount != 0u) {
        st = E_OS_RESOURCE;
    } else if (Os_Interrupt_IsLocked() != FALSE) {
        st = E_OS_DISABLEDINT;
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_WaitEvent, st);
        return st;
    }
    tcb = &Os_Tcb[Os_Running];
    {
        uint32 lk = Os_Port_DisableAll();

        if ((tcb->eventsSet & Mask) != 0u) {       /* event already there: no blocking (OSEK) */
            Os_Port_RestoreAll(lk);
            return E_OK;
        }
        tcb->state = WAITING;
        tcb->eventsWaited = Mask;
        if (MINI_TRACE_OS_SWITCH == STD_ON) {
            TRACE(TRACE_CAT_OS, "TASK_WAIT %s mask=0x%x", Os_Config.tasks[Os_Running].name, (unsigned)Mask);
        }
        Os_Port_RestoreAll(lk);
    }
    Os_Port_RequestDispatch();                     /* we continue here after SetEvent made us READY and the dispatcher chose us */
    return E_OK;
}
