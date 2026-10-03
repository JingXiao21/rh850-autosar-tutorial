/*
 * Os_Alarm.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Counter and Alarm handling of the AUTOSAR OS (GetCounterValue, GetElapsedValue,
 * IncrementCounter, GetAlarmBase, GetAlarm, SetRelAlarm, SetAbsAlarm, CancelAlarm).
 * Spec: AUTOSAR_CP_SWS_OS R25-11 chapter 7.9.23 (counters), 7.9.26 (alarms), SWS_Os_00304 (SetRelAlarm with
 * increment 0 -> E_OS_VALUE), SWS_Os_00399 IncrementCounter / SWS_Os_00383 GetCounterValue /
 * SWS_Os_00392 GetElapsedValue; alarm semantics are OSEK/VDX OS 2.2.3 chapter 9 + 13.6.
 * Implemented: one hardware counter driven by the tick (the "system counter") and optional software
 * counters, alarms with the actions ACTIVATETASK / SETEVENT / CALLBACK / INCREMENTCOUNTER, single-shot and
 * cyclic alarms, relative and absolute start, autostart per application mode.
 * NOT implemented: ScheduleTables, counter synchronization, OS-Application ownership.
 *
 * Model: a counter counts 0..maxAllowedValue and wraps.  An active alarm stores the counter value at which
 * it expires; after every counter increment the alarms of that counter are scanned (<= 4 alarms: a linear scan
 * is the most readable thing).  A cyclic alarm re-arms itself with expiry += cycle (modulo the counter range).
 */
#include "Os_Internal.h"

typedef struct {
    boolean  active;
    TickType expiry;            /* counter value at which the alarm fires */
    TickType cycle;             /* 0 = single shot */
} Os_AlarmRtType;

void Os_Counter_Advance(CounterType c);        /* one counter tick + alarm scan (defined below) */

static TickType        s_counter[OS_NUM_COUNTERS + 1u];
static Os_AlarmRtType  s_alarm[OS_NUM_ALARMS + 1u];

/* (a + b) mod (max + 1) without 32-bit overflow */
static TickType ctr_add(CounterType c, TickType a, TickType b)
{
    uint64 range = (uint64)Os_Config.counters[c].maxAllowedValue + 1u;

    return (TickType)(((uint64)a + (uint64)b) % range);
}

/* ticks from `from` until `to` going forward (modulo the counter range) */
static TickType ctr_diff(CounterType c, TickType to, TickType from)
{
    uint64 range = (uint64)Os_Config.counters[c].maxAllowedValue + 1u;

    return (TickType)(((uint64)to + range - (uint64)from) % range);
}

/* -------------------------------------------------------------------------- alarm expiry */

/* Execute the action of alarm a (kernel lock held).  Errors go to ErrorHook with the service id of the
 * underlying service (OSEK chapter 9: an error inside an alarm action is reported like an error of the service itself). */
static void alarm_fire(uint8 a)
{
    const Os_AlarmCfgType *cfg = &Os_Config.alarms[a];
    StatusType st = E_OK;
    OSServiceIdType svc = OSServiceId_ActivateTask;

    if (MINI_TRACE_OS_SWITCH == STD_ON) {
        TRACE(TRACE_CAT_OS, "ALARM %s", cfg->name);
    }
    switch (cfg->action) {
    case OS_ALARM_ACTIVATETASK:
        st = Os_ActivateInternal(cfg->task);
        break;
    case OS_ALARM_SETEVENT:
        svc = OSServiceId_SetEvent;
        st = Os_SetEventInternal(cfg->task, cfg->event);
        break;
    case OS_ALARM_CALLBACK:
        if (cfg->callback != NULL_PTR) {
            cfg->callback();                       /* runs in tick (ISR) context, with interrupts locked here */
        }
        break;
    case OS_ALARM_INCREMENTCOUNTER:
    default:
        svc = OSServiceId_IncrementCounter;
        if (cfg->incCounter < (CounterType)Os_Config.numCounters) {
            Os_Counter_Advance(cfg->incCounter);
        } else {
            st = E_OS_ID;
        }
        break;
    }
    if (st != E_OK) {
        Os_ReportError(svc, st);
    }
}

