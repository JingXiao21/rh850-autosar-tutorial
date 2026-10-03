/*
 * Os_Hooks.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the user supplied OS hook routines (AUTOSAR_CP_SWS_OS 7.x "Hook routines":
 * StartupHook, ShutdownHook, ErrorHook, PreTaskHook, PostTaskHook). The kernel calls them if the matching
 * OS_USE_*HOOK switch of the generated Os_Cfg.h is on. Implemented (DESIGN 6.7):
 *   ErrorHook    : trace "OS ERROR svc=%u err=%u" and forward to Det as a RUNTIME error (OS module id).
 *   ShutdownHook : trace "OS SHUTDOWN err=%u"; HOST ONLY additionally prints "SIM SCENARIO END" and flushes the
 *                  virtual CAN files (the host port exits the process right after this hook).
 *   StartupHook  : empty. R25-11 EcuM SWS 7.3 (Figure 7.3) shows StartupHook before the first task; the work of
 *                  the post-OS startup is done in Task_Init -> EcuM_StartupTwo, not here.
 *   Pre/PostTaskHook : empty (kernel traces TASK_START/TASK_END itself).
 * All hook bodies are defined unconditionally: the definitions are harmless when a switch is off.
 */
#include "Os.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_ModuleIds.h"
#if defined(MINI_PLATFORM_HOST)
#include "SimCan.h"
#endif

void StartupHook(void)
{
}

void ErrorHook(StatusType Error)
{
    OSServiceIdType svc = OSErrorGetServiceId();
    TRACE(TRACE_CAT_OS, "ERROR svc=%u err=%u", (unsigned)svc, (unsigned)Error);
    (void)Det_ReportRuntimeError(MINI_MODULE_OS, 0u, (uint8)svc, (uint8)Error);
}

void ShutdownHook(StatusType Error)
{
    TRACE(TRACE_CAT_OS, "SHUTDOWN err=%u", (unsigned)Error);
#if defined(MINI_PLATFORM_HOST)
    TRACE(TRACE_CAT_SIM, "SCENARIO END");
    SimCan_Deinit();                                 /* flush + close the tx log: it is the next ECU's rx script */
#endif
}

void PreTaskHook(void)
{
}

void PostTaskHook(void)
{
}
