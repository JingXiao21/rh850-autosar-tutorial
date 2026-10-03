/*
 * Os_Internal.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (the internal data model of an OS kernel is vendor specific; the SWS only
 * prescribes behaviour). This header is PRIVATE to the files in os/src and the OS unit tests: it holds the Task
 * Control Block (TCB), the kernel-global state and the helpers that the kernel files share.
 * Spec: AUTOSAR_CP_SWS_OS chapter 7 (behaviour), OSEK/VDX OS 2.2.3 chapter 4 (task state model).
 *
 * Kernel file map
 *   Os_Core.c       StartOS/ShutdownOS, error reporting, ready queue, dispatcher decision, ISR wrapper, hooks
 *   Os_Task.c       ActivateTask/TerminateTask/ChainTask/Schedule/GetTaskID/GetTaskState
 *   Os_Event.c      SetEvent/ClearEvent/GetEvent/WaitEvent
 *   Os_Alarm.c      counters, alarms, system tick handler
 *   Os_Resource.c   GetResource/ReleaseResource (priority ceiling protocol, RES_SCHEDULER)
 *   Os_Interrupt.c  Suspend/Resume/Enable/DisableAllInterrupts, Suspend/ResumeOSInterrupts, GetISRID
 *
 * Locking rule used everywhere: kernel data is only touched with "all interrupts locked"
 * (Os_Port_DisableAll, nest-safe because it returns the previous state).  Tracing/hook calls are made while
 * locked only where they cannot block; services that may switch task do so AFTER unlocking, through
 * Os_Reschedule() / Os_Port_RequestDispatch().
 */
#ifndef OS_INTERNAL_H
#define OS_INTERNAL_H

#include "Os.h"
#include "Os_CfgTypes.h"
#include "Os_Port.h"
#include "Trace.h"

/* ------------------------------------------------------------------ limits of this small kernel */
#define OS_IDLE_TASK            ((TaskType)OS_NUM_TASKS)   /* TCB index of the idle task (always the last one) */
#define OS_MAX_PRIORITY         31u                        /* 32 levels -> one uint32 ready bitmap */
#define OS_QUEUE_DEPTH          16u                        /* ready entries per priority level (power of two) */
#define OS_MAX_HELD_RESOURCES   4u                         /* resources one task may hold at the same time */
#define OS_MAX_ISR_NEST         8u
#define OS_STACK_FILL           0xDEADBEEFu                /* stack paint pattern (target port) */

/* ------------------------------------------------------------------ service ids for OSErrorGetServiceId
 * OSEK 13.1 defines the macro names OSServiceId_<Service>; AUTOSAR leaves the numeric values to the
 * implementation (OSServiceIdType).  Values here are this project's own. */
#define OSServiceId_ActivateTask        ((OSServiceIdType)0x01u)
#define OSServiceId_TerminateTask       ((OSServiceIdType)0x02u)
#define OSServiceId_ChainTask           ((OSServiceIdType)0x03u)
#define OSServiceId_Schedule            ((OSServiceIdType)0x04u)
#define OSServiceId_GetTaskID           ((OSServiceIdType)0x05u)
#define OSServiceId_GetTaskState        ((OSServiceIdType)0x06u)
#define OSServiceId_GetResource         ((OSServiceIdType)0x07u)
#define OSServiceId_ReleaseResource     ((OSServiceIdType)0x08u)
#define OSServiceId_SetEvent            ((OSServiceIdType)0x09u)
#define OSServiceId_ClearEvent          ((OSServiceIdType)0x0Au)
#define OSServiceId_GetEvent            ((OSServiceIdType)0x0Bu)
#define OSServiceId_WaitEvent           ((OSServiceIdType)0x0Cu)
#define OSServiceId_GetAlarmBase        ((OSServiceIdType)0x0Du)
#define OSServiceId_GetAlarm            ((OSServiceIdType)0x0Eu)
#define OSServiceId_SetRelAlarm         ((OSServiceIdType)0x0Fu)
#define OSServiceId_SetAbsAlarm         ((OSServiceIdType)0x10u)
#define OSServiceId_CancelAlarm         ((OSServiceIdType)0x11u)
#define OSServiceId_GetCounterValue     ((OSServiceIdType)0x12u)
#define OSServiceId_GetElapsedValue     ((OSServiceIdType)0x13u)
#define OSServiceId_IncrementCounter    ((OSServiceIdType)0x14u)
#define OSServiceId_StartOS             ((OSServiceIdType)0x15u)
#define OSServiceId_IsrExit             ((OSServiceIdType)0x16u)   /* ISR ended with locked interrupts / resources */

