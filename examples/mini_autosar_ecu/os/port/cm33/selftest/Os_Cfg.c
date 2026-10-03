/*
 * Os_Cfg.c  (OS-only target smoke test: configuration tables)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the generated Os_Cfg.c.  Shows what the generator (generator/gen_rte.py) produces
 * for the real ECUs, in miniature: task table with stacks in .os_stack, counter, alarms, resource ceiling, ISR.
 */
#include "Os_CfgTypes.h"
#include "MemMap.h"

#define STACK_WORDS   256u                          /* 1 KB per task */

DeclareTask(Task_Low);
DeclareTask(Task_Ext);
DeclareTask(Task_Hi);
DeclareISR(Isr_Sw);
DeclareAlarmCallback(Alarm_Cb);

MINI_VAR_OS_STACK static Os_StackElementType s_stackLow[STACK_WORDS];
MINI_VAR_OS_STACK static Os_StackElementType s_stackExt[STACK_WORDS];
MINI_VAR_OS_STACK static Os_StackElementType s_stackHi[STACK_WORDS];

MINI_CONST_CFG static const Os_TaskCfgType s_tasks[OS_NUM_TASKS] = {
    { "Task_Low", Os_Task_Task_Low, 1u, 1u, FALSE, OS_SCHED_FULL, 0x1u, s_stackLow, STACK_WORDS },
    { "Task_Ext", Os_Task_Task_Ext, 3u, 1u, TRUE,  OS_SCHED_FULL, 0x1u, s_stackExt, STACK_WORDS },
    { "Task_Hi",  Os_Task_Task_Hi,  5u, 1u, FALSE, OS_SCHED_FULL, 0u,   s_stackHi,  STACK_WORDS },
};

MINI_CONST_CFG static const Os_CounterCfgType s_counters[OS_NUM_COUNTERS] = {
    { 0xFFFFu, 1u, 1u, TRUE },                      /* system counter: 1 tick = 1 ms */
};

MINI_CONST_CFG static const Os_AlarmCfgType s_alarms[OS_NUM_ALARMS] = {
    /* name        ctr action                  task      event  callback             inc  auto   abs    time cycle modes */
    { "Alarm_Hi", 0u, OS_ALARM_ACTIVATETASK, Task_Hi,  0u,  NULL_PTR,             0u, TRUE,  FALSE, 20u, 0u,  0x1u },
    { "Alarm_Ev", 0u, OS_ALARM_SETEVENT,     Task_Ext, Ev1, NULL_PTR,             0u, TRUE,  FALSE, 50u, 50u, 0x1u },
    { "Alarm_Cb", 0u, OS_ALARM_CALLBACK,     0u,       0u,  Os_AlarmCb_Alarm_Cb,  0u, TRUE,  FALSE, 10u, 10u, 0x1u },
};

MINI_CONST_CFG static const Os_ResourceCfgType s_resources[OS_NUM_RESOURCES] = {
    { "Res_X", 5u },                                /* = max(priority of Task_Low, Task_Hi) */
};

MINI_CONST_CFG static const Os_IsrCfgType s_isrs[OS_NUM_ISRS] = {
    { "Isr_Sw", Os_Isr_Isr_Sw, SELFTEST_IRQ, 0x50u },
};

MINI_CONST_CFG const Os_ConfigType Os_Config = {
    OS_NUM_TASKS, OS_NUM_COUNTERS, OS_NUM_ALARMS, OS_NUM_RESOURCES, OS_NUM_ISRS,
    s_tasks, s_counters, s_alarms, s_resources, s_isrs,
    0u,                                             /* systemCounter */
    1000u                                           /* tickDurationUs */
};
