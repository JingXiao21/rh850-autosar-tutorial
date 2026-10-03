/*
 * SimTime.c  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (test infrastructure; the closest real thing is the hardware timer that a
 * Gpt/OS tick driver programs).  Implements sim/include/SimTime.h: deterministic virtual time that advances
 * ONLY when the OS host port says so (idle loop / Os_HostBurn), the optional end-of-simulation time, the
 * list of per-millisecond step hooks and the set of simulated pending Cat2 interrupts.
 */
#include "SimTime.h"
#include "Os_PortHost.h"

#define SIMTIME_MAX_HOOKS   4u
#define SIMTIME_MAX_IRQ     256u

static uint64           s_nowUs;
static uint64           s_endUs;
static SimTime_StepHook s_hooks[SIMTIME_MAX_HOOKS];
static uint32           s_pending[SIMTIME_MAX_IRQ / 32u];   /* one bit per IRQ number */

uint64 SimTime_GetUs(void)
{
    return s_nowUs;
}

void SimTime_AdvanceUs(uint32 us)
{
    s_nowUs += us;
}

void SimTime_SetEndUs(uint64 endUs)
{
    s_endUs = endUs;
}

uint64 SimTime_GetEndUs(void)
{
    return s_endUs;
}

void SimTime_RegisterStepHook(SimTime_StepHook hook)
{
    uint32 i;

    for (i = 0u; i < SIMTIME_MAX_HOOKS; i++) {
        if (s_hooks[i] == hook) {
            return;                                /* idempotent */
        }
    }
    for (i = 0u; i < SIMTIME_MAX_HOOKS; i++) {
        if (s_hooks[i] == NULL_PTR) {
            s_hooks[i] = hook;
            return;
        }
    }
}

void SimTime_RunStepHooks(uint64 nowUs)
{
    uint32 i;

    for (i = 0u; i < SIMTIME_MAX_HOOKS; i++) {
        if (s_hooks[i] != NULL_PTR) {
            s_hooks[i](nowUs);
        }
    }
}

/* The simulator "hardware" raises an interrupt line; the host OS port runs the ISR at its next delivery point. */
void SimTime_RaiseIsr(uint16 irqNumber)
{
    if (irqNumber < SIMTIME_MAX_IRQ) {
        s_pending[irqNumber / 32u] |= (1uL << (irqNumber % 32u));
    }
}

boolean SimTime_TakePendingIsr(uint16 *irqNumber)
{
    uint32 w;
    uint32 b;

    for (w = 0u; w < (SIMTIME_MAX_IRQ / 32u); w++) {
        if (s_pending[w] != 0u) {
            for (b = 0u; b < 32u; b++) {           /* lowest number first: deterministic order */
                if ((s_pending[w] & (1uL << b)) != 0u) {
                    s_pending[w] &= ~(1uL << b);
                    *irqNumber = (uint16)((w * 32u) + b);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

void SimTime_Reset(void)
{
    uint32 i;

    s_nowUs = 0u;
    s_endUs = 0u;
    for (i = 0u; i < (SIMTIME_MAX_IRQ / 32u); i++) {
        s_pending[i] = 0u;
    }
    for (i = 0u; i < SIMTIME_MAX_HOOKS; i++) {
        s_hooks[i] = NULL_PTR;
    }
}
