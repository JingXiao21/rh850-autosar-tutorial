// SOURCES: os/src/*.c os/port/host/*.c
// INCLUDES: tests/unit/cfg_os os/src
/*
 * test_os_events.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: OS conformance tests for event control of extended tasks (OSEK/VDX OS 2.2.3
 * chapter 13.5, AUTOSAR_CP_SWS_OS chapters 7.9.12/7.9.18).  Covers: blocking WaitEvent, wake-up by SetEvent
 * with preemption, events that are not waited for stay pending, WaitEvent returning at once for an already
 * set event, ClearEvent/GetEvent, events cleared on (re)activation, SetEvent from an alarm, and the error codes
 * E_OS_ID / E_OS_ACCESS / E_OS_STATE.
 */
#include "os_test_cfg.h"

/* Task_Ext (prio 8): waits for Ev1|Ev2, reports every event it was woken with, ends when Ev3 arrives */
static void ext_body(void)
{
    EventMaskType ev = 0u;

    for (;;) {
        CHECK_EQ(WaitEvent(Ev1 | Ev2), E_OK);
        CHECK_EQ(GetEvent(Task_Ext, &ev), E_OK);
        CHECK_EQ(ClearEvent(ev), E_OK);
        if ((ev & Ev1) != 0u) { os_log("1"); }
        if ((ev & Ev2) != 0u) { os_log("2"); }
        if ((ev & Ev3) != 0u) { os_log("3"); os_log("T"); (void)TerminateTask(); }
    }
}

/* a basic task must not use events */
static void mid_body(void)
{
    EXPECT_ERR(WaitEvent(Ev1), E_OS_ACCESS, OSServiceId_WaitEvent);
    EXPECT_ERR(ClearEvent(Ev1), E_OS_ACCESS, OSServiceId_ClearEvent);
    os_log("m");
    (void)TerminateTask();
}

static void script(void)
{
    EventMaskType ev = 0xFFu;
    TaskStateType st;

    os_test_body[5] = ext_body;
    os_test_body[2] = mid_body;

    /* ---- 1. Ext (8) preempts Main (6) at activation and blocks in WaitEvent */
    CHECK_EQ(ActivateTask(Task_Ext), E_OK);
    CHECK_EQ(GetTaskState(Task_Ext, &st), E_OK);
    CHECK_EQ(st, WAITING);
    CHECK_LOG("");

    /* ---- 2. SetEvent of a waited event: woken, runs immediately (higher priority) */
    CHECK_EQ(SetEvent(Task_Ext, Ev1), E_OK);
    CHECK_LOG("1");
    CHECK_EQ(GetTaskState(Task_Ext, &st), E_OK);
    CHECK_EQ(st, WAITING);                            /* it waits again */

    /* ---- 3. an event that is not waited for only becomes pending: no wake-up */
    CHECK_EQ(SetEvent(Task_Ext, Ev3), E_OK);
    CHECK_LOG("1");
    CHECK_EQ(GetEvent(Task_Ext, &ev), E_OK);
    CHECK_EQ(ev, Ev3);
    CHECK_EQ(GetTaskState(Task_Ext, &st), E_OK);
    CHECK_EQ(st, WAITING);

    /* ---- 4. waited event arrives: Ext sees BOTH pending events in one wake-up, then terminates */
    CHECK_EQ(SetEvent(Task_Ext, Ev2), E_OK);
    CHECK_LOG("123T");
    CHECK_EQ(GetTaskState(Task_Ext, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);

    /* ---- 5. error codes of the event services */
    EXPECT_ERR(SetEvent(Task_Ext, Ev1), E_OS_STATE, OSServiceId_SetEvent);        /* extended but SUSPENDED */
    EXPECT_ERR(GetEvent(Task_Ext, &ev), E_OS_STATE, OSServiceId_GetEvent);
    EXPECT_ERR(SetEvent(Task_Hi, Ev1), E_OS_ACCESS, OSServiceId_SetEvent);        /* basic task */
    EXPECT_ERR(GetEvent(Task_Hi, &ev), E_OS_ACCESS, OSServiceId_GetEvent);
    EXPECT_ERR(SetEvent((TaskType)99u, Ev1), E_OS_ID, OSServiceId_SetEvent);
    EXPECT_ERR(GetEvent((TaskType)99u, &ev), E_OS_ID, OSServiceId_GetEvent);

    /* ---- 6. basic task using events (ClearEvent/WaitEvent -> E_OS_ACCESS, checked inside Task_Mid) */
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);
    os_test_sleep_ms(1);
    CHECK_LOG("123Tm");

    /* ---- 7. WaitEvent returns at once if the event is already set; ClearEvent removes it */
    CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);
    CHECK_EQ(ClearEvent(0xFFFFFFFFu), E_OK);
    CHECK_EQ(SetEvent(Task_Main, Ev2), E_OK);
    CHECK_EQ(WaitEvent(Ev2), E_OK);                   /* must not block */
    CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);
    CHECK_EQ(ev, Ev2);
    CHECK_EQ(ClearEvent(Ev2), E_OK);
    CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);
    CHECK_EQ(ev, 0u);

    /* ---- 8. a re-activated extended task starts with all events cleared */
    os_log_clear();
    CHECK_EQ(ActivateTask(Task_Ext), E_OK);
    CHECK_EQ(GetEvent(Task_Ext, &ev), E_OK);
    CHECK_EQ(ev, 0u);
    EXPECT_ERR(ActivateTask(Task_Ext), E_OS_LIMIT, OSServiceId_ActivateTask);     /* extended: single activation */

    /* ---- 9. SetEvent from an alarm (Alarm_Ev -> Task_Ext/Ev1) 3 ms later */
    CHECK_EQ(SetRelAlarm(Alarm_Ev, 3u, 0u), E_OK);
    CHECK_EQ(GetAlarm(Alarm_Ev, &ev), E_OK);
    CHECK_EQ(ev, 3u);
    os_test_sleep_ms(2);
    CHECK_LOG("");                                    /* not yet */
    os_test_sleep_ms(2);
    CHECK_LOG("1");

    /* ---- 10. pending event + waited event again: one wake-up reports both */
    CHECK_EQ(SetEvent(Task_Ext, Ev3), E_OK);          /* not waited: stays pending, Ext remains WAITING */
    CHECK_EQ(SetEvent(Task_Ext, Ev2), E_OK);          /* wakes Ext: logs 2 3 T */
    CHECK_LOG("123T");                                /* "1" from step 9, then "23T" */
    os_test_done();
}

int main(void)
{
    os_test_start(script);
    return 1;
}
