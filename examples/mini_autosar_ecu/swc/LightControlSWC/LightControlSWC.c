/*
 * LightControlSWC.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: an application SWC with three runnables of three different event kinds - the typical
 * "controller" SWC. HAND-WRITTEN; includes only its RTE contract header (generated into gen/LightEcu/).
 *
 * Ports (SwcTypes.arxml, LightControlSWC):
 *   R_VehicleSpeed (S/R)  VehicleSpeed uint16 0.1 km/h   <- Com signal from CAN 0x101 (the data never touches this SWC's code path
 *   R_AmbientLight (S/R)  AmbientLight uint8 0..255      <- Com signal from CAN 0x301  except through Rte_Read / Rte_IRead)
 *   P_HeadlightCmd (S/R)  HeadlightCmd uint8 0/1/2       -> LightActuatorSWC (same ECU: RTE buffer + task activation)
 *   R_Odometer     (C/S)  UpdateSpeed(IN uint16)         -> OdometerSWC server runnable (same ECU: direct call)
 *   R_EcuMode      (mode) EcuMode RUN / POST_RUN         <- BswM (mode manager)
 *
 * Runnables and the event that starts each (the RTE decides the task, see Rte_Tasks.c):
 *   LightCtl_Run20ms      TimingEvent 20 ms, DISABLED in mode POST_RUN. Explicit Rte_Read of AmbientLight, decides the command,
 *                         explicit Rte_Write of HeadlightCmd.
 *   LightCtl_OnSpeed      DataReceivedEvent on VehicleSpeed (every received 0x101 frame). Implicit Rte_IRead of the speed,
 *                         then a synchronous client/server call to the odometer.
 *   LightCtl_OnModeSwitch ModeSwitchEvent (ON-ENTRY of RUN or POST_RUN). Reads the mode with Rte_Mode, forces the light off
 *                         in POST_RUN.
 * All three share the extended task Task_LightCtl: the SAME task, so they never preempt each other and may share the PIM
 * (Rte_Pim_LightCtl_State) without any lock. This is exactly the reasoning behind "RTE event to task mapping".
 *
 * API families used and why:
 *   explicit S/R (Rte_Read/Rte_Write)  : the data are accessed at a well-defined point in the runnable, and the write must be seen
 *                                        by the receiver NOW (it activates the actuator task) - not after the runnable ended.
 *   implicit S/R (Rte_IRead)           : OnSpeed only needs one consistent value of the speed for its whole run.
 *   sync client/server (Rte_Call)      : the odometer is a service; location transparent (here: direct function call).
 *   mode (Rte_Mode + ModeSwitchEvent)  : state machine owned by BswM; this SWC reacts to it.
 *   PIM (Rte_Pim)                      : controller state between activations.
 */
#include "Rte_LightControlSWC.h"
#include "Trace.h"

/* Headlight command values (HeadlightCmdIf): same numbers as IOHWAB_LIGHT_* and as the LightStatus CAN signal */
#define LIGHT_OFF   0u
#define LIGHT_LOW   1u
#define LIGHT_HIGH  2u

/* Decision thresholds (see DESIGN.md section 11.2) */
#define AMBIENT_DARK_THRESHOLD    100u     /* below: low beam */
#define AMBIENT_VERY_DARK         30u      /* below, and fast: high beam */
#define SPEED_HIGH_BEAM_01KMH     600u     /* 60.0 km/h in 0.1 km/h */

/* Print a trace line only when the command changed or the mode changed, otherwise the log would drown in 20 ms lines */
static void lightctl_trace(const LightCtl_StateType *s, uint8 cmd)
{
    TRACE(TRACE_CAT_SWC, "LightCtl cmd=%u speed=%u ambient=%u postrun=%u",
          (unsigned)cmd, (unsigned)s->lastSpeed, (unsigned)s->lastAmbient, (unsigned)s->postRun);
}

/* ------------------------------------------------------------------------------------------------------------
 * LightCtl_Run20ms - TimingEvent TE_LightCtl_20ms (20 ms), disabledInMode POST_RUN
 * ---------------------------------------------------------------------------------------------------------- */