/* One tick of counter c: increment (wrap), then fire every alarm whose expiry equals the new value. */
void Os_Counter_Advance(CounterType c)
{
    TickType value;
    uint8 a;

    value = (s_counter[c] >= Os_Config.counters[c].maxAllowedValue) ? 0u : (TickType)(s_counter[c] + 1u);
    s_counter[c] = value;
    for (a = 0u; a < Os_Config.numAlarms; a++) {
        if ((Os_Config.alarms[a].counter == c) && (s_alarm[a].active != FALSE) && (s_alarm[a].expiry == value)) {
            if (s_alarm[a].cycle != 0u) {
                s_alarm[a].expiry = ctr_add(c, value, s_alarm[a].cycle);     /* cyclic: re-arm before running the action */
            } else {
                s_alarm[a].active = FALSE;
            }
            alarm_fire(a);
        }
    }
}

/* Called by the port from the tick interrupt (target: SysTick via Mini_Time_TickHook; host: virtual 1 ms).
 * Increments the system counter, runs alarm actions, then lets the scheduler react (task activated by an
 * alarm may preempt the interrupted task when the tick interrupt returns). */
void Os_Kernel_TickHandler(void)
{
    uint32 lk;

    if (Os_Started == FALSE) {
        return;
    }
    lk = Os_Port_DisableAll();
    Os_Counter_Advance(Os_Config.systemCounter);
    Os_Port_RestoreAll(lk);
    Os_Reschedule();
}

/* -------------------------------------------------------------------------- StartOS support */

/* Arm alarm a from the given start/cycle (internal: no argument checks beyond clamping; config is trusted). */
static void alarm_arm(uint8 a, TickType start, TickType cycle, boolean absolute)
{
    CounterType c = Os_Config.alarms[a].counter;

    s_alarm[a].expiry = (absolute != FALSE) ? start : ctr_add(c, s_counter[c], start);
    s_alarm[a].cycle = cycle;
    s_alarm[a].active = TRUE;
}

void Os_Alarm_Init(AppModeType Mode)
{
    uint8 i;

    for (i = 0u; i < Os_Config.numCounters; i++) {
        s_counter[i] = 0u;
    }
    for (i = 0u; i < Os_Config.numAlarms; i++) {
        const Os_AlarmCfgType *cfg = &Os_Config.alarms[i];

        s_alarm[i].active = FALSE;
        s_alarm[i].cycle = 0u;
        s_alarm[i].expiry = 0u;
        if ((cfg->autostart != FALSE) && ((cfg->autostartModes & (1uL << Mode)) != 0u)) {
            alarm_arm(i, cfg->autostartTime, cfg->autostartCycle, cfg->autostartAbsolute);
        }
    }
}

/* -------------------------------------------------------------------------- counter services */

StatusType GetCounterValue(CounterType CounterID, TickRefType Value)
{
    StatusType st = E_OK;

    if (CounterID >= (CounterType)Os_Config.numCounters) {
        st = E_OS_ID;
    } else if (Value == NULL_PTR) {
        st = E_OS_VALUE;
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_GetCounterValue, st);
        return st;
    }
    *Value = s_counter[CounterID];
    return E_OK;
}

/* elapsed ticks since *Value (the value returned by an earlier call); *Value is updated to "now". */
StatusType GetElapsedValue(CounterType CounterID, TickRefType Value, TickRefType ElapsedValue)
{
    StatusType st = E_OK;

    if (CounterID >= (CounterType)Os_Config.numCounters) {
        st = E_OS_ID;
    } else if ((Value == NULL_PTR) || (ElapsedValue == NULL_PTR) ||
               (*Value > Os_Config.counters[CounterID].maxAllowedValue)) {
        st = E_OS_VALUE;
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_GetElapsedValue, st);
        return st;
    }
    *ElapsedValue = ctr_diff(CounterID, s_counter[CounterID], *Value);
    *Value = s_counter[CounterID];
    return E_OK;
}

/* Software counters only (SWS_Os_00399: hardware counter -> E_OS_ID).  Alarm action errors are reported
 * through the ErrorHook, the service itself still returns E_OK (SWS text "IncrementCounter shall call ErrorHook"). */
