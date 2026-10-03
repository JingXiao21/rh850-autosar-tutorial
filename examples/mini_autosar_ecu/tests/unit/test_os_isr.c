// SOURCES: os/src/*.c os/port/host/*.c
// INCLUDES: tests/unit/cfg_os os/src
/*
 * test_os_isr.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: OS conformance tests for Category 2 interrupts (AUTOSAR_CP_SWS_OS chapter 7 ISR
 * handling, SWS_Os_00368 ISR ends with locked interrupts, OSEK/VDX OS 2.2.3 chapter 5 / 13.3).
 * Uses the host simulation: SimTime_RaiseIsr(irq) pends the simulated interrupt, Os_HostBurn() lets virtual time
 * pass so that ticks and interrupts are delivered.  Covers: ISR-triggered ActivateTask/SetEvent with preemption
 * when the ISR returns, GetISRID/GetTaskID inside an ISR, E_OS_CALLEVEL for services that are illegal at ISR
 * level, masking by SuspendOSInterrupts/DisableAllInterrupts, SWS_Os_00368 clean-up, and preemption of a
 * burning task by an alarm (tick) at the exact virtual time.
 */
#include "os_test_cfg.h"

static unsigned g_hiTimeMs;
static unsigned g_isrRuns;

static void hi_body(void)
{
    os_log("H");
    g_hiTimeMs = (unsigned)(SimTime_GetUs() / 1000u);
    (void)TerminateTask();
}

static void ext_body(void)
{
    for (;;) {
        EventMaskType ev;

        CHECK_EQ(WaitEvent(Ev1), E_OK);
        CHECK_EQ(GetEvent(Task_Ext, &ev), E_OK);
        CHECK_EQ(ClearEvent(ev), E_OK);
        os_log("E");
    }
}

/* ---- ISR bodies ----------------------------------------------------------------------------------------- */

static void isr_activate_hi(void)
{
    TaskType t = 0u;

    g_isrRuns++;
    os_log("i");
    CHECK_EQ(GetISRID(), Isr_Test);
    CHECK_EQ(GetTaskID(&t), E_OK);
    CHECK_EQ(t, INVALID_TASK);                        /* no task context at ISR2 level */
    CHECK_EQ(ActivateTask(Task_Hi), E_OK);
    os_log("j");                                      /* Hi must NOT run before the ISR has returned */
}

static void isr_set_event(void)
{
    g_isrRuns++;
    os_log("i");
    CHECK_EQ(SetEvent(Task_Ext, Ev1), E_OK);
}

static void isr_illegal_calls(void)
{
    unsigned n = g_errCount;
    ResourceType r = 1u;

    g_isrRuns++;
    EXPECT_ERR(TerminateTask(), E_OS_CALLEVEL, OSServiceId_TerminateTask);
    EXPECT_ERR(ChainTask(Task_Hi), E_OS_CALLEVEL, OSServiceId_ChainTask);
    EXPECT_ERR(Schedule(), E_OS_CALLEVEL, OSServiceId_Schedule);
    EXPECT_ERR(WaitEvent(Ev1), E_OS_CALLEVEL, OSServiceId_WaitEvent);
    EXPECT_ERR(ClearEvent(Ev1), E_OS_CALLEVEL, OSServiceId_ClearEvent);
    EXPECT_ERR(GetResource(r), E_OS_CALLEVEL, OSServiceId_GetResource);
    EXPECT_ERR(ReleaseResource(r), E_OS_CALLEVEL, OSServiceId_ReleaseResource);
    CHECK_EQ(g_errCount, n + 7u);
}

/* ISR forgets to resume interrupts and to ... (SWS_Os_00368): the OS repairs it and calls the ErrorHook */
static void isr_leaves_locked(void)
{
    g_isrRuns++;
    SuspendAllInterrupts();
    SuspendOSInterrupts();
    DisableAllInterrupts();
}

