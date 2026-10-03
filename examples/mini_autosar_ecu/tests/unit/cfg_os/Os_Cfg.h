/*
 * Os_Cfg.h  (unit-test OS configuration: object ids)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the generated Os_Cfg.h of an OS generator.  Hand-written for the OS unit tests
 * (tests/unit/test_os_*.c); the tables behind these ids are in os_test_cfg.h. It is found first because the tests pass this directory first via "// INCLUDES:".
 */
#ifndef OS_CFG_H
#define OS_CFG_H

#define OS_NUM_TASKS        7u
#define OS_NUM_COUNTERS     2u
#define OS_NUM_ALARMS       6u
#define OS_NUM_RESOURCES    2u
#define OS_NUM_ISRS         1u

#define OS_USE_STARTUPHOOK  0
#define OS_USE_ERRORHOOK    1
#define OS_USE_SHUTDOWNHOOK 1
#define OS_USE_PRETASKHOOK  1
#define OS_USE_POSTTASKHOOK 1

/* tasks (index into Os_Config.tasks) */
#define Task_Main           ((TaskType)0u)   /* prio 6, extended, autostart: the test script runs here      */
#define Task_Hi             ((TaskType)1u)   /* prio 9, basic                                               */
#define Task_Mid            ((TaskType)2u)   /* prio 4, basic, 3 activations                                 */
#define Task_Mid2           ((TaskType)3u)   /* prio 4, basic, same priority as Task_Mid                     */
#define Task_Lo             ((TaskType)4u)   /* prio 2, basic, 2 activations                                 */
#define Task_Ext            ((TaskType)5u)   /* prio 8, extended                                             */
#define Task_Non            ((TaskType)6u)   /* prio 3, basic, NON-preemptive                                */

#define Counter_System      ((CounterType)0u)
#define Counter_Sw          ((CounterType)1u)   /* software counter, 0..9, mincycle 2 */

#define Alarm_Hi            ((AlarmType)0u)  /* ACTIVATETASK Task_Hi            (system counter) */
#define Alarm_Ev            ((AlarmType)1u)  /* SETEVENT     Task_Ext Ev1       (system counter) */
#define Alarm_Cb            ((AlarmType)2u)  /* CALLBACK                         (system counter) */
#define Alarm_Wake          ((AlarmType)3u)  /* SETEVENT     Task_Main Ev_Wake  (system counter) */
#define Alarm_Sw            ((AlarmType)4u)  /* ACTIVATETASK Task_Lo            (software counter) */
#define Alarm_Auto          ((AlarmType)5u)  /* autostart rel 7: SETEVENT Task_Main Ev_Auto */

#define Res_A               ((ResourceType)1u)   /* ceiling 4 (Task_Lo, Task_Mid use it) */
#define Res_B               ((ResourceType)2u)   /* ceiling 6 (Task_Lo, Task_Main use it) */

#define Isr_Test            ((ISRType)0u)

#define Ev1                 ((EventMaskType)0x01u)
#define Ev2                 ((EventMaskType)0x02u)
#define Ev3                 ((EventMaskType)0x04u)
#define Ev_Auto             ((EventMaskType)0x40u)
#define Ev_Wake             ((EventMaskType)0x80u)

#define OS_TEST_IRQ         39u

#endif
