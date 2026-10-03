/*
 * rte_mocks.c - see rte_mocks.h.  [Educational Implementation]
 */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rte_mocks.h"
#include "Os.h"
#include "Com.h"
#include "IoHwAb.h"
#include "Trace.h"

#define MAX_CALLS   512u
#define MAX_TRACES  64u
#define MAX_SIGNALS 16u

static MockCall calls[MAX_CALLS];
static uint32   ncalls;

static struct { uint32 value; uint8 size; uint32 sent; } sig[MAX_SIGNALS];
static boolean  com_stopped;

static uint32   ev_queue[16];
static uint32   ev_n, ev_pos, ev_current;
static jmp_buf  task_exit;
static int      in_task;
static int      irq_depth, res_depth;

static char     traces[MAX_TRACES][96];
static uint32   ntraces;

uint16 mock_wheel_raw;
void (*mock_iohwab_hook)(void);

static void rec(const char *fn, uint32 a, uint32 b)
{
    if (ncalls < MAX_CALLS) {
        calls[ncalls].fn = fn;
        calls[ncalls].a = a;
        calls[ncalls].b = b;
        ncalls++;
    }
}

void mock_reset(void)
{
    memset(calls, 0, sizeof calls);
    memset(sig, 0, sizeof sig);
    ncalls = 0u; com_stopped = FALSE; ev_n = ev_pos = ev_current = 0u; irq_depth = res_depth = 0;
    ntraces = 0u; mock_wheel_raw = 0u; mock_iohwab_hook = NULL_PTR; in_task = 0;
}

uint32 mock_ncalls(void) { return ncalls; }
const MockCall *mock_call(uint32 i) { return &calls[i]; }

uint32 mock_count(const char *fn)
{
    uint32 i, n = 0u;
    for (i = 0u; i < ncalls; i++) { if (strcmp(calls[i].fn, fn) == 0) { n++; } }
    return n;
}

int mock_index_of(const char *fn, uint32 from)
{
    uint32 i;
    for (i = from; i < ncalls; i++) { if (strcmp(calls[i].fn, fn) == 0) { return (int)i; } }
    return -1;
}

int mock_index_of_args(const char *fn, uint32 a, uint32 from)
{
    uint32 i;
    for (i = from; i < ncalls; i++) { if ((strcmp(calls[i].fn, fn) == 0) && (calls[i].a == a)) { return (int)i; } }
    return -1;
}

void mock_com_set(uint16 s, uint8 size, uint32 value) { sig[s].value = value; sig[s].size = size; }
uint32 mock_com_sent(uint16 s) { return sig[s].sent; }
void mock_com_set_stopped(boolean stopped) { com_stopped = stopped; }

void mock_set_events(const uint32 *evs, uint32 n)
{
    uint32 i;
    for (i = 0u; (i < n) && (i < 16u); i++) { ev_queue[i] = evs[i]; }
    ev_n = n; ev_pos = 0u;
}

int mock_run_task(void (*task)(void))
{
    int left = 0;
    in_task = 1;
    if (setjmp(task_exit) == 0) {
        task();
    } else {
        left = 1;
    }
    in_task = 0;
    return left;
}

int mock_irq_depth(void) { return irq_depth; }
int mock_res_depth(void) { return res_depth; }

int mock_trace_has(const char *text)
{
    uint32 i;
    for (i = 0u; i < ntraces; i++) { if (strstr(traces[i], text) != NULL) { return 1; } }
    return 0;
}

/* ---------------------------------------------------------------- Trace */
void Trace_Log(Trace_CategoryType cat, const char *fmt, ...)
{
    va_list ap;
    (void)cat;
    if (ntraces < MAX_TRACES) {
        va_start(ap, fmt);
        (void)vsnprintf(traces[ntraces], sizeof traces[0], fmt, ap);
        va_end(ap);
        ntraces++;
    }
}

