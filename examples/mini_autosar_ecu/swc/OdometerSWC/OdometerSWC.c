/*
 * OdometerSWC.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: a pure SERVER SWC (provides a client/server interface, owns state, has no events of its own
 * except OperationInvokedEvents). HAND-WRITTEN; includes only its RTE contract header.
 *
 * Port (SwcTypes.arxml, OdometerSWC):
 *   P_Odometer (C/S, OdometerIf)   UpdateSpeed(IN uint16 Speed)   GetDistance(OUT uint32 Distance)
 *
 * Runnables = the two server operations, started by OperationInvokedEvents OIE_Odo_UpdateSpeed / OIE_Odo_GetDistance.
 * RteEventToTaskMapping maps them to NO task (task = null): this is a synchronous call inside one partition, so the RTE
 * implements Rte_Call_R_Odometer_* in the CLIENT as a plain function call. Consequences that this file demonstrates:
 *   - the server code runs in the CALLER's task, with the caller's priority (Task_LightCtl for UpdateSpeed, Task_LightAct for
 *     GetDistance). Both are therefore potential concurrent users of the PIM ("canBeInvokedConcurrently = true").
 *   - concurrency is handled with an EXCLUSIVE AREA (EA_Odo). The RTE implements it as an OS resource with priority ceiling
 *     = highest priority among all accessing tasks (3): while Task_LightAct (prio 2) is inside GetDistance, Task_LightCtl
 *     (prio 3) cannot preempt it, so it can never observe a half-updated distance. Cost: no extra task, no blocking.
 *
 * Why the PIM holds millimetres: UpdateSpeed adds speed*20/36 mm per call, 20 ms assumed between calls (speed is 0.1 km/h:
 * v[m/s] = speed/36, distance per 20 ms = speed/36*0.02 m = speed*20/36 mm). Integer maths keeps the sub-metre part.
 */
#include "Rte_OdometerSWC.h"
#include "Trace.h"

Std_ReturnType Odo_UpdateSpeed(uint16 Speed)
{
    Odo_StateType *s = Rte_Pim_Odo_State();
    uint32 updates;
    uint32 distanceM;

    Rte_Enter_EA_Odo();                         /* GetResource(Res_EA_Odo): ceiling priority protects the PIM */
    s->distanceMm += ((uint32)Speed * 20u) / 36u;
    s->updates++;
    updates = s->updates;
    distanceM = s->distanceMm / 1000u;
    Rte_Exit_EA_Odo();                          /* ReleaseResource: a higher-priority task may now preempt */

    if ((updates % 50u) == 0u)                  /* about once per second at 20 ms update period */
    {
        TRACE(TRACE_CAT_SWC, "Odometer dist_m=%u", (unsigned)distanceM);
    }
    return RTE_E_OK;
}

Std_ReturnType Odo_GetDistance(uint32 *Distance)
{
    Odo_StateType *s = Rte_Pim_Odo_State();

    Rte_Enter_EA_Odo();
    *Distance = s->distanceMm / 1000u;          /* metres, consistent snapshot of the 32-bit value */
    Rte_Exit_EA_Odo();
    return RTE_E_OK;
}
