// SOURCES: gen/LightEcu/Rte.c swc/LightControlSWC/LightControlSWC.c swc/LightActuatorSWC/LightActuatorSWC.c swc/OdometerSWC/OdometerSWC.c tests/unit/rte_mock/rte_mocks.c
// INCLUDES: tests/unit/rte_mock
/*
 * test_rte_lightecu.c
 *
 * [Educational Implementation]
 * Unit test of the GENERATED RTE of LightEcu (gen/LightEcu/Rte.c) together with the real SWC code, against the mocks in
 * tests/unit/rte_mock. Nothing of the OS, Com or CAN is real: what is verified is exactly the RTE's own behaviour.
 *
 *   1. contract       the SWC sources compile with only their Rte_<Swc>.h (this file links them, so a missing API = build error)
 *   2. lifecycle      API refuses before Rte_Start, Com callbacks are ignored before Rte_Start
 *   3. DataReceived   Rte_COMCbk_VehicleSpeed -> SetEvent(Task_LightCtl, Ev_LightCtl_VehicleSpeed)
 *   4. implicit read  CopyIn takes a snapshot; later changes of the signal do not reach the running runnable
 *   5. intra-ECU S/R  Rte_Write stores in the RTE buffer + ActivateTask(Task_LightAct); Rte_Read returns the value
 *   6. C/S + EA       Rte_Call_R_Odometer_* is a direct call bracketed by GetResource/ReleaseResource(Res_EA_Odo)
 *   7. mode           Rte_Switch raises the ModeSwitchEvent only on change; invalid mode rejected
 *   8. SWC logic      headlight decision, POST_RUN behaviour, status/distance publication to Com
 */
#include <stdio.h>

#include "Rte.h"
#include "Rte_Cbk.h"
#include "Os.h"
#include "Com.h"
#include "rte_mocks.h"
#include "Rte_LightControlSWC.h"      /* this test plays "the integrator": it may include several contract headers */
#include "Rte_LightActuatorSWC.h"
#include "Rte_OdometerSWC.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void prepare(void)
{
    mock_reset();
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 0u);
    mock_com_set(ComConf_ComSignal_AmbientLight, 1u, 255u);
    mock_com_set(ComConf_ComSignal_EcuModeRequest, 1u, 0u);
    mock_com_set(ComConf_ComSignal_HeadlightStatus, 1u, 0u);
    mock_com_set(ComConf_ComSignal_OdometerDistance, 4u, 0u);
}

static void test_lifecycle(void)
{
    uint8 v = 7u;
    (void)Rte_Stop();
    prepare();
    CHECK(Rte_Write_P_HeadlightCmd_HeadlightCmd(1u) == RTE_E_COM_STOPPED);       /* not started: refused */
    CHECK(Rte_Read_R_AmbientLight_AmbientLight(&v) == RTE_E_COM_STOPPED);
    CHECK(mock_count("ActivateTask") == 0u);                                      /* ... and nothing was triggered */
    Rte_COMCbk_VehicleSpeed();
    CHECK(mock_count("SetEvent") == 0u);                                          /* Com callback ignored before Rte_Start */

    CHECK(Rte_Start() == RTE_E_OK);
    CHECK(Rte_Mode_EcuMode == RTE_MODE_EcuMode_RUN);                              /* initial mode from the ARXML */
    CHECK(Rte_Pim_LightCtl_State()->lastAmbient == 255u);                         /* PIM init value {0,0,0,0,255} */
    CHECK(Rte_Pim_Actuator_State()->lastState == 255u);
    CHECK(Rte_Read_R_AmbientLight_AmbientLight(&v) == RTE_E_OK && v == 255u);     /* Com init value passes through */
    CHECK(mock_irq_depth() == 0);
}

