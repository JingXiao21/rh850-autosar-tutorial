/*
 * Trace_Host.c  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (debug console backend, see include/Trace.h).
 * Host sink of the trace: stdout plus an optional log file.  Trace_Init(path) opens the file once;
 * Trace_Init(NULL_PTR) keeps an already open file (HostMain opens it first, EcuM_AL_DriverInitZero calls
 * Trace_Init(NULL_PTR) later).  Output is flushed by the C runtime at exit() (Os_Port_Halt).
 */
#include <stdio.h>
#include "Trace.h"
#include "../../src/Trace_Port.h"

static FILE *s_log;

void Trace_Init(const char *logFile)
{
    if ((logFile != NULL_PTR) && (s_log == NULL_PTR)) {
        s_log = fopen(logFile, "wb");              /* "wb": identical bytes on Windows and Linux ("\n" only) */
    }
}

void Trace_PutChar(char c)
{
    (void)fputc((int)c, stdout);
    if (s_log != NULL_PTR) {
        (void)fputc((int)c, s_log);
    }
}

/* single threaded simulator: nothing to lock */
uint32 Trace_PortLock(void)
{
    return 0u;
}

void Trace_PortUnlock(uint32 prev)
{
    (void)prev;
}

/* host extra (not in Trace.h): used by Os_Port_Halt so the log file is complete when the process exits */
void Trace_HostFlush(void)
{
    (void)fflush(stdout);
    if (s_log != NULL_PTR) {
        (void)fflush(s_log);
    }
}
