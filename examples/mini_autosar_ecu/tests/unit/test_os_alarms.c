// SOURCES: os/src/*.c os/port/host/*.c
// INCLUDES: tests/unit/cfg_os os/src
/*
 * test_os_alarms.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: OS conformance tests for counters and alarms (OSEK/VDX OS 2.2.3 chapter 9/13.6,
 * AUTOSAR_CP_SWS_OS chapters 7.9.23/7.9.26, SWS_Os_00304).  Covers: relative, absolute and cyclic alarms with
 * the actions ACTIVATETASK / SETEVENT / CALLBACK, CancelAlarm, GetAlarm/GetAlarmBase, autostart alarm,
 * software counter with IncrementCounter incl. wrap-around, GetCounterValue/GetElapsedValue and the status
 * codes E_OS_ID / E_OS_VALUE / E_OS_STATE / E_OS_NOFUNC.
 * Virtual time: 1 tick = 1 ms, the system counter starts at 0 when StartOS runs.
 */
#include "os_test_cfg.h"

#define MAX_STAMPS 16u
static unsigned g_stamp[MAX_STAMPS];     /* ms timestamps at which Task_Hi ran */
static unsigned g_nStamp;

static unsigned now_ms(void) { return (unsigned)(SimTime_GetUs() / 1000u); }

static void hi_body(void)
{
    if (g_nStamp < MAX_STAMPS) {
        g_stamp[g_nStamp] = now_ms();
        g_nStamp++;
    }
    (void)TerminateTask();
}

static void lo_body(void)
{
    os_log("L");
    (void)TerminateTask();
}

