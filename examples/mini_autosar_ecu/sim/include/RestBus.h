/*
 * RestBus.h
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart. "Rest-bus simulation" is the standard industry practice of emulating all
 * ECUs that are not under test; here it is the tester node of the scenario. ONE source file
 * (sim/restbus/RestBus.c, owner agent C) is used by:
 *   - host build : HostMain registers RestBus_Step as a SimTime step hook; send = SimCan_Inject
 *   - target     : third Renode machine "REST" (target/restbus/main_restbus.c) which calls
 *                  RestBus_Step from its SysTick and sends with Can_Write (real MCAL Can driver).
 * Frames produced (scenario DESIGN 11), all classic CAN, standard id:
 *   0x301 AmbientLight  dlc 1  byte0 = lux (0..255), every 100 ms starting at t=0
 *   0x3F0 EcuModeReq    dlc 1  byte0 = 0 (RUN) / 1 (POST_RUN), every 100 ms
 *   0x101 VehicleSpeed  dlc 2  (only with RESTBUS_FLAG_EMULATE_SPEED: stands in for ECU_A on host runs
 *                               of ECU_B that have no SensorEcu TX log), every 20 ms, LE uint16, 0.1 km/h
 * Profiles are pure functions of time (exported for the tests):
 *   RestBus_AmbientLux(t_ms):  200 for t<500 ; 60 for 500<=t<1500 ; 20 for t>=1500
 *   RestBus_ModeRequest(t_ms): 1 for 3000<=t<3500 ; else 0
 *   RestBus_SpeedRaw01kmh(t_ms) = SimAdc_ProfileRaw-based: min(3000,3*(t-200)) for t>200 else 0, scaled
 *                                 *2000/4095 (same formula as SpeedSensorSWC)
 */
#ifndef RESTBUS_H
#define RESTBUS_H

#include "Std_Types.h"

#define RESTBUS_FLAG_EMULATE_SPEED   0x01u

typedef boolean (*RestBus_SendFn)(uint32 id, uint8 dlc, const uint8 *data);   /* TRUE = accepted */

void   RestBus_Init(RestBus_SendFn send, uint32 flags);
void   RestBus_Step(uint32 nowMs);                  /* call at >= 1 kHz; sends every frame whose period elapsed */
uint8  RestBus_AmbientLux(uint32 timeMs);
uint8  RestBus_ModeRequest(uint32 timeMs);
uint16 RestBus_Speed01kmh(uint32 timeMs);
/* [ADDED by agent C] frame received from the bus (target REST node: 0x101 / 0x201); prints one SIM trace line */
void   RestBus_OnRx(uint32 id, uint8 dlc, const uint8 *data);

#endif /* RESTBUS_H */
