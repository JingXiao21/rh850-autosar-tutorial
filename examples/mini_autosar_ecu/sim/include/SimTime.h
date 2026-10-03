/*
 * SimTime.h  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart (test infrastructure). Deterministic virtual time of the host simulation:
 * time advances ONLY when the OS idle loop (or Os_HostBurn) says so, never with wall-clock time, so
 * every host run produces byte-identical traces. Owner: agent A (os/port/host/SimTime.c).
 */
#ifndef SIMTIME_H
#define SIMTIME_H

#include "Std_Types.h"

uint64 SimTime_GetUs(void);                 /* current virtual time in microseconds since boot */
void   SimTime_AdvanceUs(uint32 us);        /* only the OS host port / Os_HostBurn call this */

/* Optional simulation end: the host port calls ShutdownOS-equivalent when virtual time reaches the limit.
 * 0 = run forever. Set from the command line (--run-ms N) in sim/host/HostMain.c. */
void   SimTime_SetEndUs(uint64 endUs);
uint64 SimTime_GetEndUs(void);

/* Host-only OS helpers (declared here to keep Os.h clean). */
void   Os_HostBurn(uint32 us);              /* "consume CPU time" inside a task: advances time, delivers ticks
                                               and ISRs at their due time, may preempt the caller */

/* Hook list executed by the host OS idle loop and by Os_HostBurn at every simulated microsecond step
 * boundary that crosses a 1 ms tick: lets the simulator (CAN script, rest-bus, ADC profile) act.
 * Registered by HostMain.c: */
typedef void (*SimTime_StepHook)(uint64 nowUs);
void   SimTime_RegisterStepHook(SimTime_StepHook hook);

/* Pend a Cat2 ISR from the simulator (e.g. CAN RX). The host OS port runs Os_Isr_<name> at the next
 * ISR delivery point (idle loop / Os_HostBurn / OS service return) with full Cat2 semantics. */
void   SimTime_RaiseIsr(uint16 irqNumber);

#endif /* SIMTIME_H */
