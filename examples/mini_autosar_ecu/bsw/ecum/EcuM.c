/*
 * EcuM.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: ECU State Manager, FLEXIBLE variant (AUTOSAR_CP_SWS_ECUStateManager). R25-11 only
 * defines the flexible variant (the "fixed" EcuM of R4.0 is gone). Implemented:
 *   EcuM_Init            SWS_EcuM_02811 (SWS 8.3.2.2) : DriverInitZero -> DriverInitOne -> StartOS. NEVER returns.
 *   EcuM_StartupTwo      SWS_EcuM_02838 (SWS 8.3.2.3) : StartPostOS sequence (SWS 7.3.3, Figure 7.5)
 *   EcuM_RequestRUN/ReleaseRUN, EcuM_MainFunction : RUN <-> POST_RUN bookkeeping
 *   EcuM_GoDownHaltPoll  (SWS 8.3.2.1, minimal) / EcuM_Shutdown SWS_EcuM_02812 : SHUTDOWN, ShutdownOS(E_OK)
 *   callouts EcuM_AL_DriverInitZero/One SWS_EcuM_02905/02907: bodies are GENERATED (EcuM_Cfg.c) from the lists
 *   EcuMDriverInitListZero/One in the ECUC JSON.
 * Not implemented: sleep/wakeup, alarm clock, reset reasons, shutdown targets, EcuM_SetState, partition support.
 *
 * Startup (DESIGN 9):
 *   main -> EcuM_Init: [InitZero: Trace_Init, Det_Init/Start] [InitOne: Mcu/Port/Adc] StartOS(OSDEFAULTAPPMODE)
 *   OS autostarts Task_Init, whose body is EcuM_StartupTwo(); TerminateTask();   (SWS 7.3: "the integrator has to
 *   implement an automatically started OS task that calls EcuM_StartupTwo as its first action")
 *   EcuM_StartupTwo = StartPostOS: SchM_Start, BswM_Init, SchM_Init, SchM_StartTiming; then state STARTUP is
 *   pushed to BswM (rule R_Startup -> AL_Startup: driver init two, Can/CanIf/PduR/Com init, Rte_Start,
 *   EcuM_RequestRUN); state RUN is then entered and pushed (rule R_Run -> AL_Run: start CAN, start I-PDU group,
 *   Rte mode RUN).
 *
 * R25-11 INCONSISTENCY (documented on purpose): the RTE SWS (5.8.1 Rte_Start, "The ECU state manager calls the
 * startup routine Rte_Start of the RTE at the end of ...") says EcuM calls Rte_Start, while the BSW Mode Manager
 * SWS defines the action BswMRteStart (ECUC_BswM_01073: "the function Rte_Start(void) shall be called") and in the
 * flexible EcuM the whole post-OS init is driven by BswM. We follow the BswM SWS: Rte_Start is an action item of
 * the generated BswM action list AL_Startup. EcuM.c therefore never calls Rte_Start itself.
 */
#include "EcuM.h"
#include "BswM.h"
#include "SchM.h"
#include "Det.h"
#include "Os.h"
#include "Trace.h"
#include "Mini_Cfg.h"
#include "Mini_ModuleIds.h"

#ifndef ECUM_NUM_USERS
#define ECUM_NUM_USERS 8u              /* generated EcuM_Cfg.h normally defines this */
#endif

#define ECUM_API_STARTUP_TWO   0x1Au
#define ECUM_API_REQUEST_RUN   0x03u
#define ECUM_API_RELEASE_RUN   0x04u

static EcuM_StateType ecum_state = ECUM_STATE_OFF;
static uint8          ecum_runRequests;       /* bit n = user n holds a RUN request */

static const char *ecum_stateName(EcuM_StateType s)
{
    switch (s) {
    case ECUM_STATE_STARTUP:  return "STARTUP";
    case ECUM_STATE_RUN:      return "RUN";
    case ECUM_STATE_POST_RUN: return "POST_RUN";
    case ECUM_STATE_SHUTDOWN: return "SHUTDOWN";
    default:                  return "OFF";
    }
}

/* every state change: remember, trace, (optionally) push to BswM */
static void ecum_setState(EcuM_StateType s, boolean pushToBswM)
{
    ecum_state = s;
    TRACE(TRACE_CAT_BSW, "ECUM STATE %s", ecum_stateName(s));
    if (pushToBswM) {
        BswM_EcuM_CurrentState(s);
    }
}

/* Default of the optional callout (SWS 8.5.2.x EcuM_AL_SetProgrammableInterrupts): on this target the OS port
 * programs the NVIC priorities / enables the Cat2 IRQs in StartOS, so nothing is left to do here. Weak so that an
 * integrator (or a test) can override it. */
void MINI_WEAK EcuM_AL_SetProgrammableInterrupts(void)
{
}