static void test_data_received_event(void)
{
    prepare();
    (void)Rte_Start();
    Rte_COMCbk_VehicleSpeed();
    CHECK(mock_count("SetEvent") == 1u);
    CHECK(mock_index_of_args("SetEvent", Task_LightCtl, 0u) == 0);
    CHECK(mock_call(0u)->b == Ev_LightCtl_VehicleSpeed);                          /* the OsEvent of the mapping */
    CHECK(mock_trace_has("TRIGGER DRE_LightCtl_VehicleSpeed -> Task_LightCtl"));
}

static void test_implicit_read_snapshot(void)
{
    prepare();
    (void)Rte_Start();
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 300u);
    Rte_CopyIn_LightCtl_OnSpeed();                                                /* task body: snapshot BEFORE the runnable */
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 999u);                       /* a new frame arrives meanwhile ... */
    LightCtl_OnSpeed();                                                           /* ... the runnable still sees 300 */
    CHECK(Rte_Pim_LightCtl_State()->lastSpeed == 300u);
    CHECK(Rte_Pim_LightCtl_State()->speedRxCount == 1u);
    Rte_CopyIn_LightCtl_OnSpeed();                                                /* next activation gets the new value */
    LightCtl_OnSpeed();
    CHECK(Rte_Pim_LightCtl_State()->lastSpeed == 999u);
    CHECK(mock_count("Com_ReceiveSignal") == 2u);                                 /* Com read once per CopyIn, not per IRead */
}

static void test_intra_ecu_sr(void)
{
    uint8 cmd = 0u;
    prepare();
    (void)Rte_Start();
    CHECK(Rte_Write_P_HeadlightCmd_HeadlightCmd(2u) == RTE_E_OK);
    CHECK(mock_index_of_args("ActivateTask", Task_LightAct, 0u) >= 0);            /* INTERNAL_WRITE trigger */
    CHECK(mock_count("Com_SendSignal") == 0u);                                    /* purely intra-ECU: Com is not involved */
    CHECK(Rte_Read_R_HeadlightCmd_HeadlightCmd(&cmd) == RTE_E_OK && cmd == 2u);
    CHECK(mock_irq_depth() == 0);
}

static void test_client_server_and_exclusive_area(void)
{
    uint32 dist = 99u;
    prepare();
    (void)Rte_Start();
    CHECK(Rte_Call_R_Odometer_UpdateSpeed(1800u) == RTE_E_OK);                    /* 180.0 km/h = 50 m/s -> 1 m per 20 ms ... */
    CHECK(Rte_Call_R_Odometer_GetDistance(&dist) == RTE_E_OK);
    CHECK(dist == 1u);                                                            /* ... = 1000 mm = 1 m */
    CHECK(mock_count("GetResource") == 2u && mock_count("ReleaseResource") == 2u);
    CHECK(mock_index_of_args("GetResource", Res_EA_Odo, 0u) < mock_index_of_args("ReleaseResource", Res_EA_Odo, 0u));
    CHECK(mock_res_depth() == 0);                                                 /* balanced */
}

static void test_mode_switch(void)
{
    prepare();
    (void)Rte_Start();
    CHECK(Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_RUN) == RTE_E_OK);        /* RUN -> RUN: no change, no event */
    CHECK(mock_count("SetEvent") == 0u);
    CHECK(Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_POST_RUN) == RTE_E_OK);
    CHECK(Rte_Mode_R_EcuMode_EcuMode() == RTE_MODE_EcuMode_POST_RUN);             /* what the SWC would read */
    CHECK(mock_count("SetEvent") == 1u);
    CHECK(mock_call(0u)->a == Task_LightCtl && mock_call(0u)->b == Ev_LightCtl_ModeSwitch);
    CHECK(mock_trace_has("MODE EcuMode=POST_RUN"));
    CHECK(Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_POST_RUN) == RTE_E_OK);   /* same mode again: no second event */
    CHECK(mock_count("SetEvent") == 1u);
    CHECK(Rte_Switch_P_EcuMode_EcuMode(42u) == RTE_E_INVALID);
    CHECK(Rte_Mode_EcuMode == RTE_MODE_EcuMode_POST_RUN);                         /* invalid request did not change anything */
    CHECK(Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_RUN) == RTE_E_OK);
    CHECK(mock_count("SetEvent") == 2u);
    CHECK(mock_irq_depth() == 0);
}