void LightCtl_Run20ms(void)
{
    LightCtl_StateType *s = Rte_Pim_LightCtl_State();
    uint8 ambient = 255u;                  /* default = bright: if no value is available the lamp stays off */
    uint8 cmd = LIGHT_OFF;

    /* Explicit read: RTE_E_OK, or RTE_E_COM_STOPPED while Com is not running. Keep the last value on error. */
    if (Rte_Read_R_AmbientLight_AmbientLight(&ambient) == RTE_E_OK)
    {
        s->lastAmbient = ambient;
    }
    else
    {
        ambient = s->lastAmbient;
    }

    /* Defensive: the RTE already suppresses this runnable in POST_RUN (disabledInMode), a second check costs one load. */
    if (Rte_Mode_R_EcuMode_EcuMode() == RTE_MODE_EcuMode_POST_RUN)
    {
        return;
    }

    if ((ambient < AMBIENT_VERY_DARK) && (s->lastSpeed >= SPEED_HIGH_BEAM_01KMH))
    {
        cmd = LIGHT_HIGH;
    }
    else if (ambient < AMBIENT_DARK_THRESHOLD)
    {
        cmd = LIGHT_LOW;
    }
    else
    {
        cmd = LIGHT_OFF;
    }

    if (cmd != s->lastCmd)
    {
        s->lastCmd = cmd;
        lightctl_trace(s, cmd);
    }

    /* Explicit write EVERY cycle (not only on change): the RTE stores it in its buffer and activates the actuator task,
     * which also refreshes the odometer distance in the LightStatus frame. */
    (void)Rte_Write_P_HeadlightCmd_HeadlightCmd(cmd);
}

/* ------------------------------------------------------------------------------------------------------------
 * LightCtl_OnSpeed - DataReceivedEvent DRE_LightCtl_VehicleSpeed (a 0x101 frame was received and unpacked by Com)
 * ---------------------------------------------------------------------------------------------------------- */
void LightCtl_OnSpeed(void)
{
    LightCtl_StateType *s = Rte_Pim_LightCtl_State();

    /* Implicit read: the RTE copied the signal into a runnable-private buffer BEFORE this function started
     * (Rte_CopyIn_LightCtl_OnSpeed in the task body). Calling Rte_IRead twice returns the same value. */
    uint16 speed = Rte_IRead_LightCtl_OnSpeed_R_VehicleSpeed_VehicleSpeed();

    s->lastSpeed = speed;
    s->speedRxCount++;

    /* Synchronous client/server call into OdometerSWC. The RTE realises it as a direct call in THIS task's context
     * (Task_LightCtl); inside the server an exclusive area (OS resource with ceiling priority) protects the shared distance. */
    (void)Rte_Call_R_Odometer_UpdateSpeed(speed);
}

/* ------------------------------------------------------------------------------------------------------------
 * LightCtl_OnModeSwitch - ModeSwitchEvent: ON-ENTRY of mode RUN or POST_RUN (EcuMode, switched by BswM)
 * ---------------------------------------------------------------------------------------------------------- */
void LightCtl_OnModeSwitch(void)
{
    LightCtl_StateType *s = Rte_Pim_LightCtl_State();
    Rte_ModeType_EcuMode mode = Rte_Mode_R_EcuMode_EcuMode();      /* atomic read of the current mode */

    if (mode == RTE_MODE_EcuMode_POST_RUN)
    {
        /* Entering POST_RUN: lamps off at once; Run20ms is disabled by the RTE from now on. */
        s->postRun = 1u;
        s->lastCmd = LIGHT_OFF;
        (void)Rte_Write_P_HeadlightCmd_HeadlightCmd(LIGHT_OFF);
        lightctl_trace(s, LIGHT_OFF);
    }
    else
    {
        /* Back in RUN: Run20ms is enabled again and takes over at its next period. */
        s->postRun = 0u;
    }
}
