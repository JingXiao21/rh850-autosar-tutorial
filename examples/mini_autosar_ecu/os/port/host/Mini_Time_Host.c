/*
 * Mini_Time_Host.c  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (platform time base, see include/Mini_Time.h).  On the host the time base
 * is the deterministic virtual time of SimTime; there is no tick interrupt of its own because the host OS
 * port drives the kernel tick itself (Os_Port_Host.c).
 */
#include "Mini_Time.h"
#include "SimTime.h"

void Mini_Time_Init(void)
{
    /* nothing to program: virtual time starts at 0 */
}

uint32 Mini_Time_GetUs(void)
{
    return (uint32)SimTime_GetUs();
}

uint32 Mini_Time_GetMs(void)
{
    return (uint32)(SimTime_GetUs() / 1000u);
}

/* never called on the host; defined weak so a simulator image may override it */
__attribute__((weak)) void Mini_Time_TickHook(void)
{
}
