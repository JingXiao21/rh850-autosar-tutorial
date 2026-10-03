/*
 * Mini_Time.h
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart (the nearest real concepts are the free-running timer of the Gpt driver / StbM
 * time base). Platform time base shared by Trace and the OS port:
 *   - target: owns SysTick. Mini_Time_Init() programs SysTick for 1 kHz (reload = MINI_CPU_CLOCK_HZ/1000-1,
 *     CLKSOURCE=core, TICKINT=1) and `SysTick_Handler` increments the millisecond count and then calls
 *     Mini_Time_TickHook(); the OS port overrides the weak hook with its kernel tick handler.
 *     Mini_Time_GetUs() = ms*1000 + (reload-SysTick->VAL)*1000/(reload+1)  (monotonic, valid inside ISRs).
 *     Before Mini_Time_Init() every getter returns 0 (trace lines of EcuM_AL_DriverInitZero/One show t=0).
 *   - host  : thin wrapper over SimTime_GetUs(); Mini_Time_Init() is a no-op; the hook is never used
 *     (the host OS port drives the kernel tick itself).
 * Owner: agent A (os/port/cm33/Mini_Time_Target.c, os/port/host/Mini_Time_Host.c).
 * The REST-bus image (no OS) calls Mini_Time_Init() in main() and overrides Mini_Time_TickHook() to run RestBus_Step.
 */
#ifndef MINI_TIME_H
#define MINI_TIME_H

#include "Std_Types.h"

void   Mini_Time_Init(void);
uint32 Mini_Time_GetMs(void);
uint32 Mini_Time_GetUs(void);
void   Mini_Time_TickHook(void);     /* weak no-op default; called once per 1 ms tick from interrupt context */

#endif /* MINI_TIME_H */