/* ------------------------------------------------------------------ Task Control Block */
typedef struct {
    TaskStateType  state;                 /* RUNNING / WAITING / READY / SUSPENDED (OSEK 4.2) */
    uint8          activations;           /* pending + running instances; state==SUSPENDED <=> 0 */
    uint8          curPriority;           /* dynamic priority: base, or raised by a resource ceiling */
    boolean        started;               /* FALSE: next dispatch must build a fresh context (Os_Port_InitTaskContext) */
    uint8          resCount;              /* number of resources currently held */
    uint8          resId[OS_MAX_HELD_RESOURCES];    /* LIFO stack of held resources ...                     */
    uint8          resPrio[OS_MAX_HELD_RESOURCES];  /* ... and the curPriority that was valid before each GetResource */
    EventMaskType  eventsSet;             /* extended tasks: events that are set */
    EventMaskType  eventsWaited;          /* mask of the pending WaitEvent (0 if not waiting) */
} Os_TcbType;

/* ------------------------------------------------------------------ kernel globals (defined in Os_Core.c) */
extern Os_TcbType Os_Tcb[OS_NUM_TASKS + 1u];   /* [OS_IDLE_TASK] is the idle task's TCB (only partly used) */
extern TaskType   Os_Running;                  /* task that owns the CPU; INVALID_TASK before the first dispatch */
extern boolean    Os_Started;                  /* TRUE once the scheduler may switch tasks */
extern uint8      Os_IsrDepth;                 /* nesting depth of Cat2 ISR wrappers */
extern ISRType    Os_IsrIds[OS_MAX_ISR_NEST];  /* stack of running Cat2 ISR ids */
extern uint8      Os_MaxTaskPriority;          /* ceiling of RES_SCHEDULER */

/* Is the caller a task (not ISR/hook-less startup/idle)? */
boolean Os_InTaskContext(void);

/* error reporting: remembers the service id (OSErrorGetServiceId) and calls ErrorHook */
void    Os_ReportError(OSServiceIdType svc, StatusType err);

/* ready queue / scheduling (caller holds the kernel lock for Enqueue) */
void    Os_Ready_Enqueue(TaskType t);          /* append at the tail of the queue of tcb.curPriority */
void    Os_Reschedule(void);                   /* ask the port for a switch if the highest READY task beats the running one */
void    Os_Sched_Yield(void);                  /* Schedule(): switch if a strictly higher priority task is READY */

/* task internals */
StatusType Os_ActivateInternal(TaskType t);    /* no context checks, no ErrorHook; caller holds the lock */
void       Os_Task_Finish(void);               /* bookkeeping of the running task ending (Terminate/Chain/return) */
/* event internals */
StatusType Os_SetEventInternal(TaskType t, EventMaskType mask);  /* caller holds the lock */
/* resource internals */
void    Os_Resource_Init(void);
void    Os_Resource_ReleaseAll(TaskType t);
/* alarm internals */
void    Os_Alarm_Init(AppModeType mode);       /* counters := 0, alarms inactive, autostart alarms */
/* interrupt-service internals */
boolean Os_Interrupt_IsLocked(void);           /* some Suspend/Disable service is active (SWS_Os_00093) */
uint32  Os_Interrupt_Snapshot(void);           /* packed nesting state, taken at Cat2 ISR entry */
boolean Os_Interrupt_Unwind(uint32 snapshot);  /* undo what an ISR left locked; TRUE if something had to be undone */

#endif /* OS_INTERNAL_H */
