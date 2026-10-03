/*
 * Main_Target.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the integrator's main() (AUTOSAR_CP_SWS_ECUStateManager 7.3 "STARTUP Phase": after
 * the reset handler and a minimal MCU init, main() calls EcuM_Init(), which in turn calls StartOS and never
 * returns). Target only (file name suffix _Target); the host build uses sim/host/HostMain.c instead.
 */
#include "EcuM.h"

int main(void)
{
    EcuM_Init();            /* DriverInitZero -> DriverInitOne -> StartOS: does not return */
    for (;;) { }
}
