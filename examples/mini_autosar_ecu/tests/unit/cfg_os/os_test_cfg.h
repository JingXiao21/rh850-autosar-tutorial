/*
 * os_test_cfg.h  (unit-test OS configuration: tables + test harness)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the generated Os_Cfg.c (const Os_ConfigType Os_Config) plus a tiny test harness.
 * Included by exactly ONE translation unit per test executable (tests/unit/test_os_*.c) because it DEFINES
 * Os_Config, the task entry points and the hooks.  Every task body is a function pointer (os_test_body[]) that
 * the test fills, so each test can script its own behaviour while the configuration stays shared.
 *
 * Test model: Task_Main (priority 6) runs the script.  To let LOWER priority tasks run it sleeps with
 * os_test_sleep_ms(): that is SetRelAlarm(Alarm_Wake) + WaitEvent, i.e. it uses the OS under test and virtual
 * time (host port) to move on deterministically.  Observations are appended to a string log (os_log) and
 * compared with CHECK_LOG.  The test passes if Task_Main reaches os_test_done() (-> ShutdownOS(E_OK)).
 */
#ifndef OS_TEST_CFG_H
#define OS_TEST_CFG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Os_CfgTypes.h"
#include "Os_Internal.h"
#include "SimTime.h"

#define OS_T_UNUSED   __attribute__((unused))

