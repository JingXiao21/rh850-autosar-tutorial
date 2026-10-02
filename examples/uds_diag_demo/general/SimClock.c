/*
 * SimClock.c
 *
 * [Educational Implementation] Simulated millisecond time base. See SimClock.h.
 */
#include "SimClock.h"

static uint32 SimClock_Ms;

void SimClock_Reset(void) { SimClock_Ms = 0u; }
void SimClock_Advance(uint32 ms) { SimClock_Ms += ms; }
uint32 SimClock_NowMs(void) { return SimClock_Ms; }
