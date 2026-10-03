/*
 * Os.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Os.h of AUTOSAR OS (AUTOSAR_CP_SWS_OS, scalability class SC1 subset),
 * which itself is the OSEK/VDX OS 2.2.3 API. Implemented: basic + extended tasks, fixed-priority
 * preemptive scheduling, events, counters/alarms, priority-ceiling resources, Cat2 ISRs,
 * interrupt-control services, hooks. NOT implemented: schedule tables, OS-Applications, memory
 * protection, timing protection, Cat1 ISR API, multicore, ScheduleTable/IOC/Spinlocks.
 *
 * Conventions: priorities are numbers where a HIGHER number = MORE urgent (0 = idle, reserved).
 * Object ids (Task_*, Alarm_*, Res_*, Counter_*, Event masks) come from the generated Os_Cfg.h,
 * which is included at the bottom (after the types it needs).
 * Services are callable from the contexts listed in the OSEK service table; an illegal context
 * returns E_OS_CALLEVEL (STANDARD status; no extended-status checks beyond those listed in DESIGN).
 */
#ifndef OS_H
#define OS_H

#include "Std_Types.h"

/* HOST-BUILD LINK FIX (added by agent A, no signature changed): TDM-GCC always links libwinpthread, which pulls
 * kernel32's import thunk "SetEvent" (a Win32 API of the same name).  A project function called SetEvent would then
 * collide at link time ("multiple definition of SetEvent").  On the host build the OS service is therefore
 * linked under the name Os_SetEvent; all sources that include Os.h still write SetEvent(). */
#if defined(MINI_PLATFORM_HOST)
#define SetEvent                Os_SetEvent
#endif

/* ---------------------------------------------------------------- types */
typedef uint8   StatusType;       /* E_OK or E_OS_* */
typedef uint8   TaskType;         /* index into Os_Config.tasks, 0..N-1; INVALID_TASK = 0xFF */
typedef TaskType *TaskRefType;
typedef uint8   TaskStateType;
typedef TaskStateType *TaskStateRefType;
typedef uint32  EventMaskType;
typedef EventMaskType *EventMaskRefType;
typedef uint8   AlarmType;
typedef uint8   CounterType;
typedef uint32  TickType;
typedef TickType *TickRefType;
typedef uint8   ResourceType;
typedef uint8   AppModeType;      /* OSDEFAULTAPPMODE = 0 */
typedef uint8   ISRType;
typedef uint8   OSServiceIdType;

typedef struct {
    TickType maxallowedvalue;     /* counter wraps after this value */
    TickType ticksperbase;
    TickType mincycle;
} AlarmBaseType;
typedef AlarmBaseType *AlarmBaseRefType;

#define INVALID_TASK            ((TaskType)0xFFu)
#define INVALID_ISR             ((ISRType)0xFFu)
#define OSDEFAULTAPPMODE        ((AppModeType)0u)
#define RES_SCHEDULER           ((ResourceType)0u)   /* implicit resource; user resources get ids 1..N (Os_Config.resources[id-1]) */

/* task states (OSEK 4.2) */
#define RUNNING                 ((TaskStateType)0u)
#define WAITING                 ((TaskStateType)1u)
#define READY                   ((TaskStateType)2u)
#define SUSPENDED               ((TaskStateType)3u)

/* status codes: OSEK 13.1 + AUTOSAR OS 8.3 additions (numeric values as in AUTOSAR OS) */
#define E_OS_ACCESS             ((StatusType)1u)
#define E_OS_CALLEVEL           ((StatusType)2u)
#define E_OS_ID                 ((StatusType)3u)
#define E_OS_LIMIT              ((StatusType)4u)   /* too many activations / alarm already used */
#define E_OS_NOFUNC             ((StatusType)5u)   /* alarm not in use, ... */
#define E_OS_RESOURCE           ((StatusType)6u)   /* task terminates/waits while holding a resource */
#define E_OS_STATE              ((StatusType)7u)   /* e.g. SetEvent on suspended task */
#define E_OS_VALUE              ((StatusType)8u)
#define E_OS_SERVICEID          ((StatusType)9u)
#define E_OS_ILLEGAL_ADDRESS    ((StatusType)10u)
#define E_OS_MISSINGEND         ((StatusType)11u)  /* task body returned without TerminateTask */
#define E_OS_DISABLEDINT        ((StatusType)12u)
#define E_OS_STACKFAULT         ((StatusType)13u)  /* stack canary overwritten (educational check) */

