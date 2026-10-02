/*
 * Dcm_Internal.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none standardized. The SWS describes DSL/DSD/DSP
 * interaction with pseudo-names like DslInternal_SetSecurityLevel or
 * DspInternal_DcmConfirmation; real stacks use vendor-specific internal APIs.
 * The names below are this demo's own.
 */
#ifndef DCM_INTERNAL_H
#define DCM_INTERNAL_H

#include "Dcm.h"

#define DCM_SESSION_ROW_INVALID 0xFFu

extern const Dcm_ConfigType *Dcm_CfgPtr;

/* ---------------- DSL (Dcm_Dsl.c) ---------------- */
void Dcm_DslInit(void);
void Dcm_DslMainFunction(void);
Dcm_SesCtrlType Dcm_DslGetSesCtrlType(void);
Dcm_SecLevelType Dcm_DslGetSecurityLevel(void);
void Dcm_DslSetSecurityLevel(Dcm_SecLevelType level);       /* DslInternal_SetSecurityLevel   */
uint8 Dcm_DslFindSessionRow(Dcm_SesCtrlType level);
void Dcm_DslRequestSessionChange(uint8 sessionRowIdx);       /* applied after TX confirmation  */
void Dcm_DslRequestEcuReset(uint8 resetType);                /* executed after TX confirmation */
void Dcm_DslResetToDefaultSession(void);
boolean Dcm_DslResponsePendingWasSent(void);

/* ---------------- DSD (Dcm_Dsd.c) ---------------- */
typedef enum {
    DCM_DSD_RESULT_PENDING = 0,   /* service returned DCM_E_PENDING, call again next cycle */
    DCM_DSD_RESULT_SEND,          /* response (positive or negative) is in the TX buffer    */
    DCM_DSD_RESULT_NO_RESPONSE    /* suppressed (SPRMIB or functional NRC suppression)      */
} Dcm_DsdResultType;

Dcm_DsdResultType Dcm_DsdProcessRequest(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                        uint8 *txBuffer, PduLengthType *txLength);
void Dcm_DsdCancel(Dcm_MsgContextType *pMsgContext);
PduLengthType Dcm_DsdBuildNegativeResponse(uint8 sid, Dcm_NegativeResponseCodeType nrc, uint8 *txBuffer);

/* ---------------- DSP (Dcm_Dsp.c) ---------------- */
void Dcm_DspInit(void);
void Dcm_DspMainFunction(void);
void Dcm_DspSessionChanged(void);

Std_ReturnType Dcm_DspDiagnosticSessionControl(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                               Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspEcuReset(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                               Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspClearDiagnosticInformation(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                                 Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspReadDTCInformation(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                         Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspReadDataByIdentifier(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                           Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspSecurityAccess(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                     Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspWriteDataByIdentifier(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                            Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspRoutineControl(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                     Dcm_NegativeResponseCodeType *ErrorCode);
Std_ReturnType Dcm_DspTesterPresent(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                    Dcm_NegativeResponseCodeType *ErrorCode);

#endif /* DCM_INTERNAL_H */
