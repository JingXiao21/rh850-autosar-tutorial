// SOURCES: bsw/ecum/EcuM.c bsw/bswm/BswM.c bsw/schm/SchM.c bsw/det/Det.c
// [Educational Implementation] Host unit test of EcuM + BswM + SchM + Det together: EcuM_Init order
// (InitZero -> InitOne -> StartOS), the StartPostOS sequence of EcuM_StartupTwo (SchM_Start, BswM_Init, SchM_Init,
// SchM_StartTiming), the BswM startup/run action lists driven by EcuM state pushes, RUN request bookkeeping,
// POST_RUN, shutdown, and the Det ring buffer. OS, Com and the driver callouts are mocked.
#include "test_support.h"
#include "EcuM.h"
#include "BswM.h"
#include "SchM.h"
#include "Det.h"
#include "Com.h"

#if BSWM_NUM_RULES < 2
#error "this test needs BSWM_NUM_RULES >= 2"
#endif

/* ---- call log shared by all mocks ---- */
static char calls[128];
static void log_call(const char *s) { (void)strcat(calls, s); (void)strcat(calls, ";"); }

static int startOsCalls, shutdownCalls;
static StatusType shutdownErr = 0xFF;
void StartOS(AppModeType m) { CHECK_EQ(m, OSDEFAULTAPPMODE); startOsCalls++; log_call("StartOS"); }
void ShutdownOS(StatusType e) { shutdownCalls++; shutdownErr = e; log_call("ShutdownOS"); }

/* generated EcuM_Cfg.c callouts */
void EcuM_AL_DriverInitZero(void) { log_call("Zero"); }
void EcuM_AL_DriverInitOne(void)  { log_call("One"); }
void EcuM_AL_DriverInitTwo(void)  { log_call("Two"); }
const Det_ConfigType Det_Config = { 0u };
uint8 Com_ReceiveSignal(Com_SignalIdType id, void *p) { (void)id; (void)p; return COM_SERVICE_NOT_AVAILABLE; }

/* BswM_Cfg.c equivalents: AL_Startup = { InitTwo, Can_Init, Rte_Start, RequestRUN }, AL_Run = { CanStart, GroupStart } */
static Std_ReturnType act_two(void)   { EcuM_AL_DriverInitTwo(); return E_OK; }
static Std_ReturnType act_can(void)   { log_call("Can_Init"); return E_OK; }
static Std_ReturnType act_rte(void)   { log_call("Rte_Start"); return E_OK; }
static Std_ReturnType act_req(void)   { log_call("RequestRUN"); return EcuM_RequestRUN(EcuMConf_EcuMFlexUserConfig_BswM); }
static Std_ReturnType act_start(void) { log_call("CanStart"); return E_OK; }
static Std_ReturnType act_group(void) { log_call("GroupStart"); return E_OK; }
static const BswM_ActionItemType itStartup[] = {
    { act_two, "EcuM_AL_DriverInitTwo" }, { act_can, "Can_Init" }, { act_rte, "Rte_Start" }, { act_req, "EcuM_RequestRUN" }
};
static const BswM_ActionItemType itRun[] = { { act_start, "CanIf_Start" }, { act_group, "Com_IpduGroupStart" } };
static const BswM_ActionListType alStartup = { 4u, itStartup };
static const BswM_ActionListType alRun     = { 2u, itRun };
static const BswM_RuleType rules[2] = {
    { "R_Startup", BSWM_SRC_ECUM_STATE, 0u, ECUM_STATE_STARTUP, &alStartup, NULL_PTR },
    { "R_Run",     BSWM_SRC_ECUM_STATE, 0u, ECUM_STATE_RUN,     &alRun,     NULL_PTR }
};
const BswM_ConfigType BswM_Config = { 2u, rules, 1u };

