/*
 * Trace_Port.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (project-specific debug trace, see include/Trace.h).
 * Private contract between the portable formatter os/src/Trace.c and the two backends
 * os/port/cm33/Trace_Target.c and os/port/host/Trace_Host.c.  It deliberately does NOT use any OS service,
 * so Trace.c + Trace_Target.c also link into the bare-metal rest-bus image that has no OS.
 */
#ifndef TRACE_PORT_H
#define TRACE_PORT_H

#include "Std_Types.h"

/* short critical section so that two contexts never interleave characters of one line
 * (target: PRIMASK save + cpsid; host: single threaded, no-op) */
uint32 Trace_PortLock(void);
void   Trace_PortUnlock(uint32 prev);

#endif /* TRACE_PORT_H */