void EcuM_Init(void)
{
    ecum_runRequests = 0u;
    EcuM_AL_DriverInitZero();                       /* Trace_Init, Det_Init, Det_Start: trace works from here */
    ecum_setState(ECUM_STATE_STARTUP, FALSE);       /* BswM does not exist yet: not pushed */
    TRACE(TRACE_CAT_BSW, "ECUM INIT_ZERO");
    EcuM_AL_DriverInitOne();                        /* Mcu_Init, clock, Port_Init, Adc_Init */
    TRACE(TRACE_CAT_BSW, "ECUM INIT_ONE");
    EcuM_AL_SetProgrammableInterrupts();
    StartOS(OSDEFAULTAPPMODE);                      /* does not return: Task_Init will call EcuM_StartupTwo */
#if !defined(MINI_UNIT_TEST)
    for (;;) { }                                    /* unreachable; unit tests mock StartOS and return */
#endif
}

void EcuM_StartupTwo(void)
{
    TRACE(TRACE_CAT_BSW, "ECUM STARTUP_TWO");
    /* ---- StartPostOS sequence, EcuM SWS 7.3.3 / Figure 7.5 ---- */
    (void)SchM_Start();
    BswM_Init(&BswM_Config);
    SchM_Init();
    SchM_StartTiming();
    /* ---- hand over to BswM: it runs the configured action lists ---- */
    BswM_EcuM_CurrentState(ECUM_STATE_STARTUP);     /* R_Startup -> AL_Startup (ends with EcuM_RequestRUN) */
    ecum_setState(ECUM_STATE_RUN, TRUE);            /* R_Run -> AL_Run */
}

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user)
{
    if (user >= ECUM_NUM_USERS) {
        (void)Det_ReportError(MINI_MODULE_ECUM, 0u, ECUM_API_REQUEST_RUN, MINI_E_PARAM_INVALID);
        return E_NOT_OK;
    }
    SchM_Enter_BswM_BSWM_EXCLUSIVE_AREA_0();        /* EcuM has no own exclusive area in SchM.h: reuse one OS lock */
    ecum_runRequests = (uint8)(ecum_runRequests | (uint8)(1u << user));
    SchM_Exit_BswM_BSWM_EXCLUSIVE_AREA_0();
    return E_OK;
}

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user)
{
    if (user >= ECUM_NUM_USERS) {
        (void)Det_ReportError(MINI_MODULE_ECUM, 0u, ECUM_API_RELEASE_RUN, MINI_E_PARAM_INVALID);
        return E_NOT_OK;
    }
    SchM_Enter_BswM_BSWM_EXCLUSIVE_AREA_0();
    ecum_runRequests = (uint8)(ecum_runRequests & (uint8)~(1u << user));
    SchM_Exit_BswM_BSWM_EXCLUSIVE_AREA_0();
    return E_OK;
}

/* Called from Task_BswMain (10 ms). RUN -> POST_RUN when every user released RUN; POST_RUN -> RUN when requested
 * again. The real flexible EcuM would continue POST_RUN -> (OnGoOffOne) -> SHUTDOWN/SLEEP after the BswM
 * decided so; here only an explicit EcuM_Shutdown() leaves POST_RUN. */
void EcuM_MainFunction(void)
{
    uint8 req;
    if ((ecum_state != ECUM_STATE_RUN) && (ecum_state != ECUM_STATE_POST_RUN)) {
        return;
    }
    SchM_Enter_BswM_BSWM_EXCLUSIVE_AREA_0();
    req = ecum_runRequests;
    SchM_Exit_BswM_BSWM_EXCLUSIVE_AREA_0();
    if ((ecum_state == ECUM_STATE_RUN) && (req == 0u)) {
        ecum_setState(ECUM_STATE_POST_RUN, TRUE);
    } else if ((ecum_state == ECUM_STATE_POST_RUN) && (req != 0u)) {
        ecum_setState(ECUM_STATE_RUN, TRUE);
    } else {
        /* no change */
    }
}

EcuM_StateType EcuM_GetState(void)
{
    return ecum_state;
}

/* Minimal shutdown path: no wake-up sources, no sleep, no halt/poll loop. */
Std_ReturnType EcuM_GoDownHaltPoll(uint16 caller)
{
    (void)caller;
    ecum_setState(ECUM_STATE_SHUTDOWN, TRUE);
    ShutdownOS(E_OK);                               /* ShutdownHook -> host: end of simulation, target: halt */
    return E_OK;                                    /* only reached when ShutdownOS is mocked (unit test) */
}

void EcuM_Shutdown(void)
{
    (void)EcuM_GoDownHaltPoll(0u);
}

void EcuM_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo != NULL_PTR) {
        versioninfo->vendorID = MINI_VENDOR_ID;
        versioninfo->moduleID = MINI_MODULE_ECUM;
        versioninfo->sw_major_version = 1u;
        versioninfo->sw_minor_version = 0u;
        versioninfo->sw_patch_version = 0u;
    }
}