static void script(void)
{
    TickType v;
    TickType rem;
    TickType el;
    AlarmBaseType base;
    unsigned t0;
    unsigned cb0;

    os_test_body[1] = hi_body;
    os_test_body[4] = lo_body;

    /* ---- 0. the autostart alarm (rel 7 ms, set in the configuration) sets Ev_Auto at t = 7 ms */
    os_test_sleep_ms(10);
    {
        EventMaskType ev;
        CHECK_EQ(GetEvent(Task_Main, &ev), E_OK);
        CHECK((ev & Ev_Auto) != 0u);
        CHECK_EQ(ClearEvent(Ev_Auto), E_OK);
    }
    CHECK_EQ(GetAlarm(Alarm_Auto, &rem), E_OS_NOFUNC);            /* single shot, expired -> not active */

    /* ---- 1. relative single-shot alarm: ACTIVATETASK Hi after 5 ms */
    t0 = now_ms();
    CHECK_EQ(SetRelAlarm(Alarm_Hi, 5u, 0u), E_OK);
    CHECK_EQ(GetAlarm(Alarm_Hi, &rem), E_OK);
    CHECK_EQ(rem, 5u);
    os_test_sleep_ms(2);
    CHECK_EQ(GetAlarm(Alarm_Hi, &rem), E_OK);
    CHECK_EQ(rem, 3u);
    CHECK_EQ(g_nStamp, 0u);
    os_test_sleep_ms(4);
    CHECK_EQ(g_nStamp, 1u);
    CHECK_EQ(g_stamp[0], t0 + 5u);
    EXPECT_ERR(GetAlarm(Alarm_Hi, &rem), E_OS_NOFUNC, OSServiceId_GetAlarm);

    /* ---- 2. cyclic alarm: first after 2 ms, then every 3 ms -> t0+2, +5, +8 */
    g_nStamp = 0u;
    t0 = now_ms();
    CHECK_EQ(SetRelAlarm(Alarm_Hi, 2u, 3u), E_OK);
    os_test_sleep_ms(9);                                          /* Wake alarm expires at t0+9 */
    CHECK_EQ(g_nStamp, 3u);
    CHECK_EQ(g_stamp[0], t0 + 2u);
    CHECK_EQ(g_stamp[1], t0 + 5u);
    CHECK_EQ(g_stamp[2], t0 + 8u);
    CHECK_EQ(GetAlarm(Alarm_Hi, &rem), E_OK);                     /* still running: next at t0+11 */
    CHECK_EQ(rem, 2u);

    /* ---- 3. CancelAlarm stops it; cancelling again is E_OS_NOFUNC */
    CHECK_EQ(CancelAlarm(Alarm_Hi), E_OK);
    EXPECT_ERR(CancelAlarm(Alarm_Hi), E_OS_NOFUNC, OSServiceId_CancelAlarm);
    os_test_sleep_ms(5);
    CHECK_EQ(g_nStamp, 3u);                                       /* no more activations */

    /* ---- 4. absolute alarm: counter value + 4 */
    g_nStamp = 0u;
    CHECK_EQ(GetCounterValue(Counter_System, &v), E_OK);
    CHECK_EQ(v, now_ms());                                        /* 1 tick = 1 ms, counter started at 0 */
    CHECK_EQ(SetAbsAlarm(Alarm_Hi, v + 4u, 0u), E_OK);
    os_test_sleep_ms(6);
    CHECK_EQ(g_nStamp, 1u);
    CHECK_EQ(g_stamp[0], (unsigned)v + 4u);

    /* ---- 5. elapsed time */
    CHECK_EQ(GetCounterValue(Counter_System, &v), E_OK);
    os_test_sleep_ms(7);
    CHECK_EQ(GetElapsedValue(Counter_System, &v, &el), E_OK);
    CHECK_EQ(el, 7u);

    /* ---- 6. alarm with a CALLBACK action, cyclic every 1 ms; 5 expiries while Main sleeps 5 ms */
    cb0 = g_cbCount;
    CHECK_EQ(SetRelAlarm(Alarm_Cb, 1u, 1u), E_OK);
    os_test_sleep_ms(5);
    CHECK_EQ(g_cbCount - cb0, 5u);
    CHECK_EQ(CancelAlarm(Alarm_Cb), E_OK);

    /* ---- 7. software counter 0..9 driven by IncrementCounter; alarm action ACTIVATETASK Lo */
    os_log_clear();
    EXPECT_ERR(IncrementCounter(Counter_System), E_OS_ID, OSServiceId_IncrementCounter);   /* hardware counter */
    CHECK_EQ(GetCounterValue(Counter_Sw, &v), E_OK);
    CHECK_EQ(v, 0u);
    CHECK_EQ(SetRelAlarm(Alarm_Sw, 3u, 0u), E_OK);
    CHECK_EQ(IncrementCounter(Counter_Sw), E_OK);
    CHECK_EQ(IncrementCounter(Counter_Sw), E_OK);
    os_test_sleep_ms(1);
    CHECK_LOG("");                                                /* not expired yet */
    CHECK_EQ(IncrementCounter(Counter_Sw), E_OK);                 /* 3rd increment: alarm fires, Lo READY */
    os_test_sleep_ms(1);
    CHECK_LOG("L");
    CHECK_EQ(GetCounterValue(Counter_Sw, &v), E_OK);
    CHECK_EQ(v, 3u);
    CHECK_EQ(GetAlarmBase(Alarm_Sw, &base), E_OK);
    CHECK_EQ(base.maxallowedvalue, 9u);
    CHECK_EQ(base.mincycle, 2u);
    CHECK_EQ(base.ticksperbase, 1u);

    /* ---- 8. wrap-around: absolute value 2 is "in the past" (counter is at 3) -> expires after the wrap (9 increments) */
    os_log_clear();
    CHECK_EQ(SetAbsAlarm(Alarm_Sw, 2u, 0u), E_OK);
    {
        unsigned i;
        for (i = 0u; i < 8u; i++) {
            CHECK_EQ(IncrementCounter(Counter_Sw), E_OK);
        }
    }
    os_test_sleep_ms(1);
    CHECK_LOG("");
    CHECK_EQ(IncrementCounter(Counter_Sw), E_OK);                 /* 9th: 3 -> 4..9 -> 0 -> 1 -> 2 */
    os_test_sleep_ms(1);
    CHECK_LOG("L");
    CHECK_EQ(GetCounterValue(Counter_Sw, &v), E_OK);
    CHECK_EQ(v, 2u);
    CHECK_EQ(GetElapsedValue(Counter_Sw, &v, &el), E_OK);         /* v == now: nothing elapsed since */
    CHECK_EQ(el, 0u);
    v = 8u;
    CHECK_EQ(GetElapsedValue(Counter_Sw, &v, &el), E_OK);         /* from 8 to 2 across the wrap: 4 ticks */
    CHECK_EQ(el, 4u);
    CHECK_EQ(v, 2u);

    /* ---- 9. argument errors (SWS_Os_00304: increment 0 -> E_OS_VALUE) */
    EXPECT_ERR(SetRelAlarm(Alarm_Hi, 0u, 0u), E_OS_VALUE, OSServiceId_SetRelAlarm);
    EXPECT_ERR(SetRelAlarm(Alarm_Hi, 0x10000u, 0u), E_OS_VALUE, OSServiceId_SetRelAlarm);   /* > maxallowedvalue */
    EXPECT_ERR(SetRelAlarm(Alarm_Sw, 1u, 1u), E_OS_VALUE, OSServiceId_SetRelAlarm);         /* cycle < mincycle (2) */
    EXPECT_ERR(SetAbsAlarm(Alarm_Sw, 10u, 0u), E_OS_VALUE, OSServiceId_SetAbsAlarm);        /* start > max (9) */
    EXPECT_ERR(SetRelAlarm((AlarmType)42u, 1u, 0u), E_OS_ID, OSServiceId_SetRelAlarm);
    EXPECT_ERR(SetAbsAlarm((AlarmType)42u, 1u, 0u), E_OS_ID, OSServiceId_SetAbsAlarm);
    EXPECT_ERR(CancelAlarm((AlarmType)42u), E_OS_ID, OSServiceId_CancelAlarm);
    EXPECT_ERR(GetAlarm((AlarmType)42u, &rem), E_OS_ID, OSServiceId_GetAlarm);
    EXPECT_ERR(GetAlarmBase((AlarmType)42u, &base), E_OS_ID, OSServiceId_GetAlarmBase);
    EXPECT_ERR(GetCounterValue((CounterType)9u, &v), E_OS_ID, OSServiceId_GetCounterValue);
    EXPECT_ERR(IncrementCounter((CounterType)9u), E_OS_ID, OSServiceId_IncrementCounter);

    /* ---- 10. starting a running alarm -> E_OS_STATE, the original expiry is kept */
    CHECK_EQ(SetRelAlarm(Alarm_Hi, 20u, 0u), E_OK);
    EXPECT_ERR(SetRelAlarm(Alarm_Hi, 5u, 0u), E_OS_STATE, OSServiceId_SetRelAlarm);
    EXPECT_ERR(SetAbsAlarm(Alarm_Hi, 5u, 0u), E_OS_STATE, OSServiceId_SetAbsAlarm);
    CHECK_EQ(GetAlarm(Alarm_Hi, &rem), E_OK);
    CHECK_EQ(rem, 20u);
    CHECK_EQ(CancelAlarm(Alarm_Hi), E_OK);

    os_test_done();
}

int main(void)
{
    os_test_start(script);
    return 1;
}
