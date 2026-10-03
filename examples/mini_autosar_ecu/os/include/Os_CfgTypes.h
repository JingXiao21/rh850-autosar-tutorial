/*
 * Os_CfgTypes.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the post-build/pre-compile OS configuration produced by the OS
 * generator from the ECUC description (containers OsTask, OsAlarm, OsCounter, OsResource, OsIsr,
 * OsOS/OsHooks). This header is THE CONTRACT between the kernel (os/src, owner agent A) and the
 * generated Os_Cfg.c (owner agent D): D fills `const Os_ConfigType Os_Config`, A reads it.
 *
 * Only the kernel (os/src and os/port sources) and the generated Os_Cfg.c include this header.
 */
#ifndef OS_CFGTYPES_H
#define OS_CFGTYPES_H

#include "Os.h"

typedef uint32 Os_StackElementType;          /* stacks are word arrays, 8-byte aligned (AAPCS) */

typedef enum { OS_SCHED_FULL = 0, OS_SCHED_NON = 1 } Os_SchedulePolicyType;  /* OsTaskSchedule */

typedef struct {
    const char            *name;             /* e.g. "Task_LightCtl" */
    void                 (*entry)(void);     /* Os_Task_<name> */
    uint8                  priority;         /* 1..255, higher = more urgent (OsTaskPriority) */
    uint8                  maxActivations;   /* OsTaskActivation, 1 for extended tasks */
    boolean                extended;         /* extended task: may use events (has OsTaskEvent) */
    Os_SchedulePolicyType  schedule;
    uint32                 autostartModes;   /* bit n set: autostart in AppMode n (OsTaskAutostart) */
    Os_StackElementType   *stackBase;        /* lowest address of the stack array (target port) */
    uint16                 stackWords;       /* size in 32-bit words (host port: fibers use a fixed size) */
} Os_TaskCfgType;

typedef struct {
    TickType  maxAllowedValue;               /* OsCounterMaxAllowedValue */
    TickType  ticksPerBase;                  /* OsCounterTicksPerBase */
    TickType  minCycle;                      /* OsCounterMinCycle */
    boolean   hardware;                      /* TRUE: driven by the OS tick (SysTick); FALSE: software counter */
} Os_CounterCfgType;

typedef enum {
    OS_ALARM_ACTIVATETASK = 0,               /* OsAlarmActivateTask */
    OS_ALARM_SETEVENT,                       /* OsAlarmSetEvent */
    OS_ALARM_CALLBACK,                       /* OsAlarmCallback */
    OS_ALARM_INCREMENTCOUNTER                /* OsAlarmIncrementCounter */
} Os_AlarmActionType;

typedef struct {
    const char          *name;
    CounterType          counter;
    Os_AlarmActionType   action;
    TaskType             task;               /* ACTIVATETASK / SETEVENT target */
    EventMaskType        event;              /* SETEVENT mask */
    void               (*callback)(void);    /* CALLBACK: Os_AlarmCb_<name> */
    CounterType          incCounter;         /* INCREMENTCOUNTER target */
    boolean              autostart;
    boolean              autostartAbsolute;  /* TRUE: SetAbsAlarm semantics */
    TickType             autostartTime;      /* alarm time (abs) or increment (rel) */
    TickType             autostartCycle;     /* 0 = single shot */
    uint32               autostartModes;     /* bit mask of AppModes */
} Os_AlarmCfgType;

typedef struct {
    const char *name;
    uint8       ceilingPriority;             /* computed by the generator = max priority of all accessors */
} Os_ResourceCfgType;                        /* RES_SCHEDULER (id 0) is implicit, ceiling = max task priority */

typedef struct {
    const char *name;
    void      (*handler)(void);              /* Os_Isr_<name> */
    uint16      irqNumber;                   /* NVIC IRQ number (target); host: simulated source id */
    uint8       nvicPriority;                /* raw 8-bit NVIC priority value, Cat2 must be >= OS_CM33_MAX_SYSCALL_PRIO */
} Os_IsrCfgType;                             /* all configured ISRs are Category 2 */

typedef struct {
    uint8                    numTasks;
    uint8                    numCounters;
    uint8                    numAlarms;
    uint8                    numResources;   /* user resources (without RES_SCHEDULER) */
    uint8                    numIsrs;
    const Os_TaskCfgType    *tasks;
    const Os_CounterCfgType *counters;
    const Os_AlarmCfgType   *alarms;
    const Os_ResourceCfgType*resources;
    const Os_IsrCfgType     *isrs;
    CounterType              systemCounter;  /* counter incremented by the tick */
    TickType                 tickDurationUs; /* 1000 */
} Os_ConfigType;

extern const Os_ConfigType Os_Config;        /* defined in generated Os_Cfg.c */

#endif /* OS_CFGTYPES_H */
