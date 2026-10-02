/*
 * Dcm_Dsd.c - Diagnostic Service Dispatcher
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the DSD part of Dcm (DCM SWS R20-11 §7.4, p.88-103).
 * "How does DCM know that 0x22 is ReadDataByIdentifier?" -> it does not know
 * anything about 0x22: it looks the SID up in the configured service table
 * (Dcm_Cfg.c, DcmDsdServiceTable) and calls the function configured there.
 *
 * Check order implemented (SWS_Dcm_01535 p.94 without manufacturer/supplier
 * notification, authentication and mode rules):
 *   1. SID in service table                    else NRC 0x11 (SWS_Dcm_00197)
 *   2. SID allowed in active session           else NRC 0x7F (SWS_Dcm_00211)
 *   3. SID allowed at active security level    else NRC 0x33 (SWS_Dcm_00217)
 *   4. minimum length                          else NRC 0x13 (SWS_Dcm_00696)
 *   5. sub-function: SPRMIB bit evaluated and removed (SWS_Dcm_00204),
 *      sub-function configured                 else NRC 0x12 (SWS_Dcm_00273, not for 0x31)
 *      sub-function allowed in session         else NRC 0x7E (SWS_Dcm_00616)
 *      sub-function allowed at security level  else NRC 0x33 (SWS_Dcm_00617)
 *   6. dispatch to DSP; assemble positive / negative response
 *   7. SPRMIB: suppress positive response (SWS_Dcm_00200) unless 0x78 was sent
 *   8. functional request: suppress NRC 0x11/0x12/0x31/0x7E/0x7F (SWS_Dcm_00001)
 * Real stacks differ in where 0x13 sits relative to 0x7F/0x33: see research
 * note 02 §3.7 - verify with tests when upgrading a Dcm.
 */
#include "Dcm_Internal.h"
#include "Det.h"
#include "UdsTrace.h"

static const Dcm_DsdServiceType *Dcm_DsdActiveService;

PduLengthType Dcm_DsdBuildNegativeResponse(uint8 sid, Dcm_NegativeResponseCodeType nrc, uint8 *txBuffer)
{
    txBuffer[0] = 0x7Fu;
    txBuffer[1] = sid;
    txBuffer[2] = nrc;
    return 3u;
}

static boolean Dcm_DsdNrcSuppressedForFunctional(Dcm_NegativeResponseCodeType nrc)
{
    return (boolean)((nrc == DCM_E_SERVICENOTSUPPORTED) || (nrc == DCM_E_SUBFUNCTIONNOTSUPPORTED) ||
                     (nrc == DCM_E_REQUESTOUTOFRANGE) ||
                     (nrc == DCM_E_SUBFUNCTIONNOTSUPPORTEDINACTIVESESSION) ||
                     (nrc == DCM_E_SERVICENOTSUPPORTEDINACTIVESESSION));
}

static Dcm_DsdResultType Dcm_DsdReject(const Dcm_MsgContextType *ctx, Dcm_NegativeResponseCodeType nrc,
                                       uint8 *txBuffer, PduLengthType *txLength, const char *why)
{
    uint8 sid = (uint8)ctx->idContext;

    Dcm_DsdActiveService = NULL_PTR;
    if ((ctx->msgAddInfo.reqType == DCM_FUNCTIONAL_REQUEST) && Dcm_DsdNrcSuppressedForFunctional(nrc) &&
        !Dcm_DslResponsePendingWasSent()) {
        UDS_TRACE("Dcm/DSD", "NRC 0x%02X (%s) suppressed: functional request (SWS_Dcm_00001)", (unsigned)nrc, why);
        return DCM_DSD_RESULT_NO_RESPONSE;
    }
    UDS_TRACE("Dcm/DSD", "negative response 7F %02X %02X (%s)", (unsigned)sid, (unsigned)nrc, why);
    *txLength = Dcm_DsdBuildNegativeResponse(sid, nrc, txBuffer);
    return DCM_DSD_RESULT_SEND;
}

static const Dcm_DsdServiceType *Dcm_DsdFindService(uint8 sid)
{
    uint8 i;
    for (i = 0u; i < Dcm_CfgPtr->numServices; i++) {
        if (Dcm_CfgPtr->services[i].sid == sid) {
            return &Dcm_CfgPtr->services[i];
        }
    }
    return NULL_PTR;
}

/* Returns TRUE if the request may be dispatched; otherwise *reject holds the
 * result (negative response prepared or suppressed). */