int main(void)
{
    CHECK(EcuM_GetState() == ECUM_STATE_OFF);

    /* ---- EcuM_Init: pre-OS part ---- */
    EcuM_Init();                                         /* StartOS is mocked and returns (MINI_UNIT_TEST) */
    CHECK(strcmp(calls, "Zero;One;StartOS;") == 0);
    CHECK_EQ(startOsCalls, 1);
    CHECK(EcuM_GetState() == ECUM_STATE_STARTUP);
    CHECK(t_traceFind("BSW ECUM INIT_ZERO") >= 0);
    CHECK(t_traceFind("BSW ECUM INIT_ZERO") < t_traceFind("BSW ECUM INIT_ONE"));

    /* ---- EcuM_StartupTwo (called from Task_Init) ---- */
    calls[0] = '\0';
    EcuM_StartupTwo();
    CHECK(strcmp(calls, "Two;Can_Init;Rte_Start;RequestRUN;CanStart;GroupStart;") == 0);
    CHECK(EcuM_GetState() == ECUM_STATE_RUN);
    {   /* R25-11 StartPostOS order visible in the trace */
        int p0 = t_traceFind("BSW ECUM STARTUP_TWO");
        int p1 = t_traceFind("BSW SCHM START");
        int p2 = t_traceFind("BSW SCHM INIT");
        int p3 = t_traceFind("BSW SCHM TIMING");
        int p4 = t_traceFind("BSW BSWM RULE R_Startup TRUE");
        int p5 = t_traceFind("BSW ECUM STATE RUN");
        int p6 = t_traceFind("BSW BSWM RULE R_Run TRUE");
        CHECK(p0 >= 0 && p0 < p1 && p1 < p2 && p2 < p3 && p3 < p4 && p4 < p5 && p5 < p6);
        CHECK(t_traceCount("BSW BSWM ACTION Rte_Start rc=0") == 1);
    }

    /* ---- RUN request bookkeeping: BswM holds RUN -> main function keeps RUN ---- */
    EcuM_MainFunction();
    CHECK(EcuM_GetState() == ECUM_STATE_RUN);
    CHECK_EQ(EcuM_ReleaseRUN(EcuMConf_EcuMFlexUserConfig_BswM), E_OK);
    EcuM_MainFunction();
    CHECK(EcuM_GetState() == ECUM_STATE_POST_RUN);
    CHECK(t_traceCount("BSW ECUM STATE POST_RUN") == 1);
    EcuM_MainFunction();                                 /* stays */
    CHECK(t_traceCount("BSW ECUM STATE POST_RUN") == 1);
    CHECK_EQ(EcuM_RequestRUN(EcuMConf_EcuMFlexUserConfig_BswM), E_OK);
    EcuM_MainFunction();
    CHECK(EcuM_GetState() == ECUM_STATE_RUN);
    /* unknown user -> Det development error + E_NOT_OK */
    {
        uint16 before = Det_GetErrorCount();
        CHECK_EQ(EcuM_RequestRUN(200u), E_NOT_OK);
        CHECK_EQ(EcuM_ReleaseRUN(200u), E_NOT_OK);
        CHECK_EQ(Det_GetErrorCount(), before + 2);
        CHECK(t_traceCount("DET ERROR mod=10 inst=0 api=3 err=3") == 1);
    }

    /* ---- shutdown path ---- */
    calls[0] = '\0';
    EcuM_Shutdown();
    CHECK(EcuM_GetState() == ECUM_STATE_SHUTDOWN);
    CHECK_EQ(shutdownCalls, 1);
    CHECK_EQ(shutdownErr, E_OK);
    CHECK(t_traceCount("BSW ECUM STATE SHUTDOWN") == 1);
    EcuM_MainFunction();                                 /* no state change in SHUTDOWN */
    CHECK(EcuM_GetState() == ECUM_STATE_SHUTDOWN);

    /* ---- Det ring buffer (entries so far: the two EcuM errors) ---- */
    {
        Det_EntryType e;
        uint16 n = Det_GetErrorCount();
        CHECK(Det_GetEntry(n - 1u, &e));
        CHECK_EQ(e.moduleId, MINI_MODULE_ECUM);
        CHECK_EQ(e.errorId, MINI_E_PARAM_INVALID);
        CHECK_EQ(e.kind, 0);
        CHECK(!Det_GetEntry(n, &e));
        (void)Det_ReportRuntimeError(MINI_MODULE_COM, 1u, 2u, 3u);
        CHECK(Det_GetEntry(n, &e));
        CHECK_EQ(e.kind, 1);
        CHECK(t_traceCount("DET RUNTIME mod=50 inst=1 api=2 err=3") == 1);
        {   /* more than 16 reports: ring keeps the newest 16, oldest first */
            int i;
            for (i = 0; i < 20; i++) { (void)Det_ReportError(1u, 0u, 0u, (uint8)i); }
            CHECK(Det_GetEntry(0u, &e));  CHECK_EQ(e.errorId, 4);
            CHECK(Det_GetEntry(15u, &e)); CHECK_EQ(e.errorId, 19);
            CHECK(!Det_GetEntry(16u, &e));
        }
        Det_Init(&Det_Config);
        CHECK_EQ(Det_GetErrorCount(), 0);
    }
    CHECK_EQ(t_osLockDepth, 0);
    TEST_DONE();
}