/* ---------------------------------------------------------------- OS */
StatusType ActivateTask(TaskType t) { rec("ActivateTask", t, 0u); return E_OK; }
StatusType SetEvent(TaskType t, EventMaskType m) { rec("SetEvent", t, m); return E_OK; }
StatusType TerminateTask(void) { rec("TerminateTask", 0u, 0u); return E_OK; }
StatusType GetResource(ResourceType r) { rec("GetResource", r, 0u); res_depth++; return E_OK; }
StatusType ReleaseResource(ResourceType r) { rec("ReleaseResource", r, 0u); res_depth--; return E_OK; }
void SuspendOSInterrupts(void) { irq_depth++; }
void ResumeOSInterrupts(void) { irq_depth--; }
void SuspendAllInterrupts(void) { irq_depth++; }
void ResumeAllInterrupts(void) { irq_depth--; }

StatusType WaitEvent(EventMaskType mask)
{
    rec("WaitEvent", mask, 0u);
    if (ev_pos >= ev_n) {
        if (in_task) { longjmp(task_exit, 1); }
        return E_OK;
    }
    ev_current = ev_queue[ev_pos++];
    return E_OK;
}

StatusType GetEvent(TaskType t, EventMaskRefType ev) { rec("GetEvent", t, 0u); *ev = ev_current; return E_OK; }
StatusType ClearEvent(EventMaskType m) { rec("ClearEvent", m, 0u); return E_OK; }

/* ---------------------------------------------------------------- Com */
uint8 Com_SendSignal(Com_SignalIdType id, const void *p)
{
    uint32 v = 0u;
    rec("Com_SendSignal", id, 0u);
    if (com_stopped) { return COM_SERVICE_NOT_AVAILABLE; }
    memcpy(&v, p, (sig[id].size != 0u) ? sig[id].size : 1u);   /* little endian host */
    sig[id].sent = v;
    calls[ncalls - 1u].b = v;
    return E_OK;
}

uint8 Com_ReceiveSignal(Com_SignalIdType id, void *p)
{
    rec("Com_ReceiveSignal", id, sig[id].value);
    if (com_stopped) { return COM_SERVICE_NOT_AVAILABLE; }
    memcpy(p, &sig[id].value, (sig[id].size != 0u) ? sig[id].size : 1u);
    return E_OK;
}

/* ---------------------------------------------------------------- IoHwAb */
void IoHwAb_Init(void) { rec("IoHwAb_Init", 0u, 0u); }
Std_ReturnType IoHwAb_GetWheelSpeed(uint16 *raw)
{
    rec("IoHwAb_GetWheelSpeed", 0u, 0u);
    if (mock_iohwab_hook != NULL_PTR) { mock_iohwab_hook(); }
    *raw = mock_wheel_raw;
    return E_OK;
}
Std_ReturnType IoHwAb_SetHeadlight(uint8 s) { rec("IoHwAb_SetHeadlight", s, 0u); return E_OK; }
Std_ReturnType IoHwAb_GetHeadlight(uint8 *s) { *s = 0u; return E_OK; }

/* ---------------------------------------------------------------- BSW main functions / init called by generated task bodies */
void EcuM_StartupTwo(void) { rec("EcuM_StartupTwo", 0u, 0u); }
void EcuM_MainFunction(void) { rec("EcuM_MainFunction", 0u, 0u); }
void Can_MainFunction_Write(void) { rec("Can_MainFunction_Write", 0u, 0u); }
void Can_MainFunction_Read(void) { rec("Can_MainFunction_Read", 0u, 0u); }
void Can_Isr_Rx(void) { rec("Can_Isr_Rx", 0u, 0u); }
void Com_MainFunctionRx(void) { rec("Com_MainFunctionRx", 0u, 0u); }
void Com_MainFunctionTx(void) { rec("Com_MainFunctionTx", 0u, 0u); }
void BswM_MainFunction(void) { rec("BswM_MainFunction", 0u, 0u); }
