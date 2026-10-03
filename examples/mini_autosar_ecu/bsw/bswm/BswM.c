/*
 * BswM.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: BSW Mode Manager (AUTOSAR_CP_SWS_BSWModeManager). Implemented:
 *   BswM_Init              SWS_BswM_00002 (8.3.x)   resets all rule states to UNDEFINED
 *   BswM_Deinit            SWS_BswM_00119 (8.3.8)   "no mode processing shall be performed after BswM_Deinit"
 *   BswM_EcuM_CurrentState (8.3.9)                  push source: EcuM state changes
 *   BswM_RequestMode       SWS_BswM_00046 (8.3.26)  push source: mode request ports of SW-Cs / BSW modules
 *   BswM_MainFunction      SWS_BswM_00053           poll source: Com signals (BswMComSignal mode request)
 * Rule engine = BswMRule: condition "source value == expected" -> BswMActionList. A rule runs its TRUE list on
 * every transition (UNDEFINED/FALSE) -> TRUE and its FALSE list on (UNDEFINED/TRUE) -> FALSE; the first evaluation
 * also counts as a transition (CONDITION execution semantics in the real standard). Every rule evaluation result
 * change and every action is traced:  BSW BSWM RULE <name> TRUE|FALSE   /   BSW BSWM ACTION <name> rc=%u.
 * Not implemented: logical expressions, timers, arbitration, deferred (queued) evaluation, other port kinds.
 *
 * R25-11 NOTE (see EcuM.c too): the standard action BswMRteStart (ECUC_BswM_01073) is what calls Rte_Start();
 * the RTE SWS claims EcuM does. Here the action list AL_Startup of the generated BswM_Cfg.c contains the item
 * "Rte_Start" (a BswMRteStart equivalent) - BswM.c itself stays ignorant of the modules the actions touch.
 *
 * Re-entrancy: actions may call EcuM_RequestRUN etc. but must not call BswM_* APIs (no nested evaluation).
 */
#include "BswM.h"
#include "Com.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_Cfg.h"
#include "Mini_ModuleIds.h"

#define BSWM_API_INIT           0x00u
#define BSWM_API_CURRENT_STATE  0x0Eu
#define BSWM_API_REQUEST_MODE   0x01u

#ifndef BSWM_NUM_RULES
#define BSWM_NUM_RULES 8u
#endif
#ifndef BSWM_NUM_REQUEST_USERS
#define BSWM_NUM_REQUEST_USERS 4u
#endif

static const BswM_ConfigType *bswm_cfg;
static BswM_RuleStateType     bswm_ruleState[BSWM_NUM_RULES];

static void bswm_runList(const BswM_ActionListType *list)
{
    uint8 i;
    if (list == NULL_PTR) { return; }
    for (i = 0u; i < list->numItems; i++) {
        Std_ReturnType rc = list->items[i].fn();
        TRACE(TRACE_CAT_BSW, "BSWM ACTION %s rc=%u", list->items[i].name, (unsigned)rc);
    }
}

/* evaluate rule `idx` against the current source value */
static void bswm_evaluate(uint8 idx, uint32 value)
{
    const BswM_RuleType *r = &bswm_cfg->rules[idx];
    BswM_RuleStateType   ns = (value == r->expected) ? BSWM_RULE_TRUE : BSWM_RULE_FALSE;
    if (ns == bswm_ruleState[idx]) {
        return;                                      /* no transition: nothing to do */
    }
    bswm_ruleState[idx] = ns;
    TRACE(TRACE_CAT_BSW, "BSWM RULE %s %s", r->name, (ns == BSWM_RULE_TRUE) ? "TRUE" : "FALSE");
    bswm_runList((ns == BSWM_RULE_TRUE) ? r->trueList : r->falseList);
}

void BswM_Init(const BswM_ConfigType *ConfigPtr)
{
    uint8 i;
    if (ConfigPtr == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_BSWM, 0u, BSWM_API_INIT, MINI_E_PARAM_POINTER);
        return;
    }
    if (ConfigPtr->numRules > BSWM_NUM_RULES) {
        (void)Det_ReportError(MINI_MODULE_BSWM, 0u, BSWM_API_INIT, MINI_E_PARAM_INVALID);
        return;
    }
    for (i = 0u; i < BSWM_NUM_RULES; i++) { bswm_ruleState[i] = BSWM_RULE_UNDEFINED; }
    bswm_cfg = ConfigPtr;                            /* last: enables the other APIs */
}

void BswM_Deinit(void)
{
    bswm_cfg = NULL_PTR;
}

void BswM_EcuM_CurrentState(EcuM_StateType CurrentState)
{
    uint8 i;
    if (bswm_cfg == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_BSWM, 0u, BSWM_API_CURRENT_STATE, MINI_E_UNINIT);
        return;
    }
    for (i = 0u; i < bswm_cfg->numRules; i++) {
        if (bswm_cfg->rules[i].source == BSWM_SRC_ECUM_STATE) {
            bswm_evaluate(i, (uint32)CurrentState);
        }
    }
}

void BswM_RequestMode(BswM_UserType requestingUser, BswM_ModeType requestedMode)
{
    uint8 i;
    if (bswm_cfg == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_BSWM, 0u, BSWM_API_REQUEST_MODE, MINI_E_UNINIT);
        return;
    }
    if (requestingUser >= bswm_cfg->numRequestUsers) {
        (void)Det_ReportError(MINI_MODULE_BSWM, 0u, BSWM_API_REQUEST_MODE, MINI_E_PARAM_INVALID);
        return;
    }
    /* the requested value is not stored: rules are evaluated right away with the pushed value */
    for (i = 0u; i < bswm_cfg->numRules; i++) {
        if ((bswm_cfg->rules[i].source == BSWM_SRC_REQUEST) && (bswm_cfg->rules[i].arg == (uint16)requestingUser)) {
            bswm_evaluate(i, (uint32)requestedMode);
        }
    }
}

/* Poll sources. Called every 10 ms from Task_BswMain. */
void BswM_MainFunction(void)
{
    uint8 i;
    if (bswm_cfg == NULL_PTR) {
        return;
    }
    for (i = 0u; i < bswm_cfg->numRules; i++) {
        if (bswm_cfg->rules[i].source == BSWM_SRC_COM_SIGNAL) {
            uint8 v = 0u;
            if (Com_ReceiveSignal((Com_SignalIdType)bswm_cfg->rules[i].arg, &v) == E_OK) {
                bswm_evaluate(i, (uint32)v);
            }                                        /* not E_OK (I-PDU group stopped): skip this cycle */
        }
    }
}

void BswM_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo != NULL_PTR) {
        versioninfo->vendorID = MINI_VENDOR_ID;
        versioninfo->moduleID = MINI_MODULE_BSWM;
        versioninfo->sw_major_version = 1u;
        versioninfo->sw_minor_version = 0u;
        versioninfo->sw_patch_version = 0u;
    }
}
