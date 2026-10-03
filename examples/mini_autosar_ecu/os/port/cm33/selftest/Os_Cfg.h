/*
 * Os_Cfg.h  (OS-only target smoke test: object ids)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the generated Os_Cfg.h of an OS generator.  Hand-written for the OS-only Cortex-M33
 * smoke test (os/port/cm33/selftest); NOT part of the ECU images (those use gen/<Ecu>/Os_Cfg.h).
 */
#ifndef OS_CFG_H
#define OS_CFG_H

#define OS_NUM_TASKS        3u
#define OS_NUM_COUNTERS     1u
#define OS_NUM_ALARMS       3u
#define OS_NUM_RESOURCES    1u
#define OS_NUM_ISRS         1u

#define OS_USE_STARTUPHOOK  1
#define OS_USE_ERRORHOOK    1
#define OS_USE_SHUTDOWNHOOK 1
#define OS_USE_PRETASKHOOK  0
#define OS_USE_POSTTASKHOOK 0

#define Task_Low            ((TaskType)0u)    /* prio 1, basic, autostart: long running background job      */
#define Task_Ext            ((TaskType)1u)    /* prio 3, extended, autostart: waits for events               */
#define Task_Hi             ((TaskType)2u)    /* prio 5, basic: activated by Alarm_Hi                        */

#define Counter_System      ((CounterType)0u)

#define Alarm_Hi            ((AlarmType)0u)   /* t = 20 ms, single shot: ACTIVATETASK Task_Hi                */
#define Alarm_Ev            ((AlarmType)1u)   /* t = 50 ms, every 50 ms: SETEVENT Task_Ext Ev1               */
#define Alarm_Cb            ((AlarmType)2u)   /* t = 10 ms, every 10 ms: CALLBACK (counts)                   */

#define Res_X               ((ResourceType)1u) /* ceiling 5 (Task_Low and Task_Hi use it)                    */

#define Isr_Sw              ((ISRType)0u)     /* software-triggered IRQ 10 (NVIC_ISPR)                       */
#define SELFTEST_IRQ        10u

#define Ev1                 ((EventMaskType)0x1u)
#define Ev2                 ((EventMaskType)0x2u)

#endif
