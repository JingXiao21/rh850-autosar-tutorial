/*
 * LightActuatorSWC.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: an actuator SWC: it receives a command from another SWC, drives hardware through a
 * BSW service, and publishes status to the network. HAND-WRITTEN; includes only its RTE contract header.
 *
 * Ports (SwcTypes.arxml, LightActuatorSWC):
 *   R_HeadlightCmd     (S/R) HeadlightCmd uint8     <- LightControlSWC (same ECU: RTE buffer)
 *   R_LightHw          (C/S) SetHeadlight(IN uint8) -> IoHwAb (BSW server; IoHwAb -> Dio -> GPIO)
 *   R_Odometer         (C/S) GetDistance(OUT uint32)-> OdometerSWC (direct call, exclusive area inside the server)
 *   P_HeadlightStatus  (S/R) HeadlightStatus uint8  -> Com signal -> CAN 0x201
 *   P_OdometerDistance (S/R) Distance uint32 [m]    -> Com signal -> CAN 0x201
 *
 * Runnable:
 *   Actuator_OnCmd   started by DataReceivedEvent DRE_Actuator_HeadlightCmd. Because provider and receiver are on the SAME
 *                    ECU the RTE implements the event as: LightControl's Rte_Write stores the value in an RTE buffer and
 *                    calls ActivateTask(Task_LightAct) (trigger INTERNAL_WRITE). The runnable runs in that lower-priority
 *                    task (priority 2) - so it can be preempted by Task_LightCtl (3) and Task_BswMain (4).
 *
 * API families: explicit Rte_Read / Rte_Write (the order and moment of publication matter: first drive the lamp, then
 * report the new state), Rte_Call to a BSW server and to a SWC server, PIM for "last driven state".
 */
#include "Rte_LightActuatorSWC.h"
#include "Trace.h"

void Actuator_OnCmd(void)
{
    Actuator_StateType *s = Rte_Pim_Actuator_State();
    uint8 cmd = 0u;
    uint32 distance = 0u;

    /* 1. fetch the command (explicit read of the RTE buffer written by LightControlSWC) */
    if (Rte_Read_R_HeadlightCmd_HeadlightCmd(&cmd) != RTE_E_OK)
    {
        return;                              /* RTE not started / no data: nothing to do */
    }

    /* 2. read the odometer (server in OdometerSWC; the RTE enters the exclusive area EA_Odo inside the server) */
    (void)Rte_Call_R_Odometer_GetDistance(&distance);

    /* 3. drive the hardware only when the command changed: IoHwAb -> Dio_WriteChannel */
    if (cmd != s->lastState)
    {
        if (Rte_Call_R_LightHw_SetHeadlight(cmd) == RTE_E_OK)
        {
            s->lastState = cmd;
            TRACE(TRACE_CAT_SWC, "Actuator state=%u dist=%u", (unsigned)cmd, (unsigned)distance);
        }
    }
    s->calls++;

    /* 4. publish status + distance: both are packed by Com into I-PDU 0x201 (sent every 100 ms by Com_MainFunctionTx) */
    (void)Rte_Write_P_HeadlightStatus_HeadlightStatus(cmd);
    (void)Rte_Write_P_OdometerDistance_Distance(distance);
}
