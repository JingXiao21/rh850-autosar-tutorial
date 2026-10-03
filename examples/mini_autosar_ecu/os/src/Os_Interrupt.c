/*
 * Os_Interrupt.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Interrupt handling services of the AUTOSAR OS: EnableAllInterrupts,
 * DisableAllInterrupts, ResumeAllInterrupts, SuspendAllInterrupts, ResumeOSInterrupts, SuspendOSInterrupts,
 * GetISRID.  Spec: AUTOSAR_CP_SWS_OS R25-11 (OSEK/VDX OS 2.2.3 chapter 13.3 interrupt handling):
 *   SWS_Os_00299  these services must work before StartOS and after ShutdownOS (state is static, zero-init)
 *   SWS_Os_00092  Resume/Enable without matching Suspend/Disable is ignored
 *   SWS_Os_00368  an ISR that ends with locked interrupts gets them re-enabled + ErrorHook(E_OS_DISABLEDINT)
 * Semantics:
 *   Suspend/ResumeAllInterrupts  nestable, mask EVERYTHING (target: PRIMASK)
 *   Suspend/ResumeOSInterrupts   nestable, mask only OS-related (Cat2) interrupts (target: BASEPRI)
 *   Disable/EnableAllInterrupts  NOT nestable, plain pair (target: PRIMASK)
 * The port provides the primitive (Os_Port_DisableAll/RestoreAll/SetOsMask); this file only counts nesting.
 */
#include "Os_Internal.h"

static uint8   s_allNest;        /* SuspendAllInterrupts depth */
static uint8   s_osNest;         /* SuspendOSInterrupts depth */
static boolean s_disabled;       /* DisableAllInterrupts active */
static uint32  s_allSaved;       /* PRIMASK state before the outermost SuspendAllInterrupts */
static uint32  s_disSaved;       /* PRIMASK state before DisableAllInterrupts */

void SuspendAllInterrupts(void)
{
    if (s_allNest < 255u) {
        if (s_allNest == 0u) {
            s_allSaved = Os_Port_DisableAll();     /* outermost: lock and remember previous state */
        }
        s_allNest++;
    }
}

void ResumeAllInterrupts(void)
{
    if (s_allNest == 0u) {
        return;                                    /* SWS_Os_00092: no matching Suspend */
    }
    s_allNest--;
    if (s_allNest == 0u) {
        Os_Port_RestoreAll(s_allSaved);
        Os_Port_InterruptPoint();
    }
}

void SuspendOSInterrupts(void)
{
    if (s_osNest < 255u) {
        if (s_osNest == 0u) {
            Os_Port_SetOsMask(TRUE);
        }
        s_osNest++;
    }
}

void ResumeOSInterrupts(void)
{
    if (s_osNest == 0u) {
        return;
    }
    s_osNest--;
    if (s_osNest == 0u) {
        Os_Port_SetOsMask(FALSE);
        Os_Port_InterruptPoint();
    }
}

void DisableAllInterrupts(void)
{
    if (s_disabled == FALSE) {
        s_disSaved = Os_Port_DisableAll();
        s_disabled = TRUE;
    }
}

void EnableAllInterrupts(void)
{
    if (s_disabled != FALSE) {
        s_disabled = FALSE;
        Os_Port_RestoreAll(s_disSaved);
        Os_Port_InterruptPoint();
    }
}

ISRType GetISRID(void)
{
    return (Os_IsrDepth != 0u) ? Os_IsrIds[Os_IsrDepth - 1u] : INVALID_ISR;
}

/* ---------------------------------------------------------------- kernel-internal helpers */

boolean Os_Interrupt_IsLocked(void)
{
    return (boolean)((s_allNest != 0u) || (s_osNest != 0u) || (s_disabled != FALSE));
}

/* packed state: [15:8] SuspendAll depth, [7:0] SuspendOS depth, bit 16 DisableAll flag */
uint32 Os_Interrupt_Snapshot(void)
{
    return ((uint32)s_allNest << 8) | (uint32)s_osNest | ((s_disabled != FALSE) ? 0x10000uL : 0uL);
}

/* Bring the nesting state back to `snapshot` by resuming whatever was left locked; TRUE if anything changed.
 * Order matters: undo in reverse order of the typical misuse (Disable inside Suspend), so that every
 * Os_Port_RestoreAll() restores a state that was really saved before it. */
boolean Os_Interrupt_Unwind(uint32 snapshot)
{
    boolean changed = FALSE;

    if ((s_disabled != FALSE) && ((snapshot & 0x10000uL) == 0u)) {
        EnableAllInterrupts();
        changed = TRUE;
    }
    while ((uint32)s_osNest > (snapshot & 0xFFuL)) {
        ResumeOSInterrupts();
        changed = TRUE;
    }
    while (((uint32)s_allNest << 8) > (snapshot & 0xFF00uL)) {
        ResumeAllInterrupts();
        changed = TRUE;
    }
    return changed;
}
