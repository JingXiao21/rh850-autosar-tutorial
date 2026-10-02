/*
 * SimClock.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the OS counter driven by a hardware timer
 * (on RH850/P1M-E typically an OSTM channel feeding the OS SystemCounter).
 * Here simulated time only advances when SchM_Tick() is called, which makes
 * every test deterministic: "5000 ms of S3 timeout" takes microseconds of
 * host time.
 */
#ifndef SIM_CLOCK_H
#define SIM_CLOCK_H

#include "Std_Types.h"

void SimClock_Reset(void);
void SimClock_Advance(uint32 ms);
uint32 SimClock_NowMs(void);

#endif /* SIM_CLOCK_H */