static boolean Dcm_DsdCheckRequest(Dcm_MsgContextType *ctx, uint8 *txBuffer, PduLengthType *txLength,
                                   Dcm_DsdResultType *reject)
{
    uint8 sid = (uint8)ctx->idContext;
    uint32 sesBit = DCM_SES_MASK(Dcm_DslGetSesCtrlType());
    uint32 secBit = DCM_SEC_MASK(Dcm_DslGetSecurityLevel());
    const Dcm_DsdServiceType *svc = Dcm_DsdFindService(sid);

    UDS_TRACE("Dcm/DSD", "SID 0x%02X: lookup in DcmDsdServiceTable -> %s", (unsigned)sid,
              (svc != NULL_PTR) ? svc->name : "not configured");
    if (svc == NULL_PTR) {
        *reject = Dcm_DsdReject(ctx, DCM_E_SERVICENOTSUPPORTED, txBuffer, txLength, "SID not in service table");
        return FALSE;
    }
    if ((svc->sessionMask & sesBit) == 0u) {
        *reject = Dcm_DsdReject(ctx, DCM_E_SERVICENOTSUPPORTEDINACTIVESESSION, txBuffer, txLength,
                             "service not allowed in active session");
        return FALSE;
    }
    if ((svc->securityMask & secBit) == 0u) {
        *reject = Dcm_DsdReject(ctx, DCM_E_SECURITYACCESSDENIED, txBuffer, txLength,
                             "service needs a higher security level");
        return FALSE;
    }
    if ((ctx->reqDataLen + 1u) < svc->minReqLen) {
        *reject = Dcm_DsdReject(ctx, DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT, txBuffer, txLength,
                             "request shorter than minimum length");
        return FALSE;
    }
    if (svc->subFuncAvail) {
        uint8 sf = ctx->reqData[0];
        if ((sf & 0x80u) != 0u) {
            ctx->msgAddInfo.suppressPosResponse = 1u;
            ctx->reqData[0] = (uint8)(sf & 0x7Fu);
            UDS_TRACE("Dcm/DSD", "suppressPosRspMsgIndicationBit set (sub-function 0x%02X)", (unsigned)(sf & 0x7Fu));
        }
        if (svc->subServices != NULL_PTR) {
            uint8 i;
            const Dcm_DsdSubServiceType *sub = NULL_PTR;
            for (i = 0u; i < svc->numSubServices; i++) {
                if (svc->subServices[i].subFunctionId == ctx->reqData[0]) {
                    sub = &svc->subServices[i];
                    break;
                }
            }
            if (sub == NULL_PTR) {
                *reject = Dcm_DsdReject(ctx, DCM_E_SUBFUNCTIONNOTSUPPORTED, txBuffer, txLength,
                                     "sub-function not configured");
                return FALSE;
            }
            if ((sub->sessionMask & sesBit) == 0u) {
                *reject = Dcm_DsdReject(ctx, DCM_E_SUBFUNCTIONNOTSUPPORTEDINACTIVESESSION, txBuffer, txLength,
                                     "sub-function not allowed in active session");
                return FALSE;
            }
            if ((sub->securityMask & secBit) == 0u) {
                *reject = Dcm_DsdReject(ctx, DCM_E_SECURITYACCESSDENIED, txBuffer, txLength,
                                     "sub-function needs a higher security level");
                return FALSE;
            }
        }
    }
    Dcm_DsdActiveService = svc;
    UDS_TRACE("Dcm/DSD", "checks passed (session/security/length/sub-function) -> dispatch to DSP %s", svc->name);
    return TRUE;
}

Dcm_DsdResultType Dcm_DsdProcessRequest(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                        uint8 *txBuffer, PduLengthType *txLength)
{
    Dcm_NegativeResponseCodeType nrc = DCM_POS_RESP;
    Std_ReturnType ret;
    uint8 sid = (uint8)pMsgContext->idContext;

    if (OpStatus == DCM_INITIAL) {
        Dcm_DsdResultType reject = DCM_DSD_RESULT_NO_RESPONSE;
        Dcm_DsdActiveService = NULL_PTR;
        if (!Dcm_DsdCheckRequest(pMsgContext, txBuffer, txLength, &reject)) {
            return reject;
        }
        pMsgContext->resDataLen = 0u;
    }
    if (Dcm_DsdActiveService == NULL_PTR) {
        return DCM_DSD_RESULT_NO_RESPONSE;
    }

    ret = Dcm_DsdActiveService->fnc(OpStatus, pMsgContext, &nrc);
    if (ret == DCM_E_PENDING) {
        UDS_TRACE("Dcm/DSD", "%s returned DCM_E_PENDING -> call again with DCM_PENDING next cycle",
                  Dcm_DsdActiveService->name);
        return DCM_DSD_RESULT_PENDING;
    }
    if (ret != E_OK) {
        if ((nrc == DCM_POS_RESP) || (ret != E_NOT_OK)) {
            nrc = DCM_E_GENERALREJECT;          /* SWS_Dcm_00271 default NRC */
        }
        return Dcm_DsdReject(pMsgContext, nrc, txBuffer, txLength, Dcm_DsdActiveService->name);
    }

    Dcm_DsdActiveService = NULL_PTR;
    if ((pMsgContext->msgAddInfo.suppressPosResponse != 0u) && !Dcm_DslResponsePendingWasSent()) {
        UDS_TRACE("Dcm/DSD", "positive response suppressed (SPRMIB, SWS_Dcm_00200)");
        return DCM_DSD_RESULT_NO_RESPONSE;
    }
    txBuffer[0] = (uint8)(sid + 0x40u);         /* response SID; resData already in txBuffer[1..] */
    *txLength = (PduLengthType)(1u + pMsgContext->resDataLen);
    UDS_TRACE("Dcm/DSD", "positive response assembled: SID 0x%02X + %u data bytes",
              (unsigned)txBuffer[0], (unsigned)pMsgContext->resDataLen);
    return DCM_DSD_RESULT_SEND;
}

void Dcm_DsdCancel(Dcm_MsgContextType *pMsgContext)
{
    Dcm_NegativeResponseCodeType nrc = DCM_POS_RESP;
    if (Dcm_DsdActiveService != NULL_PTR) {
        (void)Dcm_DsdActiveService->fnc(DCM_CANCEL, pMsgContext, &nrc);   /* return value ignored (SWS_Dcm_01046) */
        Dcm_DsdActiveService = NULL_PTR;
    }
}
