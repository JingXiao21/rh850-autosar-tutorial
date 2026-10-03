// SOURCES: bsw/bswm/BswM.c
// [Educational Implementation] Host unit test of the BswM rule engine: pushed sources (EcuM state, BswM_RequestMode),
// polled Com-signal source, transition semantics (first evaluation executes, no re-execution without a change),
// action tracing, Det checks. Com and Det are mocked.
#define TEST_MOCK_DET
#include "test_support.h"
#include "BswM.h"
#include "Com.h"

#if BSWM_NUM_RULES < 3
#error "this test needs BSWM_NUM_RULES >= 3"
#endif

static uint8 comValue;
static uint8 comRc = E_OK;
uint8 Com_ReceiveSignal(Com_SignalIdType id, void *p) { CHECK_EQ(id, 3); if (comRc == E_OK) { *(uint8 *)p = comValue; } return comRc; }

/* action bodies record the order in which they ran */
static char order[64];
static void rec(char c) { size_t n = strlen(order); order[n] = c; order[n + 1u] = '\0'; }
static Std_ReturnType a_run1(void)   { rec('a'); return E_OK; }
static Std_ReturnType a_run2(void)   { rec('b'); return E_OK; }
static Std_ReturnType a_notrun(void) { rec('n'); return E_NOT_OK; }
static Std_ReturnType a_req(void)    { rec('r'); return E_OK; }
static Std_ReturnType a_comT(void)   { rec('T'); return E_OK; }
static Std_ReturnType a_comF(void)   { rec('F'); return E_OK; }

static const BswM_ActionItemType itemsRun[]    = { { a_run1, "A_Run1" }, { a_run2, "A_Run2" } };
static const BswM_ActionItemType itemsNotRun[] = { { a_notrun, "A_NotRun" } };
static const BswM_ActionItemType itemsReq[]    = { { a_req, "A_Req" } };
static const BswM_ActionItemType itemsComT[]   = { { a_comT, "A_ComT" } };
static const BswM_ActionItemType itemsComF[]   = { { a_comF, "A_ComF" } };
static const BswM_ActionListType lRun    = { 2u, itemsRun };
static const BswM_ActionListType lNotRun = { 1u, itemsNotRun };
static const BswM_ActionListType lReq    = { 1u, itemsReq };
static const BswM_ActionListType lComT   = { 1u, itemsComT };
static const BswM_ActionListType lComF   = { 1u, itemsComF };
static const BswM_RuleType rules[3] = {
    { "R_Run",  BSWM_SRC_ECUM_STATE,  0u, ECUM_STATE_RUN, &lRun,  &lNotRun },
    { "R_Req",  BSWM_SRC_REQUEST,     0u, 5u,             &lReq,  NULL_PTR },
    { "R_Com",  BSWM_SRC_COM_SIGNAL,  3u, 1u,             &lComT, &lComF }
};
const BswM_ConfigType BswM_Config = { 3u, rules, 1u };

int main(void)
{
    /* before init: Det errors, nothing runs */
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);
    BswM_RequestMode(0u, 5u);
    BswM_MainFunction();                                 /* silently ignored */
    CHECK_EQ(t_detErrors, 2);
    CHECK_EQ(strlen(order), 0);

    BswM_Init(&BswM_Config);

    /* pushed EcuM state: first evaluation FALSE executes the FALSE list (STARTUP != RUN) */
    BswM_EcuM_CurrentState(ECUM_STATE_STARTUP);
    CHECK(strcmp(order, "n") == 0);
    CHECK(t_traceCount("BSW BSWM RULE R_Run FALSE") == 1);
    CHECK(t_traceCount("BSW BSWM ACTION A_NotRun rc=1") == 1);
    /* same value again: no transition, no action */
    BswM_EcuM_CurrentState(ECUM_STATE_STARTUP);
    CHECK(strcmp(order, "n") == 0);
    /* -> RUN: TRUE list, actions in configured order */
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);
    CHECK(strcmp(order, "nab") == 0);
    CHECK(t_traceCount("BSW BSWM RULE R_Run TRUE") == 1);
    CHECK(t_traceFind("ACTION A_Run1") < t_traceFind("ACTION A_Run2"));
    /* RUN -> POST_RUN -> RUN again: re-executes on each transition */
    BswM_EcuM_CurrentState(ECUM_STATE_POST_RUN);
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);
    CHECK(strcmp(order, "nabnab") == 0);

    /* BswM_RequestMode: only the matching user/source; null falseList is fine */
    order[0] = '\0';
    BswM_RequestMode(0u, 3u);                            /* FALSE first: falseList NULL -> trace only */
    CHECK(strlen(order) == 0);
    CHECK(t_traceCount("RULE R_Req FALSE") == 1);
    BswM_RequestMode(0u, 5u);
    CHECK(strcmp(order, "r") == 0);
    BswM_RequestMode(0u, 5u);
    CHECK(strcmp(order, "r") == 0);
    {
        int before = t_detErrors;
        BswM_RequestMode(1u, 5u);                        /* user out of range */
        CHECK_EQ(t_detErrors, before + 1);
    }

    /* polled Com signal source: skipped while Com says not available, evaluated otherwise */
    order[0] = '\0';
    comRc = COM_SERVICE_NOT_AVAILABLE;
    BswM_MainFunction();
    CHECK(strlen(order) == 0);
    CHECK(t_traceCount("RULE R_Com") == 0);
    comRc = E_OK; comValue = 0u;
    BswM_MainFunction();                                 /* first evaluation FALSE -> falseList */
    CHECK(strcmp(order, "F") == 0);
    BswM_MainFunction();
    CHECK(strcmp(order, "F") == 0);
    comValue = 1u;
    BswM_MainFunction();
    CHECK(strcmp(order, "FT") == 0);
    comValue = 0u;
    BswM_MainFunction();
    CHECK(strcmp(order, "FTF") == 0);

    /* Deinit stops processing; Init resets rule states so everything fires again */
    BswM_Deinit();
    order[0] = '\0';
    BswM_MainFunction();
    CHECK(strlen(order) == 0);
    BswM_Init(&BswM_Config);
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);
    CHECK(strcmp(order, "ab") == 0);

    CHECK_EQ(t_osLockDepth, 0);
    TEST_DONE();
}
