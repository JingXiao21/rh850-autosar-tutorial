// SOURCES: os/src/*.c os/port/host/*.c
// INCLUDES: tests/unit/cfg_os os/src
/*
 * test_os_sched.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: OS conformance tests for task management (AUTOSAR_CP_SWS_OS + OSEK/VDX OS 2.2.3
 * chapter 13.2).  Covers: fixed-priority preemption, FIFO order of equal priorities, multiple activation and
 * E_OS_LIMIT, TerminateTask with queued activations, ChainTask (other task / itself / error cases),
 * Schedule() of a non-preemptive task, GetTaskID/GetTaskState, Pre/PostTaskHook being called.
 * Runs on the host port (fibers + virtual time); exit code 0 = pass.
 */
#include "os_test_cfg.h"

static unsigned g_midRuns;

static void hi_body(void)   { os_log("H"); (void)TerminateTask(); }
static void lo_body(void)   { os_log("L"); (void)TerminateTask(); }
static void mid_body(void)  { os_log("M"); (void)TerminateTask(); }
static void mid2_body(void) { os_log("N"); (void)TerminateTask(); }

/* Task_Lo: log, then hand over to Task_Hi with ChainTask (after a failing chain to an invalid id) */
static void lo_chain_body(void)
{
    TaskType me = INVALID_TASK;

    (void)GetTaskID(&me);
    CHECK_EQ(me, Task_Lo);
    EXPECT_ERR(ChainTask(0x7Fu), E_OS_ID, OSServiceId_ChainTask);     /* task continues after the error */
    os_log("L");
    (void)ChainTask(Task_Hi);
    os_log("!");                                                      /* never reached */
}

/* Task_Mid chains to itself once: a task may re-activate itself through ChainTask */
static void mid_selfchain_body(void)
{
    os_log("M");
    g_midRuns++;
    if (g_midRuns == 1u) {
        (void)ChainTask(Task_Mid);
    }
    (void)TerminateTask();
}

/* Task_Non (non-preemptive) activates a more urgent task: it must NOT be preempted until Schedule() */
static void non_body(void)
{
    os_log("a");
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);
    os_log("b");                         /* Mid (4) > Non (3) but Non is non-preemptive: still running */
    CHECK_EQ(Schedule(), E_OK);          /* explicit rescheduling point: Mid runs now */
    os_log("c");
    (void)TerminateTask();
}

/* Task_Non: ChainTask to a task whose activation limit is exhausted -> E_OS_LIMIT, caller keeps running */
static void non_chainlimit_body(void)
{
    EXPECT_ERR(ChainTask(Task_Lo), E_OS_LIMIT, OSServiceId_ChainTask);
    os_log("n");
    (void)TerminateTask();
}

static void script(void)
{
    TaskType id = INVALID_TASK;
    TaskStateType st = 0xFFu;

    /* ---- identity and state of the running script task */
    CHECK_EQ(GetTaskID(&id), E_OK);
    CHECK_EQ(id, Task_Main);
    CHECK_EQ(GetTaskState(Task_Main, &st), E_OK);
    CHECK_EQ(st, RUNNING);
    CHECK_EQ(GetTaskState(Task_Lo, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);
    EXPECT_ERR(GetTaskState(Task_Lo + 100u, &st), E_OS_ID, OSServiceId_GetTaskState);

    /* ---- 1. priority preemption: Lo(2) waits, Hi(9) preempts Main(6) immediately */
    os_test_body[1] = hi_body;
    os_test_body[4] = lo_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    CHECK_LOG("");                                    /* Lo cannot run while Main (6) is running */
    CHECK_EQ(GetTaskState(Task_Lo, &st), E_OK);
    CHECK_EQ(st, READY);
    CHECK_EQ(ActivateTask(Task_Hi), E_OK);
    CHECK_LOG("H");                                   /* Hi ran to completion before ActivateTask returned */
    CHECK_EQ(GetTaskState(Task_Main, &st), E_OK);
    CHECK_EQ(st, RUNNING);                            /* ... and Main is RUNNING again */
    os_test_sleep_ms(2);                              /* Main waits -> Lo gets the CPU */
    CHECK_LOG("HL");
    CHECK_EQ(GetTaskState(Task_Lo, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);

    /* ---- 2. equal priority is FIFO per ACTIVATION: Mid, Mid2, Mid  ->  M N M */
    os_log_clear();
    os_test_body[2] = mid_body;
    os_test_body[3] = mid2_body;
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);
    CHECK_EQ(ActivateTask(Task_Mid2), E_OK);
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);           /* second (queued) activation of Mid */
    EXPECT_ERR(ActivateTask(Task_Mid2), E_OS_LIMIT, OSServiceId_ActivateTask);   /* Mid2: 1 activation configured */
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);           /* third activation: the configured maximum of Mid */
    EXPECT_ERR(ActivateTask(Task_Mid), E_OS_LIMIT, OSServiceId_ActivateTask);
    EXPECT_ERR(ActivateTask(Task_Lo + 50u), E_OS_ID, OSServiceId_ActivateTask);
    CHECK_LOG("");
    os_test_sleep_ms(2);
    CHECK_LOG("MNMM");
    CHECK_EQ(GetTaskState(Task_Mid, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);

    /* ---- 3. ChainTask: Lo chains to Hi; Hi runs right after Lo has ended */
    os_log_clear();
    os_test_body[4] = lo_chain_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    os_test_sleep_ms(2);
    CHECK_LOG("LH");
    CHECK_EQ(GetTaskState(Task_Lo, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);

    /* ---- 4. ChainTask to itself: Mid terminates and is re-activated atomically */
    os_log_clear();
    os_test_body[2] = mid_selfchain_body;
    g_midRuns = 0u;
    CHECK_EQ(ActivateTask(Task_Mid), E_OK);
    os_test_sleep_ms(2);
    CHECK_LOG("MM");
    CHECK_EQ(g_midRuns, 2u);

    /* ---- 5. non-preemptive task: only Schedule() lets the higher priority Mid run */
    os_log_clear();
    os_test_body[2] = mid_body;
    os_test_body[6] = non_body;
    CHECK_EQ(ActivateTask(Task_Non), E_OK);
    os_test_sleep_ms(2);
    CHECK_LOG("abMc");

    /* ---- 6. ChainTask limit error: Lo has 2 activations configured and both are used */
    os_log_clear();
    os_test_body[4] = lo_body;
    os_test_body[6] = non_chainlimit_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    CHECK_EQ(ActivateTask(Task_Non), E_OK);           /* Non(3) runs before Lo(2) when Main sleeps */
    os_test_sleep_ms(2);
    CHECK_LOG("nLL");

    /* ---- 7. Schedule() with nothing more urgent is a no-op for Main */
    CHECK_EQ(Schedule(), E_OK);

    /* ---- hooks were called around the task switches */
    CHECK(g_preCount > 10u);
    CHECK(g_postCount > 10u);
    os_test_done();
}

int main(void)
{
    os_test_start(script);
    return 1;                                         /* not reached: StartOS does not return */
}
