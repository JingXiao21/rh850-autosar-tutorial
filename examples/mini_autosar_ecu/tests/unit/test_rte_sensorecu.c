// SOURCES: gen/SensorEcu/Rte.c gen/SensorEcu/Rte_Tasks.c swc/SpeedSensorSWC/SpeedSensorSWC.c tests/unit/rte_mock/rte_mocks.c
// INCLUDES: gen/SensorEcu swc/SpeedSensorSWC tests/unit/rte_mock
/*
 * test_rte_sensorecu.c
 *
 * [Educational Implementation]
 * Unit test of the generated RTE of SensorEcu with the real SpeedSensorSWC: IMPLICIT write semantics (the value reaches Com
 * only after the runnable terminated, from Rte_CopyOut in the task body), the client/server call to the BSW server IoHwAb,
 * the task body of Task_Swc10ms and the PIM-based trace throttling.
 * (INCLUDES lists gen/SensorEcu first so that it shadows gen/LightEcu which the runner adds for all unit tests.)
 */
#include <stdio.h>

#include "Rte.h"
#include "Os.h"
#include "Com.h"
#include "rte_mocks.h"

DeclareTask(Task_Swc10ms);

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int sent_before_runnable_end;
static void hook_check_not_yet_sent(void)      /* called from inside IoHwAb_GetWheelSpeed = while the runnable is running */
{
    sent_before_runnable_end = (int)mock_count("Com_SendSignal");
}

int main(void)
{
    uint32 i;

    mock_reset();
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 0u);
    CHECK(Rte_Start() == RTE_E_OK);
    mock_reset();
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 0u);

    /* one activation of the task: raw 2048 -> 2048*2000/4095 = 1000 (100.0 km/h) */
    mock_wheel_raw = 2048u;
    mock_iohwab_hook = hook_check_not_yet_sent;
    Os_Task_Task_Swc10ms();
    CHECK(sent_before_runnable_end == 0);                                          /* implicit write: nothing sent while the runnable runs */
    CHECK(mock_count("Com_SendSignal") == 1u);                                     /* ... exactly one publication at the end */
    CHECK(mock_com_sent(ComConf_ComSignal_VehicleSpeed) == 1000u);
    CHECK(mock_index_of("IoHwAb_GetWheelSpeed", 0u) < mock_index_of("Com_SendSignal", 0u));
    CHECK(mock_index_of("Com_SendSignal", 0u) < mock_index_of("TerminateTask", 0u));
    CHECK(mock_trace_has("WRITE P_VehicleSpeed_VehicleSpeed=1000"));
    CHECK(mock_trace_has("CALL R_WheelSpeed_GetWheelSpeed"));

    /* trace throttling uses the PIM: one SWC trace line per 10 activations (Rte_Start re-initialises the PIM) */
    (void)Rte_Start();
    mock_reset();
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 0u);
    mock_wheel_raw = 4095u;
    for (i = 0u; i < 9u; i++) { Os_Task_Task_Swc10ms(); }
    CHECK(!mock_trace_has("SpeedSensor raw="));
    Os_Task_Task_Swc10ms();
    CHECK(mock_trace_has("SpeedSensor raw=4095 speed=2000"));
    CHECK(mock_irq_depth() == 0);

    /* Rte_Stop: publication refused */
    (void)Rte_Stop();
    mock_reset();
    mock_com_set(ComConf_ComSignal_VehicleSpeed, 2u, 0u);
    Os_Task_Task_Swc10ms();
    CHECK(mock_com_sent(ComConf_ComSignal_VehicleSpeed) == 0u || mock_count("Com_SendSignal") == 0u);

    if (failures == 0) {
        printf("test_rte_sensorecu: all checks passed\n");
        return 0;
    }
    printf("test_rte_sensorecu: %d check(s) FAILED\n", failures);
    return 1;
}
