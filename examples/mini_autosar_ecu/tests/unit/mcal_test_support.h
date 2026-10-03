/*
 * mcal_test_support.h  (tests/unit)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (unit-test scaffolding). Shared by test_mcal_*.c / test_canif_*.c / test_iohwab*.c; include it ONCE per
 * test program. It replaces the neighbours of the module under test with small recording stubs, so the tests do not depend on the
 * other agents' implementations:
 *   - Trace_Log  : formats like include/Trace.h (%u %x %03x %s %c %B) into a ring of text lines, searchable with ts_trace_has()
 *   - Det_*      : counts and remembers the last report (ts_det_count, ts_det_last_*)
 *   - OS         : SuspendOSInterrupts / ResumeOSInterrupts (SchM exclusive areas) with nesting check
 *   - SimTime    : SimTime_GetUs (variable ts_now_us) and SimTime_RaiseIsr (ts_irq_raised[irq] counter)
 * T_CHECK / T_DONE implement the pass/fail bookkeeping (exit code 0 = pass).
 */
#ifndef MCAL_TEST_SUPPORT_H
#define MCAL_TEST_SUPPORT_H

/* helpers/variables not used by every test program are fine */
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Std_Types.h"
#include "Trace.h"
#include "Det.h"
#include "SimTime.h"

/* ------------------------------------------------------------------------------------------------ check bookkeeping */
static int ts_failures;
static int ts_checks;
#define T_CHECK(cond) do { ts_checks++; if (!(cond)) { ts_failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define T_DONE() do { printf("%s: %d checks, %d failures\n", __FILE__, ts_checks, ts_failures); return (ts_failures == 0) ? 0 : 1; } while (0)

/* ------------------------------------------------------------------------------------------------ Trace */
#define TS_TRACE_LINES 128
#define TS_TRACE_LEN   128
static char ts_trace[TS_TRACE_LINES][TS_TRACE_LEN];
static int  ts_trace_count;

void Trace_Log(Trace_CategoryType cat, const char *fmt, ...)
{
    char line[TS_TRACE_LEN];
    size_t n = 0u;
    va_list ap;
    (void)cat;
    va_start(ap, fmt);
    while ((*fmt != '\0') && (n < sizeof(line) - 16u)) {
        if (*fmt != '%') { line[n++] = *fmt++; continue; }
        {
            char spec[16];
            size_t k = 0u;
            char tmp[32];
            spec[k++] = *fmt++;
            while ((*fmt != '\0') && (k < sizeof(spec) - 2u) && (strchr("0123456789", *fmt) != NULL)) { spec[k++] = *fmt++; }
            if (*fmt == 'B') {
                const uint8 *p = va_arg(ap, const uint8 *);
                unsigned len = va_arg(ap, unsigned), i;
                for (i = 0u; (i < len) && (n < sizeof(line) - 4u); i++) { n += (size_t)sprintf(&line[n], "%s%02x", (i != 0u) ? " " : "", (unsigned)p[i]); }
                fmt++;
                continue;
            }
            spec[k++] = *fmt;
            spec[k] = '\0';
            if ((*fmt == 'u') || (*fmt == 'x') || (*fmt == 'X')) { (void)snprintf(tmp, sizeof(tmp), spec, va_arg(ap, unsigned)); }
            else if ((*fmt == 'd') || (*fmt == 'c')) { (void)snprintf(tmp, sizeof(tmp), spec, va_arg(ap, int)); }
            else if (*fmt == 's') { (void)snprintf(tmp, sizeof(tmp), spec, va_arg(ap, const char *)); }
            else { (void)snprintf(tmp, sizeof(tmp), "%%"); }
            if (*fmt != '\0') { fmt++; }
            n += (size_t)snprintf(&line[n], sizeof(line) - n, "%s", tmp);
        }
    }
    line[n] = '\0';
    va_end(ap);
    if (ts_trace_count < TS_TRACE_LINES) {
        (void)strcpy(ts_trace[ts_trace_count], line);
    }
    ts_trace_count++;
}
uint32 Trace_GetTimeUs(void) { return 0u; }
void   Trace_Init(const char *logFile) { (void)logFile; }
void   Trace_PutChar(char c) { (void)c; }

static int ts_trace_has(const char *needle)
{
    int i;
    for (i = 0; (i < ts_trace_count) && (i < TS_TRACE_LINES); i++) {
        if (strstr(ts_trace[i], needle) != NULL) { return 1; }
    }
    return 0;
}
static void ts_trace_clear(void) { ts_trace_count = 0; }

/* ------------------------------------------------------------------------------------------------ Det */
const Det_ConfigType Det_Config = { 0u };
static int    ts_det_count;
static int    ts_det_runtime_count;
static uint16 ts_det_last_module;
static uint8  ts_det_last_api;
static uint8  ts_det_last_error;
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)InstanceId;
    ts_det_count++; ts_det_last_module = ModuleId; ts_det_last_api = ApiId; ts_det_last_error = ErrorId;
    return E_OK;
}
Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)InstanceId;
    ts_det_runtime_count++; ts_det_last_module = ModuleId; ts_det_last_api = ApiId; ts_det_last_error = ErrorId;
    return E_OK;
}
Std_ReturnType Det_ReportTransientFault(uint16 m, uint8 i, uint8 a, uint8 e) { (void)m; (void)i; (void)a; (void)e; return E_OK; }
void   Det_Init(const Det_ConfigType *c) { (void)c; }
void   Det_Start(void) { }
void   Det_GetVersionInfo(Std_VersionInfoType *v) { (void)v; }
uint16 Det_GetErrorCount(void) { return (uint16)(ts_det_count + ts_det_runtime_count); }
boolean Det_GetEntry(uint16 index, Det_EntryType *entry) { (void)index; (void)entry; return FALSE; }

/* ------------------------------------------------------------------------------------------------ OS (SchM exclusive areas) */
static int ts_os_nesting;
static int ts_os_max_nesting;
static int ts_os_enters;
void SuspendOSInterrupts(void)  { ts_os_nesting++; ts_os_enters++; if (ts_os_nesting > ts_os_max_nesting) { ts_os_max_nesting = ts_os_nesting; } }
void ResumeOSInterrupts(void)   { ts_os_nesting--; }
void SuspendAllInterrupts(void) { SuspendOSInterrupts(); }
void ResumeAllInterrupts(void)  { ResumeOSInterrupts(); }

/* ------------------------------------------------------------------------------------------------ SimTime */
static uint64 ts_now_us;
static int    ts_irq_raised[256];
uint64 SimTime_GetUs(void) { return ts_now_us; }
void   SimTime_RaiseIsr(uint16 irqNumber) { if (irqNumber < 256u) { ts_irq_raised[irqNumber]++; } }

/* ------------------------------------------------------------------------------------------------ helpers */
/* path of a scratch file in the temp directory (unit tests run with the repo root as cwd: keep it clean) */
static const char *ts_tmpfile(const char *name)
{
    static char path[512];
    const char *dir = getenv("TEMP");
    if (dir == NULL) { dir = getenv("TMPDIR"); }
    if (dir == NULL) { dir = "."; }
    (void)snprintf(path, sizeof(path), "%s/%s", dir, name);
    return path;
}
static char *ts_read_file(const char *path, char *buf, size_t size)
{
    FILE *f = fopen(path, "r");
    size_t n = 0u;
    buf[0] = '\0';
    if (f != NULL) { n = fread(buf, 1u, size - 1u, f); fclose(f); }
    buf[n] = '\0';
    return buf;
}

#endif /* MCAL_TEST_SUPPORT_H */
