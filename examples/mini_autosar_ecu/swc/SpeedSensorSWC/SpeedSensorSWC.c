/*
 * SpeedSensorSWC.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: an application SWC (here a sensor SWC) as written by a function developer. This file is
 * HAND-WRITTEN and is the whole "application": it knows nothing about CAN, Com, tasks, ADC or the OS. It only talks to
 * its ports through the RTE contract header (the single include below, generated into gen/SensorEcu/).
 *
 * Ports (SwcTypes.arxml, SpeedSensorSWC):
 *   R_WheelSpeed   (client/server, WheelSpeedIf)   operation GetWheelSpeed(OUT uint16 Raw)  -> IoHwAb (BSW server)
 *   P_VehicleSpeed (sender/receiver, VehicleSpeedIf) element VehicleSpeed, uint16, unit 0.1 km/h -> CAN 0x101 (via Com)
 *
 * Runnable:
 *   SpeedSensor_Run10ms   started by TimingEvent TE_SpeedSensor_10ms (every 10 ms). The RTE maps it to Task_Swc10ms
 *                         (RteEventToTaskMapping in config/ecuc/SensorEcu.ecuc.json); the SWC does not know that.
 *
 * Which RTE API family is used here, and why:
 *   - Rte_Call_R_WheelSpeed_GetWheelSpeed : SYNCHRONOUS client/server call. The server is not a SWC but BSW (IoHwAb);
 *     the RTE maps the call to IoHwAb_GetWheelSpeed (RteBswServerMapping). The SWC cannot tell the difference between a
 *     SWC server and a BSW server: that is location transparency.
 *   - Rte_IWrite_..._VehicleSpeed : IMPLICIT sender/receiver write. The value is written into a runnable-private buffer
 *     and the RTE publishes it (-> Com_SendSignal) AFTER the runnable has terminated (Rte_CopyOut in the task body).
 *     Implicit is the right choice for a cyclic "compute and publish once per activation" runnable: no API return
 *     value to handle, a single consistent publication per cycle.
 *   - Rte_Pim_SpeedSensor_State : PER-INSTANCE MEMORY. State that must survive between activations (a call counter)
 *     lives in RTE-managed memory instead of a C "static" in this file.
 *
 * Spec: AUTOSAR_CP_SWS_RTE SWS_Rte_03744 (Rte_IWrite), Rte_Call (client/server), Rte_Pim; naming per Rte_Common.h.
 */
#include "Rte_SpeedSensorSWC.h"
#include "Trace.h"      /* teaching trace only (not an AUTOSAR API); the SWC includes no BSW/MCAL/OS header */

void SpeedSensor_Run10ms(void)
{
    SpeedSensor_StateType *state = Rte_Pim_SpeedSensor_State();   /* PIM: persistent across activations */
    uint16 raw = 0u;
    uint16 speed = 0u;

    /* 1. sample the wheel-speed ADC through the client/server port (12-bit raw value, 0..4095) */
    if (Rte_Call_R_WheelSpeed_GetWheelSpeed(&raw) == RTE_E_OK)
    {
        /* 2. scale: 4095 counts = 200.0 km/h, result in 0.1 km/h (0..2000) -> fits the uint16 element */
        speed = (uint16)(((uint32)raw * 2000u) / 4095u);
    }
    else
    {
        raw = 0u;                       /* sensor not available: publish speed 0 (no error management in this demo) */
    }

    /* 3. implicit write: lands in the runnable's buffer now, is sent to Com when the runnable terminates */
    Rte_IWrite_SpeedSensor_Run10ms_P_VehicleSpeed_VehicleSpeed(speed);

    /* teaching trace: one line per 10 activations (= every 100 ms) so the log stays readable */
    state->calls++;
    if ((state->calls % 10u) == 0u)
    {
        TRACE(TRACE_CAT_SWC, "SpeedSensor raw=%u speed=%u", (unsigned)raw, (unsigned)speed);
    }
}
