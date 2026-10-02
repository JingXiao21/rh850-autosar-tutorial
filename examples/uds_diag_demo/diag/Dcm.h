/*
 * Dcm.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Dcm (Diagnostic Communication Manager), CP R20-11.
 * Public API names/signatures are taken from DCM SWS R20-11:
 *   Dcm_Init               SWS_Dcm_00037 (p.236)
 *   Dcm_MainFunction       SWS_Dcm_00053 (p.260)
 *   Dcm_StartOfReception   SWS_Dcm_00094 (p.243)
 *   Dcm_CopyRxData         SWS_Dcm_00556 (p.244)
 *   Dcm_TpRxIndication     SWS_Dcm_00093 (p.245)
 *   Dcm_CopyTxData         SWS_Dcm_00092 (p.245)
 *   Dcm_TpTxConfirmation   SWS_Dcm_00351 (p.247)
 *   Dcm_GetSesCtrlType     SWS_Dcm_00339, Dcm_GetSecurityLevel SWS_Dcm_00338
 *   Dcm_ResetToDefaultSession SWS_Dcm_00520
 *
 * Internal split (Dcm_Dsl.c / Dcm_Dsd.c / Dcm_Dsp.c) mirrors the SWS
 * description; the SWS itself says this split is not mandatory (p.50).
 */
#ifndef DCM_H
#define DCM_H

#include "Dcm_Types.h"
#include "Dcm_Cfg.h"

void Dcm_Init(const Dcm_ConfigType *ConfigPtr);
void Dcm_MainFunction(void);

BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr);
void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result);
BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr);
void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result);

Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType *SesCtrlType);
Std_ReturnType Dcm_GetSecurityLevel(Dcm_SecLevelType *SecLevel);
Std_ReturnType Dcm_ResetToDefaultSession(void);

#endif /* DCM_H */
