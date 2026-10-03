/*
 * Det.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Default Error Tracer (AUTOSAR_CP_SWS_DefaultErrorTracer).
 * Implemented: Det_Init (SWS_Det_00008), Det_Start, Det_ReportError, Det_ReportRuntimeError,
 * Det_ReportTransientFault. R25-11 change vs. older releases: Det_ReportError returns Std_ReturnType
 * (always E_OK here; the real one returns the value of the optional error-hook chain), so callers can
 * write "return Det_ReportError(...)" style code.
 * Behaviour: every report (1) is stored in a 16 entry ring buffer, (2) increments the counters and
 * (3) prints ONE trace line (DESIGN 12):  DET ERROR|RUNTIME|TRANSIENT mod=%u inst=%u api=%u err=%u.
 * Not implemented: error/runtime/fault hook lists, DLT, "halt on error" (a real Det often loops forever
 * in Det_ReportError so that a debugger stops there; the check script just greps for "DET ERROR").
 * Context: callable from any task/ISR/before StartOS; the ring buffer is guarded by the Det exclusive area
 * (SuspendAllInterrupts, DESIGN 6.4: usable before StartOS).
 * Spec: AUTOSAR_CP_SWS_DefaultErrorTracer 8.1.3.1 Det_Init, 8.1.3.2 Det_ReportError, 8.1.3.3 Det_Start
 */
#include "Det.h"
#include "SchM.h"
#include "Trace.h"

#define DET_RING_SIZE  16u

static Det_EntryType det_ring[DET_RING_SIZE];
static uint16        det_total;       /* all reports since Det_Init */
static boolean       det_started;

void Det_Init(const Det_ConfigType *ConfigPtr)
{
    uint8 i;
    (void)ConfigPtr;                  /* no configurable behaviour in this subset */
    SchM_Enter_Det_DET_EXCLUSIVE_AREA_0();
    for (i = 0u; i < DET_RING_SIZE; i++) {
        det_ring[i].moduleId = 0u; det_ring[i].instanceId = 0u;
        det_ring[i].apiId = 0u;    det_ring[i].errorId = 0u; det_ring[i].kind = 0u;
    }
    det_total = 0u;
    det_started = FALSE;
    SchM_Exit_Det_DET_EXCLUSIVE_AREA_0();
}

void Det_Start(void)
{
    det_started = TRUE;               /* real Det: reports are only forwarded to hooks after Det_Start */
}

/* one common path for the three report kinds */
static Std_ReturnType det_report(uint8 kind, const char *label,
                                 uint16 mod, uint8 inst, uint8 api, uint8 err)
{
    SchM_Enter_Det_DET_EXCLUSIVE_AREA_0();
    det_ring[det_total % DET_RING_SIZE].moduleId   = mod;
    det_ring[det_total % DET_RING_SIZE].instanceId = inst;
    det_ring[det_total % DET_RING_SIZE].apiId      = api;
    det_ring[det_total % DET_RING_SIZE].errorId    = err;
    det_ring[det_total % DET_RING_SIZE].kind       = kind;
    if (det_total < 0xFFFFu) { det_total++; }
    SchM_Exit_Det_DET_EXCLUSIVE_AREA_0();
    /* trace outside the lock (Trace takes its own short lock) */
    TRACE(TRACE_CAT_DET, "%s mod=%u inst=%u api=%u err=%u", label,
          (unsigned)mod, (unsigned)inst, (unsigned)api, (unsigned)err);
    return E_OK;
}

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    return det_report(0u, "ERROR", ModuleId, InstanceId, ApiId, ErrorId);
}

Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    return det_report(1u, "RUNTIME", ModuleId, InstanceId, ApiId, ErrorId);
}

Std_ReturnType Det_ReportTransientFault(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 FaultId)
{
    return det_report(2u, "TRANSIENT", ModuleId, InstanceId, ApiId, FaultId);
}

void Det_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo != NULL_PTR) {
        versioninfo->vendorID = MINI_VENDOR_ID;
        versioninfo->moduleID = MINI_MODULE_DET;
        versioninfo->sw_major_version = 1u;
        versioninfo->sw_minor_version = 0u;
        versioninfo->sw_patch_version = 0u;
    }
}

uint16 Det_GetErrorCount(void)
{
    return det_total;
}

boolean Det_GetEntry(uint16 index, Det_EntryType *entry)
{
    uint16 n = (det_total < DET_RING_SIZE) ? det_total : (uint16)DET_RING_SIZE;
    uint16 first;
    if ((entry == NULL_PTR) || (index >= n)) { return FALSE; }
    first = (det_total < DET_RING_SIZE) ? 0u : (uint16)(det_total % DET_RING_SIZE);   /* oldest slot */
    *entry = det_ring[(first + index) % DET_RING_SIZE];
    return TRUE;
}