static void test_swc_logic(void)
{
    uint8 cmd = 0u;
    prepare();
    (void)Rte_Start();

    /* dusk (ambient 60) -> LOW beam, written explicitly to the actuator buffer */
    mock_com_set(ComConf_ComSignal_AmbientLight, 1u, 60u);
    LightCtl_Run20ms();
    (void)Rte_Read_R_HeadlightCmd_HeadlightCmd(&cmd);
    CHECK(cmd == 1u);
    CHECK(mock_trace_has("LightCtl cmd=1 speed=0 ambient=60 postrun=0"));

    /* dark and fast -> HIGH beam */
    mock_com_set(ComConf_ComSignal_AmbientLight, 1u, 20u);
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 700u);
    Rte_CopyIn_LightCtl_OnSpeed();
    LightCtl_OnSpeed();
    LightCtl_Run20ms();
    (void)Rte_Read_R_HeadlightCmd_HeadlightCmd(&cmd);
    CHECK(cmd == 2u);

    /* POST_RUN: the mode runnable forces OFF, Run20ms (if the task body did call it) stays silent */
    (void)Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_POST_RUN);
    LightCtl_OnModeSwitch();
    (void)Rte_Read_R_HeadlightCmd_HeadlightCmd(&cmd);
    CHECK(cmd == 0u);
    CHECK(mock_trace_has("LightCtl cmd=0 speed=700 ambient=20 postrun=1"));
    mock_reset();
    mock_com_set(ComConf_ComSignal_AmbientLight, 1u, 20u);
    LightCtl_Run20ms();
    CHECK(mock_count("ActivateTask") == 0u);                                      /* defensive mode check: no write in POST_RUN */

    /* actuator: command -> hardware via BSW server, status + distance -> Com signals */
    (void)Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_RUN);
    mock_reset();
    mock_com_set(ComConf_ComSignal_HeadlightStatus, 1u, 0u);
    mock_com_set(ComConf_ComSignal_OdometerDistance, 4u, 0u);
    (void)Rte_Call_R_Odometer_UpdateSpeed(1800u);
    (void)Rte_Write_P_HeadlightCmd_HeadlightCmd(2u);
    mock_reset();
    mock_com_set(ComConf_ComSignal_HeadlightStatus, 1u, 0u);
    mock_com_set(ComConf_ComSignal_OdometerDistance, 4u, 0u);
    Actuator_OnCmd();
    CHECK(mock_index_of_args("IoHwAb_SetHeadlight", 2u, 0u) >= 0);                /* BSW server mapping */
    CHECK(mock_com_sent(ComConf_ComSignal_HeadlightStatus) == 2u);
    CHECK(mock_com_sent(ComConf_ComSignal_OdometerDistance) == 1u);
    mock_reset();
    Actuator_OnCmd();                                                             /* same command again: hardware not touched */
    CHECK(mock_count("IoHwAb_SetHeadlight") == 0u);
    CHECK(mock_irq_depth() == 0 && mock_res_depth() == 0);

    /* Com stopped: Rte_Write reports it */
    mock_com_set_stopped(TRUE);
    CHECK(Rte_Write_P_HeadlightStatus_HeadlightStatus(1u) == RTE_E_COM_STOPPED);
    mock_com_set_stopped(FALSE);
    (void)Rte_Stop();
    CHECK(Rte_Write_P_HeadlightStatus_HeadlightStatus(1u) == RTE_E_COM_STOPPED);
}

int main(void)
{
    test_lifecycle();
    test_data_received_event();
    test_implicit_read_snapshot();
    test_intra_ecu_sr();
    test_client_server_and_exclusive_area();
    test_mode_switch();
    test_swc_logic();
    if (failures == 0) {
        printf("test_rte_lightecu: all checks passed\n");
        return 0;
    }
    printf("test_rte_lightecu: %d check(s) FAILED\n", failures);
    return 1;
}
