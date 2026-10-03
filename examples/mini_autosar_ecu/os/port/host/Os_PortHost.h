/*
 * Os_PortHost.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (private interface between the host OS port and the virtual-time
 * simulator, HOST BUILD ONLY).  Declares the part of SimTime that only Os_Port_Host.c uses.
 */
#ifndef OS_PORTHOST_H
#define OS_PORTHOST_H

#include "Std_Types.h"

/* run all registered step hooks (HostMain: SimCan_Step, RestBus_Step, ...) for virtual time `nowUs` */
void    SimTime_RunStepHooks(uint64 nowUs);

/* take the lowest pending simulated IRQ number (set by SimTime_RaiseIsr); FALSE if none is pending */
boolean SimTime_TakePendingIsr(uint16 *irqNumber);

/* test support: forget pending IRQs and the virtual time (used by unit tests, not by the simulator) */
void    SimTime_Reset(void);

/* flush stdout and the trace log file (Trace_Host.c) */
void    Trace_HostFlush(void);

#endif /* OS_PORTHOST_H */
