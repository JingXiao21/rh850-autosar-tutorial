// SOURCES: os/src/*.c os/port/host/*.c
// INCLUDES: tests/unit/cfg_os os/src
/*
 * test_os_resources.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: OS conformance tests for resource management (OSEK/VDX OS 2.2.3 chapter 8,
 * AUTOSAR_CP_SWS_OS chapter 7.9.21, SWS_Os_00801 LIFO release, SWS_Os_00851..00854 RES_SCHEDULER).
 * Covers: priority ceiling (a task that holds a resource cannot be preempted by tasks up to the ceiling,
 * but by higher ones), preemption right at ReleaseResource, nesting, RES_SCHEDULER, and the error codes
 * E_OS_ID / E_OS_ACCESS / E_OS_NOFUNC / E_OS_RESOURCE; resources of a task that "falls off its end" are
 * released by the OS (SWS_Os_00070).
 * Cast: Res_A ceiling 4 (users Task_Lo 2, Task_Mid 4), Res_B ceiling 6 (users Task_Lo, Task_Main 6).
 */
#include "os_test_cfg.h"

static void hi_body(void)  { os_log("H"); (void)TerminateTask(); }
static void mid_body(void) { os_log("M"); (void)TerminateTask(); }

/* Lo takes Res_A (ceiling 4): Mid (4) must NOT preempt, Hi (9) MUST; Mid runs as soon as Lo releases. */
static void lo_ceiling_body(void)
{
    CHECK_EQ(GetResource(Res_A), E_OK);
    CHECK_EQ(Os_Tcb[Task_Lo].curPriority, 4u);        /* raised from 2 to the ceiling */
    os_log("g");
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);           /* priority 4 is not > 4: stays READY */
    os_log("a");
    CHECK_EQ(ActivateTask(Task_Hi), E_OK);            /* priority 9 > 4: preempts at once */
    os_log("b");
    CHECK_EQ(ReleaseResource(Res_A), E_OK);           /* priority back to 2 -> Mid (4) runs right here */
    os_log("r");
    CHECK_EQ(Os_Tcb[Task_Lo].curPriority, 2u);
    (void)TerminateTask();
}

/* Lo nests Res_A then Res_B (ceiling 6) and burns 5 ms: Main (6) becomes READY after 2 ms but must wait. */
static void lo_nested_body(void)
{
    CHECK_EQ(GetResource(Res_A), E_OK);
    CHECK_EQ(GetResource(Res_B), E_OK);
    CHECK_EQ(Os_Tcb[Task_Lo].curPriority, 6u);
    Os_HostBurn(5000u);                               /* virtual time: ticks arrive, Wake alarm sets Main's event */
    os_log("x");
    EXPECT_ERR(ReleaseResource(Res_A), E_OS_NOFUNC, OSServiceId_ReleaseResource);   /* LIFO violated: B is on top */
    CHECK_EQ(Os_Tcb[Task_Lo].resCount, 2u);           /* ... and nothing was released */
    CHECK_EQ(ReleaseResource(Res_B), E_OK);           /* priority 4: Main (6) preempts here */
    os_log("y");
    CHECK_EQ(Os_Tcb[Task_Lo].curPriority, 4u);
    CHECK_EQ(ReleaseResource(Res_A), E_OK);
    CHECK_EQ(Os_Tcb[Task_Lo].curPriority, 2u);
    (void)TerminateTask();
}

/* error cases inside a task that holds a resource */
static void lo_errors_body(void)
{
    CHECK_EQ(GetResource(Res_A), E_OK);
    EXPECT_ERR(GetResource(Res_A), E_OS_ACCESS, OSServiceId_GetResource);          /* already occupied (by us) */
    EXPECT_ERR(TerminateTask(), E_OS_RESOURCE, OSServiceId_TerminateTask);         /* must not end holding it */
    EXPECT_ERR(ChainTask(Task_Hi), E_OS_RESOURCE, OSServiceId_ChainTask);
    EXPECT_ERR(Schedule(), E_OS_RESOURCE, OSServiceId_Schedule);
    EXPECT_ERR(ReleaseResource(Res_B), E_OS_NOFUNC, OSServiceId_ReleaseResource);  /* not held */
    CHECK_EQ(ReleaseResource(Res_A), E_OK);
    os_log("e");
    (void)TerminateTask();
}

