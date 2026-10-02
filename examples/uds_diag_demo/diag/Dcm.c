/*
 * Dcm.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Dcm module entry points (Dcm_Init, Dcm_MainFunction,
 * Dcm_Get* service APIs). The TP callbacks from PduR live in Dcm_Dsl.c because
 * buffer handling and protocol timing are DSL responsibilities.
 */
#include "Dcm_Internal.h"
#include "Det.h"
#include "UdsTrace.h"

const Dcm_ConfigType *Dcm_CfgPtr = NULL_PTR;

/* [AUTOSAR API] SWS_Dcm_00037 */
void Dcm_Init(const Dcm_ConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        (void)Det_ReportError(DET_MODULE_ID_DCM, 0u, 0x01u, 0x08u /* DCM_E_INIT_FAILED */);
        return;
    }
    Dcm_CfgPtr = ConfigPtr;
    Dcm_DslInit();
    Dcm_DspInit();
    UDS_TRACE("Dcm", "Init: %u services, %u DIDs, %u routines, %u security rows; DefaultSession, LOCKED",
              (unsigned)ConfigPtr->numServices, (unsigned)ConfigPtr->numDids,
              (unsigned)ConfigPtr->numRoutines, (unsigned)ConfigPtr->numSecurityRows);
}

/* [AUTOSAR API] SWS_Dcm_00053: called by the BSW scheduler every DcmTaskTime.
 * Everything time based (P2, P2* and S3, security delay, pending re-calls) is
 * counted in units of this call, so its period limits the timing accuracy. */
void Dcm_MainFunction(void)
{
    if (Dcm_CfgPtr == NULL_PTR) {
        return;
    }
    Dcm_DslMainFunction();
    Dcm_DspMainFunction();
}

Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType *SesCtrlType)
{
    if ((Dcm_CfgPtr == NULL_PTR) || (SesCtrlType == NULL_PTR)) {
        return E_NOT_OK;
    }
    *SesCtrlType = Dcm_DslGetSesCtrlType();
    return E_OK;
}

Std_ReturnType Dcm_GetSecurityLevel(Dcm_SecLevelType *SecLevel)
{
    if ((Dcm_CfgPtr == NULL_PTR) || (SecLevel == NULL_PTR)) {
        return E_NOT_OK;
    }
    *SecLevel = Dcm_DslGetSecurityLevel();
    return E_OK;
}

Std_ReturnType Dcm_ResetToDefaultSession(void)
{
    if (Dcm_CfgPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    Dcm_DslResetToDefaultSession();
    return E_OK;
}