/* ------------------------------------------------------------------ tiny assertion + log helpers */
#define CHECK(cond) \
    do { if (!(cond)) { printf("CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); fflush(stdout); exit(1); } } while (0)
#define CHECK_EQ(a, b) \
    do { unsigned long a_ = (unsigned long)(a); unsigned long b_ = (unsigned long)(b); \
         if (a_ != b_) { printf("CHECK_EQ FAILED %s:%d: %s (=%lu) != %s (=%lu)\n", __FILE__, __LINE__, #a, a_, #b, b_); \
                         fflush(stdout); exit(1); } } while (0)
#define CHECK_LOG(expected) \
    do { if (strcmp(g_log, (expected)) != 0) { printf("LOG MISMATCH %s:%d: got \"%s\" expected \"%s\"\n", \
                                                       __FILE__, __LINE__, g_log, (expected)); fflush(stdout); exit(1); } } while (0)

static char g_log[256];
static OS_T_UNUSED void os_log(const char *s) { strcat(g_log, s); }
static OS_T_UNUSED void os_log_clear(void)    { g_log[0] = '\0'; }

/* ------------------------------------------------------------------ hooks (recorded for the tests) */
static StatusType      g_errLast;
static OSServiceIdType g_errSvc;
static unsigned        g_errCount;
static unsigned        g_preCount;
static unsigned        g_postCount;
static boolean         g_done;

void ErrorHook(StatusType Error)
{
    g_errLast = Error;
    g_errSvc = OSErrorGetServiceId();
    g_errCount++;
}

void PreTaskHook(void)
{
    g_preCount++;
}

void PostTaskHook(void)
{
    g_postCount++;
}

void ShutdownHook(StatusType Error)
{
    printf("[test] ShutdownHook err=%u done=%u errors=%u pre=%u post=%u\n", (unsigned)Error, (unsigned)g_done,
           g_errCount, g_preCount, g_postCount);
    fflush(stdout);
    if ((Error != E_OK) || (g_done == FALSE)) {
        printf("[test] FAIL (shutdown before the script finished)\n");
        fflush(stdout);
        exit(1);
    }
}

/* ------------------------------------------------------------------ call-and-expect-an-error helper */
/* run `call`, expect status `st` and that ErrorHook ran exactly once with that status and service id */
#define EXPECT_ERR(call, st, svc) \
    do { unsigned n_ = g_errCount; StatusType r_ = (call); \
         CHECK_EQ(r_, (st)); CHECK_EQ(g_errCount, n_ + 1u); CHECK_EQ(g_errLast, (st)); CHECK_EQ(g_errSvc, (svc)); } while (0)

/* ------------------------------------------------------------------ task entry points */
static void (*os_test_body[OS_NUM_TASKS])(void);

TASK(Task_Main) { if (os_test_body[0] != NULL_PTR) { os_test_body[0](); } }
TASK(Task_Hi)   { if (os_test_body[1] != NULL_PTR) { os_test_body[1](); } }
TASK(Task_Mid)  { if (os_test_body[2] != NULL_PTR) { os_test_body[2](); } }
TASK(Task_Mid2) { if (os_test_body[3] != NULL_PTR) { os_test_body[3](); } }
TASK(Task_Lo)   { if (os_test_body[4] != NULL_PTR) { os_test_body[4](); } }
TASK(Task_Ext)  { if (os_test_body[5] != NULL_PTR) { os_test_body[5](); } }
TASK(Task_Non)  { if (os_test_body[6] != NULL_PTR) { os_test_body[6](); } }

/* ------------------------------------------------------------------ ISR + alarm callback */
static void (*os_test_isr_body)(void);
static volatile unsigned g_cbCount;

ISR(Isr_Test)               { if (os_test_isr_body != NULL_PTR) { os_test_isr_body(); } }
ALARMCALLBACK(Alarm_Cb)     { g_cbCount++; }

/* ------------------------------------------------------------------ configuration tables */
static const Os_TaskCfgType os_test_tasks[OS_NUM_TASKS] = {
    /* name        entry              prio act ext    sched          autostart stack     stackWords */
    { "Task_Main", Os_Task_Task_Main, 6u, 1u, TRUE,  OS_SCHED_FULL, 0x1u,     NULL_PTR, 0u },
    { "Task_Hi",   Os_Task_Task_Hi,   9u, 1u, FALSE, OS_SCHED_FULL, 0u,       NULL_PTR, 0u },
    { "Task_Mid",  Os_Task_Task_Mid,  4u, 3u, FALSE, OS_SCHED_FULL, 0u,       NULL_PTR, 0u },
    { "Task_Mid2", Os_Task_Task_Mid2, 4u, 1u, FALSE, OS_SCHED_FULL, 0u,       NULL_PTR, 0u },
    { "Task_Lo",   Os_Task_Task_Lo,   2u, 2u, FALSE, OS_SCHED_FULL, 0u,       NULL_PTR, 0u },
    { "Task_Ext",  Os_Task_Task_Ext,  8u, 1u, TRUE,  OS_SCHED_FULL, 0u,       NULL_PTR, 0u },
    { "Task_Non",  Os_Task_Task_Non,  3u, 1u, FALSE, OS_SCHED_NON,  0u,       NULL_PTR, 0u },
};

static const Os_CounterCfgType os_test_counters[OS_NUM_COUNTERS] = {
    /* max     ticksPerBase minCycle hardware */
    { 0xFFFFu, 1u, 1u, TRUE  },    /* system counter, 1 tick = 1 ms */
    { 9u,      1u, 2u, FALSE },    /* software counter 0..9 */
};

static const Os_AlarmCfgType os_test_alarms[OS_NUM_ALARMS] = {
    /* name         ctr action                  task       event     callback             incCtr autostart abs    time cycle modes */
    { "Alarm_Hi",   0u, OS_ALARM_ACTIVATETASK, Task_Hi,   0u,       NULL_PTR,            0u, FALSE, FALSE, 0u, 0u, 0u },
    { "Alarm_Ev",   0u, OS_ALARM_SETEVENT,     Task_Ext,  Ev1,      NULL_PTR,            0u, FALSE, FALSE, 0u, 0u, 0u },
    { "Alarm_Cb",   0u, OS_ALARM_CALLBACK,     0u,        0u,       Os_AlarmCb_Alarm_Cb, 0u, FALSE, FALSE, 0u, 0u, 0u },
    { "Alarm_Wake", 0u, OS_ALARM_SETEVENT,     Task_Main, Ev_Wake,  NULL_PTR,            0u, FALSE, FALSE, 0u, 0u, 0u },
    { "Alarm_Sw",   1u, OS_ALARM_ACTIVATETASK, Task_Lo,   0u,       NULL_PTR,            0u, FALSE, FALSE, 0u, 0u, 0u },
    { "Alarm_Auto", 0u, OS_ALARM_SETEVENT,     Task_Main, Ev_Auto,  NULL_PTR,            0u, TRUE,  FALSE, 7u, 0u, 0x1u },
};

static const Os_ResourceCfgType os_test_resources[OS_NUM_RESOURCES] = {
    { "Res_A", 4u },
    { "Res_B", 6u },
};

static const Os_IsrCfgType os_test_isrs[OS_NUM_ISRS] = {
    { "Isr_Test", Os_Isr_Isr_Test, OS_TEST_IRQ, 0x40u },
};

const Os_ConfigType Os_Config = {
    OS_NUM_TASKS, OS_NUM_COUNTERS, OS_NUM_ALARMS, OS_NUM_RESOURCES, OS_NUM_ISRS,
    os_test_tasks, os_test_counters, os_test_alarms, os_test_resources, os_test_isrs,
    0u /* systemCounter */, 1000u /* tickDurationUs */
};

/* ------------------------------------------------------------------ harness API for the test scripts */

/* Sleep `ms` of virtual time in a way that lets lower priority tasks run: relative alarm -> Ev_Wake, WaitEvent. */
static OS_T_UNUSED void os_test_sleep_ms(TickType ms)
{
    EventMaskType ev;

    CHECK_EQ(SetRelAlarm(Alarm_Wake, ms, 0u), E_OK);
    CHECK_EQ(WaitEvent(Ev_Wake), E_OK);
    CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);
    CHECK((ev & Ev_Wake) != 0u);
    CHECK_EQ(ClearEvent(Ev_Wake), E_OK);
}

/* end of the script: everything before this point passed */
static OS_T_UNUSED void os_test_done(void)
{
    g_done = TRUE;
    printf("[test] script finished\n");
    ShutdownOS(E_OK);
}

/* start the OS with `script` running in Task_Main; never returns */
static OS_T_UNUSED void os_test_start(void (*script)(void))
{
    os_test_body[0] = script;
    SimTime_SetEndUs(10000000u);         /* safety net: a hung test ends after 10 s virtual time with g_done == FALSE */
    StartOS(OSDEFAULTAPPMODE);
}

#endif /* OS_TEST_CFG_H */
