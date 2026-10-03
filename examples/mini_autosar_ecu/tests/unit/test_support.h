/*
 * test_support.h  (host unit tests of agent C: Com, PduR, EcuM, BswM, RestBus)
 *
 * [Educational Implementation]
 * Tiny self-contained test harness: CHECK macros, a Trace_Log mock that records the formatted lines (so tests can
 * assert on the DESIGN 12 trace catalogue), mocks of the OS interrupt-lock services used through SchM.h, and an
 * optional Det mock (define TEST_MOCK_DET before including this header; otherwise link bsw/det/Det.c).
 * Each test is ONE executable, so this header may define (non-static) mock functions and globals.
 */
#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "Std_Types.h"
#include "Trace.h"
#include "Os.h"

static int t_failures;
static int t_checks;

#define CHECK(cond) do { t_checks++; if (!(cond)) { t_failures++; \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a); long _b = (long)(b); t_checks++; if (_a != _b) { t_failures++; \
    (void)printf("FAIL %s:%d: %s == %s  (%ld vs %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

#define TEST_DONE() do { (void)printf("%s: %d checks, %d failures\n", __FILE__, t_checks, t_failures); \
    return (t_failures == 0) ? 0 : 1; } while (0)

/* ---------------- trace mock: whole output kept in one buffer ---------------- */
char t_traceBuf[16384];
int  t_traceLen;

void Trace_Log(Trace_CategoryType cat, const char *fmt, ...)
{
    char line[200];
    va_list ap;
    int n;
    static const char *const names[] = { "OS", "RTE", "COM", "PDUR", "CANIF", "CAN", "SWC", "BSW", "DET", "SIM" };
    va_start(ap, fmt);
    n = vsnprintf(line, sizeof line, fmt, ap);          /* %B is not supported by vsnprintf: tests avoid it */
    va_end(ap);
    if (n < 0) { return; }
    n = snprintf(t_traceBuf + t_traceLen, sizeof t_traceBuf - (size_t)t_traceLen, "%s %s\n",
                 names[(int)cat], line);
    if (n > 0 && (size_t)(t_traceLen + n) < sizeof t_traceBuf) { t_traceLen += n; }
}
uint32 Trace_GetTimeUs(void) { return 0u; }
void   Trace_Init(const char *logFile) { (void)logFile; }
void   Trace_PutChar(char c) { (void)c; }

/* number of occurrences of `needle` in the trace */
static inline int t_traceCount(const char *needle)
{
    int c = 0;
    const char *p = t_traceBuf;
    while ((p = strstr(p, needle)) != NULL) { c++; p++; }
    return c;
}
/* byte offset of the first occurrence, -1 if absent (order checks) */
static inline int t_traceFind(const char *needle)
{
    const char *p = strstr(t_traceBuf, needle);
    return (p == NULL) ? -1 : (int)(p - t_traceBuf);
}
static inline void t_traceClear(void) { t_traceBuf[0] = '\0'; t_traceLen = 0; }

/* ---------------- OS mocks (SchM.h exclusive areas map onto these) ---------------- */
int t_osLockDepth;
void SuspendOSInterrupts(void)  { t_osLockDepth++; }
void ResumeOSInterrupts(void)   { t_osLockDepth--; }
void SuspendAllInterrupts(void) { t_osLockDepth++; }
void ResumeAllInterrupts(void)  { t_osLockDepth--; }

/* ---------------- optional Det mock ---------------- */
#ifdef TEST_MOCK_DET
#include "Det.h"
int t_detErrors;
int t_detRuntime;
Std_ReturnType Det_ReportError(uint16 m, uint8 i, uint8 a, uint8 e)
{ (void)m; (void)i; (void)a; (void)e; t_detErrors++; return E_OK; }
Std_ReturnType Det_ReportRuntimeError(uint16 m, uint8 i, uint8 a, uint8 e)
{ (void)m; (void)i; (void)a; (void)e; t_detRuntime++; return E_OK; }
Std_ReturnType Det_ReportTransientFault(uint16 m, uint8 i, uint8 a, uint8 e)
{ (void)m; (void)i; (void)a; (void)e; return E_OK; }
#endif

#endif /* TEST_SUPPORT_H */
