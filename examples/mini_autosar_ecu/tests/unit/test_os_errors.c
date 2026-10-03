// SOURCES: os/src/*.c os/port/host/*.c
// INCLUDES: tests/unit/cfg_os os/src
/*
 * test_os_errors.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: OS conformance tests for error handling (AUTOSAR_CP_SWS_OS: ErrorHook,
 * OSErrorGetServiceId, SWS_Os_00052/00069 E_OS_MISSINGEND, SWS_Os_00092 unmatched Resume/Enable ignored,
 * SWS_Os_00093 E_OS_DISABLEDINT, SWS_Os_00299 interrupt services usable before StartOS).
 * Every misuse must (a) return the documented status and (b) call the ErrorHook exactly once with that status
 * and the id of the failing service; the OS must stay usable afterwards.
 */
#include "os_test_cfg.h"

static void lo_missingend_body(void)
{
    os_log("L");
    /* falls off the end: no TerminateTask */
}

static void lo_callevel_body(void)
{
    os_log("l");
    (void)TerminateTask();
}

static void script(void)
{
    TaskType id = 0u;
    TaskStateType st = 0u;
    EventMaskType ev = 0u;
    unsigned n;

    /* ---- 1. NULL pointers / bad ids on the query services */
    EXPECT_ERR(GetTaskID(NULL_PTR), E_OS_VALUE, OSServiceId_GetTaskID);
    EXPECT_ERR(GetTaskState(Task_Hi, NULL_PTR), E_OS_VALUE, OSServiceId_GetTaskState);
    EXPECT_ERR(GetTaskState((TaskType)OS_NUM_TASKS, &st), E_OS_ID, OSServiceId_GetTaskState);
    EXPECT_ERR(ActivateTask(INVALID_TASK), E_OS_ID, OSServiceId_ActivateTask);
    EXPECT_ERR(ChainTask(INVALID_TASK), E_OS_ID, OSServiceId_ChainTask);
    CHECK_EQ(GetTaskID(&id), E_OK);
    CHECK_EQ(id, Task_Main);

    /* ---- 2. E_OS_MISSINGEND: the task is terminated by the OS and can be activated again */
    os_test_body[4] = lo_missingend_body;
    n = g_errCount;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    os_test_sleep_ms(1);
    CHECK_LOG("L");
    CHECK_EQ(g_errCount, n + 1u);
    CHECK_EQ(g_errLast, E_OS_MISSINGEND);
    CHECK_EQ(GetTaskState(Task_Lo, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);
    os_log_clear();
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);            /* still usable */
    os_test_sleep_ms(1);
    CHECK_LOG("L");

    /* ---- 3. E_OS_DISABLEDINT: blocking services refuse to run with interrupts suspended */
    SuspendOSInterrupts();
    EXPECT_ERR(WaitEvent(Ev1), E_OS_DISABLEDINT, OSServiceId_WaitEvent);
    EXPECT_ERR(Schedule(), E_OS_DISABLEDINT, OSServiceId_Schedule);
    EXPECT_ERR(TerminateTask(), E_OS_DISABLEDINT, OSServiceId_TerminateTask);
    EXPECT_ERR(ChainTask(Task_Hi), E_OS_DISABLEDINT, OSServiceId_ChainTask);
    ResumeOSInterrupts();
    SuspendAllInterrupts();
    SuspendAllInterrupts();                           /* nestable */
    EXPECT_ERR(WaitEvent(Ev1), E_OS_DISABLEDINT, OSServiceId_WaitEvent);
    ResumeAllInterrupts();
    EXPECT_ERR(WaitEvent(Ev1), E_OS_DISABLEDINT, OSServiceId_WaitEvent);   /* still one level suspended */
    ResumeAllInterrupts();
    DisableAllInterrupts();
    EXPECT_ERR(Schedule(), E_OS_DISABLEDINT, OSServiceId_Schedule);
    EnableAllInterrupts();
    CHECK_EQ(Schedule(), E_OK);                       /* all clear again */

    /* ---- 4. SWS_Os_00092: Resume/Enable without matching Suspend/Disable is ignored (no effect, no error) */
    n = g_errCount;
    ResumeAllInterrupts();
    ResumeOSInterrupts();
    EnableAllInterrupts();
    CHECK_EQ(g_errCount, n);
    CHECK_EQ(Schedule(), E_OK);

    /* ---- 5. alarm action errors go to the ErrorHook with the underlying service id (E_OS_LIMIT on activation) */
    os_test_body[4] = lo_callevel_body;
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);
    CHECK_EQ(ActivateTask(Task_Lo), E_OK);            /* Lo: 2 of 2 activations used while Main runs */
    n = g_errCount;
    CHECK_EQ(SetRelAlarm(Alarm_Sw, 1u, 0u), E_OK);    /* software counter: Alarm_Sw activates Task_Lo */
    CHECK_EQ(IncrementCounter(Counter_Sw), E_OK);     /* expires -> ActivateTask(Lo) fails inside the alarm */
    CHECK_EQ(g_errCount, n + 1u);
    CHECK_EQ(g_errLast, E_OS_LIMIT);
    CHECK_EQ(g_errSvc, OSServiceId_ActivateTask);
    os_test_sleep_ms(1);

    /* ---- 6. SetEvent via alarm to a SUSPENDED extended task -> E_OS_STATE reported by the hook */
    n = g_errCount;
    CHECK_EQ(SetRelAlarm(Alarm_Ev, 1u, 0u), E_OK);    /* targets Task_Ext, which is suspended */
    os_test_sleep_ms(2);
    CHECK_EQ(g_errCount, n + 1u);
    CHECK_EQ(g_errLast, E_OS_STATE);
    CHECK_EQ(g_errSvc, OSServiceId_SetEvent);

    /* ---- 7. ErrorHook/service id survive: last error info is still the SetEvent one, GetEvent works */
    CHECK_EQ(OSErrorGetServiceId(), OSServiceId_SetEvent);
    CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);

    os_test_done();
}

int main(void)
{
    /* SWS_Os_00299: the interrupt services work before StartOS (static state, zero initialised) */
    SuspendAllInterrupts();
    SuspendOSInterrupts();
    ResumeOSInterrupts();
    ResumeAllInterrupts();
    DisableAllInterrupts();
    EnableAllInterrupts();
    CHECK_EQ(GetISRID(), INVALID_ISR);
    os_test_start(script);
    return 1;
}