/* ---------------------------------------------------------------- definition macros */
/* TASK(Task_Foo) { ... } defines the entry function referenced by the generated Os_Cfg.c. */
#define TASK(name)              void Os_Task_##name(void)
#define ISR(name)               void Os_Isr_##name(void)
#define ALARMCALLBACK(name)     void Os_AlarmCb_##name(void)
#define DeclareTask(name)       extern void Os_Task_##name(void)
#define DeclareISR(name)        extern void Os_Isr_##name(void)
#define DeclareAlarmCallback(n) extern void Os_AlarmCb_##n(void)
#define DeclareEvent(name)      /* events are plain masks in Os_Cfg.h */
#define DeclareResource(name)   /* ids in Os_Cfg.h */
#define DeclareAlarm(name)      /* ids in Os_Cfg.h */
#define DeclareCounter(name)    /* ids in Os_Cfg.h */

/* generated object ids, AppMode ids, counts and hook enable switches (OS_USE_STARTUPHOOK, ...) */
#include "Os_Cfg.h"

/* ---------------------------------------------------------------- OS control */
void            StartOS(AppModeType Mode);              /* does not return */
void            ShutdownOS(StatusType Error);           /* calls ShutdownHook; target: halts; host: ends sim */
AppModeType     GetActiveApplicationMode(void);

/* ---------------------------------------------------------------- task management */
StatusType      ActivateTask(TaskType TaskID);
StatusType      TerminateTask(void);                    /* does not return on success */
StatusType      ChainTask(TaskType TaskID);             /* does not return on success */
StatusType      Schedule(void);
StatusType      GetTaskID(TaskRefType TaskID);          /* INVALID_TASK if none runs (e.g. ISR2 level) */
StatusType      GetTaskState(TaskType TaskID, TaskStateRefType State);

/* ---------------------------------------------------------------- interrupt handling */
void            EnableAllInterrupts(void);
void            DisableAllInterrupts(void);
void            ResumeAllInterrupts(void);              /* nestable */
void            SuspendAllInterrupts(void);
void            ResumeOSInterrupts(void);               /* nestable; masks only Cat2 (BASEPRI on target) */
void            SuspendOSInterrupts(void);
ISRType         GetISRID(void);

/* ---------------------------------------------------------------- resources */
StatusType      GetResource(ResourceType ResID);        /* priority ceiling; RES_SCHEDULER = block scheduler */
StatusType      ReleaseResource(ResourceType ResID);    /* LIFO order required */

/* ---------------------------------------------------------------- events (extended tasks only) */
StatusType      SetEvent(TaskType TaskID, EventMaskType Mask);
StatusType      ClearEvent(EventMaskType Mask);         /* calling extended task only */
StatusType      GetEvent(TaskType TaskID, EventMaskRefType Event);
StatusType      WaitEvent(EventMaskType Mask);          /* may block; resource must not be held */

/* ---------------------------------------------------------------- counters and alarms */
StatusType      GetCounterValue(CounterType CounterID, TickRefType Value);
StatusType      GetElapsedValue(CounterType CounterID, TickRefType Value, TickRefType ElapsedValue);
StatusType      IncrementCounter(CounterType CounterID);   /* software counters; hardware counter is driven by the tick */
StatusType      GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType Info);
StatusType      GetAlarm(AlarmType AlarmID, TickRefType Tick);
StatusType      SetRelAlarm(AlarmType AlarmID, TickType increment, TickType cycle);   /* increment > 0 */
StatusType      SetAbsAlarm(AlarmType AlarmID, TickType start, TickType cycle);
StatusType      CancelAlarm(AlarmType AlarmID);

/* ---------------------------------------------------------------- hooks (user supplied, see integration/Os_Hooks.c) */
void            StartupHook(void);
void            ShutdownHook(StatusType Error);
void            ErrorHook(StatusType Error);
void            PreTaskHook(void);
void            PostTaskHook(void);
/* OSEK helper macros usable inside ErrorHook: kernel stores the failing service id and first argument */
OSServiceIdType OSErrorGetServiceId(void);

/* ---------------------------------------------------------------- educational extras (not AUTOSAR) */
/* Highest stack usage in bytes of a task, from the fill pattern (0 if unknown); host port returns 0. */
uint32          Os_GetTaskStackUsage(TaskType TaskID);
/* Name string of a task / ISR from the generated config, for trace output. */
const char     *Os_GetTaskName(TaskType TaskID);

#endif /* OS_H */