static void script(void)
{
    unsigned n;
    unsigned t0;
    TickType c0;
    TickType c1;
    TaskStateType st;

    os_test_body[1] = hi_body;
    os_test_body[5] = ext_body;
    CHECK_EQ(GetISRID(), INVALID_ISR);

    /* ---- 1. ISR activates Hi (9 > Main 6): Hi runs when the ISR has returned, before Os_HostBurn returns */
    os_test_isr_body = isr_activate_hi;
    SimTime_RaiseIsr(OS_TEST_IRQ);
    CHECK_LOG("");                                    /* an interrupt is only pending until a delivery point */
    Os_HostBurn(100u);
    CHECK_LOG("ijH");
    CHECK_EQ(g_isrRuns, 1u);
    CHECK_EQ(GetTaskState(Task_Hi, &st), E_OK);
    CHECK_EQ(st, SUSPENDED);

    /* ---- 2. ISR sets an event: Ext (8) is woken and preempts Main when the ISR ends */
    os_log_clear();
    CHECK_EQ(ActivateTask(Task_Ext), E_OK);           /* Ext runs, blocks in WaitEvent */
    os_test_isr_body = isr_set_event;
    SimTime_RaiseIsr(OS_TEST_IRQ);
    Os_HostBurn(10u);
    CHECK_LOG("iE");

    /* ---- 3. services that are illegal at ISR level fail with E_OS_CALLEVEL (checked inside the ISR) */
    os_test_isr_body = isr_illegal_calls;
    SimTime_RaiseIsr(OS_TEST_IRQ);
    Os_HostBurn(10u);
    CHECK_EQ(g_isrRuns, 3u);

    /* ---- 4. SuspendOSInterrupts masks the Cat2 interrupt; it is delivered right at ResumeOSInterrupts */
    os_log_clear();
    os_test_isr_body = isr_activate_hi;
    SuspendOSInterrupts();
    SuspendOSInterrupts();                            /* nested */
    SimTime_RaiseIsr(OS_TEST_IRQ);
    Os_HostBurn(100u);
    CHECK_LOG("");                                    /* masked */
    ResumeOSInterrupts();
    Os_HostBurn(1u);
    CHECK_LOG("");                                    /* still one level suspended */
    ResumeOSInterrupts();
    CHECK_LOG("ijH");                                 /* delivered immediately when the last level is resumed */

    /* ---- 5. DisableAllInterrupts / SuspendAllInterrupts mask it too */
    os_log_clear();
    DisableAllInterrupts();
    SimTime_RaiseIsr(OS_TEST_IRQ);
    Os_HostBurn(50u);
    CHECK_LOG("");
    EnableAllInterrupts();
    CHECK_LOG("ijH");
    os_log_clear();
    SuspendAllInterrupts();
    SimTime_RaiseIsr(OS_TEST_IRQ);
    Os_HostBurn(50u);
    CHECK_LOG("");
    ResumeAllInterrupts();
    CHECK_LOG("ijH");

    /* ---- 6. SWS_Os_00368: an ISR that returns with locked interrupts gets them unlocked + ErrorHook */
    os_test_isr_body = isr_leaves_locked;
    n = g_errCount;
    SimTime_RaiseIsr(OS_TEST_IRQ);
    Os_HostBurn(10u);
    CHECK_EQ(g_errCount, n + 1u);
    CHECK_EQ(g_errLast, E_OS_DISABLEDINT);
    CHECK_EQ(Schedule(), E_OK);                       /* interrupts are enabled again: blocking services work */
    CHECK_EQ(Os_Interrupt_IsLocked(), FALSE);

    /* ---- 7. the tick preempts a burning task at the exact time: Alarm_Hi in 3 ms, Main burns 5 ms */
    os_log_clear();
    t0 = (unsigned)(SimTime_GetUs() / 1000u);
    CHECK_EQ(GetCounterValue(Counter_System, &c0), E_OK);
    CHECK_EQ(SetRelAlarm(Alarm_Hi, 3u, 0u), E_OK);
    os_log("a");
    Os_HostBurn(5000u);
    os_log("b");
    CHECK_LOG("aHb");                                 /* Hi ran in the middle of the burn */
    CHECK_EQ(g_hiTimeMs, t0 + 3u);
    CHECK_EQ(GetCounterValue(Counter_System, &c1), E_OK);
    CHECK_EQ(c1 - c0, 5u);                            /* 5 ms burned = 5 ticks */

    /* ---- 8. unknown interrupt lines are ignored (no ISR configured) */
    n = g_isrRuns;
    SimTime_RaiseIsr(77u);
    Os_HostBurn(10u);
    CHECK_EQ(g_isrRuns, n);

    os_test_done();
}

int main(void)
{
    os_test_start(script);
    return 1;
}