/* Mid ends without TerminateTask while holding Res_A: E_OS_MISSINGEND + the OS releases the resource */
static void mid_missingend_body(void)
{
    CHECK_EQ(GetResource(Res_A), E_OK);
    os_log("m");
    /* returns: no TerminateTask */
}

static void script(void)
{
    TaskStateType st;

    os_test_body[1] = hi_body;
    os_test_body[2] = mid_body;

    /* ---- 1. ceiling protocol: expected trace g a H b M r */
    os_test_body[4] = lo_ceiling_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    os_test_sleep_ms(2);
    CHECK_LOG("gaHbMr");

    /* ---- 2. nesting + LIFO + ceiling 6 keeps the equal-priority Main from running until the release */
    os_log_clear();
    os_test_body[4] = lo_nested_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    {
        EventMaskType ev;

        CHECK_EQ(SetRelAlarm(Alarm_Wake, 2u, 0u), E_OK);
        CHECK_EQ(WaitEvent(Ev_Wake), E_OK);           /* Lo runs, Wake alarm fires after 2 ms of Lo's burn */
        os_log("W");                                  /* only after Lo released Res_B */
        CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);
        CHECK_EQ(ClearEvent(Ev_Wake), E_OK);
    }
    os_test_sleep_ms(1);                              /* let Lo finish */
    CHECK_LOG("xWy");
    CHECK_EQ(GetTaskState(Task_Lo, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);

    /* ---- 3. error codes in the holder, and in Main */
    os_log_clear();
    os_test_body[4] = lo_errors_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    os_test_sleep_ms(1);
    CHECK_LOG("e");
    EXPECT_ERR(GetResource((ResourceType)9u), E_OS_ID, OSServiceId_GetResource);
    EXPECT_ERR(ReleaseResource((ResourceType)9u), E_OS_ID, OSServiceId_ReleaseResource);
    EXPECT_ERR(ReleaseResource(Res_B), E_OS_NOFUNC, OSServiceId_ReleaseResource);          /* never taken */
    EXPECT_ERR(GetResource(Res_A), E_OS_ACCESS, OSServiceId_GetResource);   /* Main (6) is above ceiling 4 */
    CHECK_EQ(GetResource(Res_B), E_OK);                                     /* ceiling 6: allowed */
    EXPECT_ERR(WaitEvent(Ev1), E_OS_RESOURCE, OSServiceId_WaitEvent);       /* cannot wait while holding */
    CHECK_EQ(ReleaseResource(Res_B), E_OK);

    /* ---- 4. RES_SCHEDULER: no task (even Hi, 9) can preempt while it is held */
    os_log_clear();
    CHECK_EQ(GetResource(RES_SCHEDULER), E_OK);
    CHECK_EQ(Os_Tcb[Task_Main].curPriority, 9u);
    CHECK_EQ(ActivateTask(Task_Hi), E_OK);
    CHECK_LOG("");
    CHECK_EQ(ReleaseResource(RES_SCHEDULER), E_OK);   /* Hi runs now */
    CHECK_LOG("H");
    CHECK_EQ(Os_Tcb[Task_Main].curPriority, 6u);

    /* ---- 5. task body returns while holding a resource (SWS_Os_00070) */
    os_log_clear();
    os_test_body[2] = mid_missingend_body;
    {
        unsigned n = g_errCount;

        CHECK_EQ(ActivateTask(Task_Mid), E_OK);
        os_test_sleep_ms(1);
        CHECK_LOG("m");
        CHECK_EQ(g_errCount, n + 1u);
        CHECK_EQ(g_errLast, E_OS_MISSINGEND);
        CHECK_EQ(Os_Tcb[Task_Mid].resCount, 0u);      /* released by the OS */
        CHECK_EQ(Os_Tcb[Task_Mid].curPriority, 4u);
        CHECK_EQ(GetTaskState(Task_Mid, &st), E_OK);
        CHECK_EQ(st, SUSPENDED);
        os_test_body[2] = mid_body;                   /* and the resource is free again: Lo can take it */
    }
    os_test_body[4] = lo_ceiling_body;
    os_log_clear();
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    os_test_sleep_ms(2);
    CHECK_LOG("gaHbMr");

    os_test_done();
}

int main(void)
{
    os_test_start(script);
    return 1;
}
