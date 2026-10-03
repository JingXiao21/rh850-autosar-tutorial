/*
 * Os_PortFiber_Host.c  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (the "context" primitive of a port; on the Cortex-M33 this is the PendSV
 * assembly in os/port/cm33/Os_Port_Cm33.c).  Thin wrapper around the Windows Fiber API.
 * It lives in its own translation unit because <windows.h> clashes with AUTOSAR names (SetEvent, CONST,
 * ...), so it can never be included together with Os.h.  Interface: HostFiber.h.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "HostFiber.h"

static void (*s_entry)(unsigned arg);

/* Windows fiber start routine -> project style entry function.  A fiber function must never return. */
static VOID CALLBACK fiber_trampoline(PVOID param)
{
    s_entry((unsigned)(UINT_PTR)param);
    for (;;) { }
}

void *HostFiber_ConvertThread(void)
{
    return ConvertThreadToFiber(NULL);
}

void *HostFiber_Create(unsigned stackBytes, void (*entry)(unsigned arg), unsigned arg)
{
    s_entry = entry;                               /* one entry function for all task fibers */
    return CreateFiber(stackBytes, fiber_trampoline, (LPVOID)(UINT_PTR)arg);
}

void HostFiber_Switch(void *fiber)
{
    SwitchToFiber(fiber);
}

void HostFiber_Delete(void *fiber)
{
    DeleteFiber(fiber);
}