StatusType IncrementCounter(CounterType CounterID)
{
    uint32 lk;

    if ((CounterID >= (CounterType)Os_Config.numCounters) || (Os_Config.counters[CounterID].hardware != FALSE)) {
        Os_ReportError(OSServiceId_IncrementCounter, E_OS_ID);
        return E_OS_ID;
    }
    lk = Os_Port_DisableAll();
    Os_Counter_Advance(CounterID);
    Os_Port_RestoreAll(lk);
    Os_Reschedule();
    return E_OK;
}

/* -------------------------------------------------------------------------- alarm services */

StatusType GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType Info)
{
    StatusType st = E_OK;

    if (AlarmID >= (AlarmType)Os_Config.numAlarms) {
        st = E_OS_ID;
    } else if (Info == NULL_PTR) {
        st = E_OS_VALUE;
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_GetAlarmBase, st);
        return st;
    }
    {
        const Os_CounterCfgType *c = &Os_Config.counters[Os_Config.alarms[AlarmID].counter];

        Info->maxallowedvalue = c->maxAllowedValue;
        Info->ticksperbase = c->ticksPerBase;
        Info->mincycle = c->minCycle;
    }
    return E_OK;
}

/* ticks left until the alarm fires; E_OS_NOFUNC if it is not running */
StatusType GetAlarm(AlarmType AlarmID, TickRefType Tick)
{
    StatusType st = E_OK;
    uint32 lk;

    if (AlarmID >= (AlarmType)Os_Config.numAlarms) {
        st = E_OS_ID;
    } else if (Tick == NULL_PTR) {
        st = E_OS_VALUE;
    }
    if (st == E_OK) {
        lk = Os_Port_DisableAll();
        if (s_alarm[AlarmID].active == FALSE) {
            st = E_OS_NOFUNC;
        } else {
            CounterType c = Os_Config.alarms[AlarmID].counter;
            *Tick = ctr_diff(c, s_alarm[AlarmID].expiry, s_counter[c]);
        }
        Os_Port_RestoreAll(lk);
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_GetAlarm, st);
    }
    return st;
}

/* shared argument checks of SetRel/SetAbsAlarm; `first` = increment (rel, must be > 0) or start (abs) */
static StatusType alarm_check_set(AlarmType a, TickType first, TickType cycle, boolean absolute)
{
    const Os_CounterCfgType *c;

    if (a >= (AlarmType)Os_Config.numAlarms) {
        return E_OS_ID;
    }
    c = &Os_Config.counters[Os_Config.alarms[a].counter];
    if ((first > c->maxAllowedValue) || ((absolute == FALSE) && (first == 0u)) ||       /* SWS_Os_00304: increment 0 */
        (cycle > c->maxAllowedValue) || ((cycle != 0u) && (cycle < c->minCycle))) {
        return E_OS_VALUE;
    }
    return E_OK;
}

static StatusType alarm_set(AlarmType a, TickType first, TickType cycle, boolean absolute, OSServiceIdType svc)
{
    StatusType st = alarm_check_set(a, first, cycle, absolute);

    if (st == E_OK) {
        uint32 lk = Os_Port_DisableAll();

        if (s_alarm[a].active != FALSE) {
            st = E_OS_STATE;                       /* alarm is already running */
        } else {
            alarm_arm(a, first, cycle, absolute);
        }
        Os_Port_RestoreAll(lk);
    }
    if (st != E_OK) {
        Os_ReportError(svc, st);
    }
    return st;
}

StatusType SetRelAlarm(AlarmType AlarmID, TickType increment, TickType cycle)
{
    return alarm_set(AlarmID, increment, cycle, FALSE, OSServiceId_SetRelAlarm);
}

StatusType SetAbsAlarm(AlarmType AlarmID, TickType start, TickType cycle)
{
    return alarm_set(AlarmID, start, cycle, TRUE, OSServiceId_SetAbsAlarm);
}

StatusType CancelAlarm(AlarmType AlarmID)
{
    StatusType st = E_OK;

    if (AlarmID >= (AlarmType)Os_Config.numAlarms) {
        st = E_OS_ID;
    } else {
        uint32 lk = Os_Port_DisableAll();

        if (s_alarm[AlarmID].active == FALSE) {
            st = E_OS_NOFUNC;
        } else {
            s_alarm[AlarmID].active = FALSE;
        }
        Os_Port_RestoreAll(lk);
    }
    if (st != E_OK) {
        Os_ReportError(OSServiceId_CancelAlarm, st);
    }
    return st;
}
