// SOURCES: gen/LightEcu/Rte_Tasks.c gen/LightEcu/Rte.c tests/unit/rte_mock/rte_mocks.c
// INCLUDES: tests/unit/rte_mock
/*
 * test_rte_tasks.c
 *
 * [Educational Implementation]
 * Unit test of the GENERATED TASK BODIES of LightEcu (gen/LightEcu/Rte_Tasks.c): which runnables run in which order for which
 * OS event, disabledInMode, implicit copy-in/out placement, BSW task call order, ISR body. The runnables themselves are
 * replaced by recording stubs here (the real SWCs are tested in test_rte_lightecu.c), so the test sees only the task skeleton.
 */
#include <stdio.h>

#include "Rte.h"
#include "Os.h"
#include "rte_mocks.h"
#include "Rte_LightControlSWC.h"
#include "Rte_LightActuatorSWC.h"
#include "Rte_OdometerSWC.h"

DeclareTask(Task_Init);
DeclareTask(Task_BswMain);
DeclareTask(Task_LightCtl);
DeclareTask(Task_LightAct);
DeclareISR(Isr_CanRx);

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/* ---- recording runnable stubs (the "SWC" side) ---- */
static const char *order[32];
static unsigned    norder;
static void note(const char *n) { if (norder < 32u) { order[norder++] = n; } }
void LightCtl_Run20ms(void)      { note("Run20ms"); }
void LightCtl_OnSpeed(void)      { note("OnSpeed"); }
void LightCtl_OnModeSwitch(void) { note("OnModeSwitch"); }
void Actuator_OnCmd(void)        { note("OnCmd"); }
Std_ReturnType Odo_UpdateSpeed(uint16 Speed) { (void)Speed; note("UpdateSpeed"); return E_OK; }
Std_ReturnType Odo_GetDistance(uint32 *Distance) { *Distance = 0u; note("GetDistance"); return E_OK; }

static void reset_all(void) { mock_reset(); norder = 0u; (void)Rte_Start(); mock_reset(); }
static int is(unsigned i, const char *n) { return (i < norder) && (order[i] == n); }

static void test_task_lightctl(void)
{
    uint32 evs1[] = { Ev_LightCtl_Timer20ms | Ev_LightCtl_VehicleSpeed | Ev_LightCtl_ModeSwitch };
    uint32 evs2[] = { Ev_LightCtl_VehicleSpeed, Ev_LightCtl_Timer20ms };

    /* all three events at once: served in RtePositionInTask order 1,2,3 */
    reset_all();
    mock_set_events(evs1, 1u);
    CHECK(mock_run_task(Os_Task_Task_LightCtl) == 1);                              /* extended task never returns by itself */
    CHECK(norder == 3u && is(0u, "Run20ms") && is(1u, "OnSpeed") && is(2u, "OnModeSwitch"));
    CHECK(mock_index_of("WaitEvent", 0u) == 0);
    CHECK(mock_call(0u)->a == (Ev_LightCtl_Timer20ms | Ev_LightCtl_VehicleSpeed | Ev_LightCtl_ModeSwitch));
    CHECK(mock_index_of_args("ClearEvent", evs1[0], 0u) > mock_index_of("GetEvent", 0u));   /* WaitEvent, GetEvent, ClearEvent */
    CHECK(mock_count("TerminateTask") == 0u);

    /* two separate wake-ups: one runnable each, in the order the events came */
    reset_all();
    norder = 0u;
    mock_set_events(evs2, 2u);
    (void)mock_run_task(Os_Task_Task_LightCtl);
    CHECK(norder == 2u && is(0u, "OnSpeed") && is(1u, "Run20ms"));
    CHECK(mock_count("WaitEvent") == 3u);                                          /* third WaitEvent = the parked state */

    /* disabledInMode: in POST_RUN the TimingEvent runnable is skipped, the others still run */
    reset_all();
    norder = 0u;
    (void)Rte_Switch_P_EcuMode_EcuMode(RTE_MODE_EcuMode_POST_RUN);
    mock_set_events(evs1, 1u);
    (void)mock_run_task(Os_Task_Task_LightCtl);
    CHECK(norder == 2u && is(0u, "OnSpeed") && is(1u, "OnModeSwitch"));

    /* implicit read: CopyIn (Com_ReceiveSignal) happens before OnSpeed runs, never for Run20ms */
    reset_all();
    norder = 0u;
    mock_set_events(evs1, 1u);
    (void)mock_run_task(Os_Task_Task_LightCtl);
    CHECK(mock_count("Com_ReceiveSignal") == 1u);
}

static void test_other_tasks(void)
{
    reset_all();
    Os_Task_Task_LightAct();
    CHECK(norder == 1u && is(0u, "OnCmd"));
    CHECK(mock_count("TerminateTask") == 1u);                                      /* basic task terminates itself */

    reset_all();
    Os_Task_Task_BswMain();                                                        /* SchM.tasks order from the ECUC */
    CHECK(mock_call(0u)->fn[0] == 'C' && mock_index_of("Can_MainFunction_Write", 0u) == 0);
    CHECK(mock_index_of("Com_MainFunctionRx", 0u) == 1);
    CHECK(mock_index_of("Com_MainFunctionTx", 0u) == 2);
    CHECK(mock_index_of("BswM_MainFunction", 0u) == 3);
    CHECK(mock_index_of("EcuM_MainFunction", 0u) == 4);
    CHECK(mock_index_of("TerminateTask", 0u) == 5);

    reset_all();
    Os_Task_Task_Init();
    CHECK(mock_index_of("EcuM_StartupTwo", 0u) == 0 && mock_index_of("TerminateTask", 0u) == 1);

    reset_all();
    Os_Isr_Isr_CanRx();
    CHECK(mock_ncalls() == 1u && mock_index_of("Can_Isr_Rx", 0u) == 0);
}

int main(void)
{
    test_task_lightctl();
    test_other_tasks();
    if (failures == 0) {
        printf("test_rte_tasks: all checks passed\n");
        return 0;
    }
    printf("test_rte_tasks: %d check(s) FAILED\n", failures);
    return 1;
}
