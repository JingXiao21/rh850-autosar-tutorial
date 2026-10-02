/*
 * Dcm_Dsl.c - Diagnostic Session Layer
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the DSL part of Dcm (DCM SWS R20-11 §7.3, p.53-88).
 * Responsibilities implemented here:
 *   - TP buffer handshake with PduR (StartOfReception/CopyRxData/TpRxIndication,
 *     CopyTxData/TpTxConfirmation) and ownership of the RX/TX buffers
 *   - one protocol (UDS on CAN), one connection, physical + functional RX
 *   - P2 / P2* supervision and NRC 0x78 (SWS_Dcm_00024, separate buffer 00119)
 *   - S3 timer (SWS_Dcm_00140/00141) -> fall back to the default session
 *   - session + security state (SWS_Dcm_00022/00020/00139)
 *   - session change and ECU reset AFTER the positive response was sent
 *     (SWS_Dcm_00311, SWS_Dcm_00594)
 *   - functional "3E 80" handled without DSD (SWS_Dcm_00112/00113)
 * Not implemented: protocol preemption, multiple connections, paged buffer,
 * ComM interaction (ComM_DCM_ActiveDiagnostic), ROE, periodic transmission,
 * authentication (0x29), DCM_E_FORCE_RCRRP.
 *
 * State machine of one request:
 *
 *   IDLE --StartOfReception--> RECEIVING --TpRxIndication(OK)--> REQ_RECEIVED
 *     ^                                                              |
 *     |                                      Dcm_MainFunction: DSD(INITIAL)
 *     |                                                              v
 *     +--TpTxConfirmation-- TRANSMITTING <--response ready-- PROCESSING (DSD(PENDING) each cycle,
 *                                                                      P2 expiry -> NRC 0x78)
 */
#include "Dcm_Internal.h"
#include "PduR.h"
#include "SchM_Dcm.h"
#include "Det.h"
#include "UdsTrace.h"
#include <string.h>

typedef enum {
    DCM_DSL_IDLE = 0,
    DCM_DSL_RECEIVING,
    DCM_DSL_REQ_RECEIVED,
    DCM_DSL_PROCESSING,
    DCM_DSL_TRANSMITTING
} Dcm_DslStateType;

typedef struct {
    Dcm_DslStateType   state;
    PduIdType          rxPduId;
    PduLengthType      rxLen;
    PduLengthType      rxCopied;
    PduLengthType      txLen;
    PduLengthType      txCopied;
    /* concurrent functional TesterPresent while busy (SWS_Dcm_00557) */
    boolean            tpActive;
    PduLengthType      tpCopied;
    /* NRC 0x78 in its own buffer (SWS_Dcm_00119) */
    boolean            rcrrpInFlight;
    PduLengthType      rcrrpCopied;
    boolean            rcrrpSentForRequest;
    uint8              respPendCount;
    boolean            finalWaiting;      /* final response ready while 0x78 still sending */
    PduLengthType      finalLen;
    sint32             p2TimerMs;
    boolean            s3Running;
    sint32             s3TimerMs;
    uint8              sessionRow;
    Dcm_SecLevelType   secLevel;
    uint8              pendingSessionRow; /* DCM_SESSION_ROW_INVALID = none */
    boolean            resetPending;
    uint8              resetType;
    Dcm_MsgContextType msgContext;
} Dcm_DslRuntimeType;

static uint8 Dcm_DslRxBuffer[DCM_DSL_BUFFER_SIZE];
static uint8 Dcm_DslTxBuffer[DCM_DSL_BUFFER_SIZE];
static uint8 Dcm_DslRcrrpBuffer[3];
static uint8 Dcm_DslTpBuffer[2];
static Dcm_DslRuntimeType Dcm_Dsl;

/* ------------------------------------------------------------------ */
/* session / security                                                  */
/* ------------------------------------------------------------------ */

static const Dcm_DspSessionRowType *Dcm_DslRow(void)
{
    return &Dcm_CfgPtr->sessionRows[Dcm_Dsl.sessionRow];
}

uint8 Dcm_DslFindSessionRow(Dcm_SesCtrlType level)
{
    uint8 i;
    for (i = 0u; i < Dcm_CfgPtr->numSessionRows; i++) {
        if (Dcm_CfgPtr->sessionRows[i].level == level) {
            return i;
        }
    }
    return DCM_SESSION_ROW_INVALID;
}

Dcm_SesCtrlType Dcm_DslGetSesCtrlType(void) { return Dcm_DslRow()->level; }
Dcm_SecLevelType Dcm_DslGetSecurityLevel(void) { return Dcm_Dsl.secLevel; }
boolean Dcm_DslResponsePendingWasSent(void) { return Dcm_Dsl.rcrrpSentForRequest; }

void Dcm_DslSetSecurityLevel(Dcm_SecLevelType level)
{
    UDS_TRACE("Dcm/DSL", "security level %u -> %u", (unsigned)Dcm_Dsl.secLevel, (unsigned)level);
    Dcm_Dsl.secLevel = level;
}

static void Dcm_DslSetSession(uint8 rowIdx, const char *reason)
{
    const Dcm_DspSessionRowType *oldRow = Dcm_DslRow();
    const Dcm_DspSessionRowType *newRow = &Dcm_CfgPtr->sessionRows[rowIdx];

    Dcm_Dsl.sessionRow = rowIdx;
    /* SWS_Dcm_00139: any session transition (except default->default) locks security. */
    Dcm_Dsl.secLevel = DCM_SEC_LEV_LOCKED;
    Dcm_DspSessionChanged();
    UDS_TRACE("Dcm/DSL", "session %s -> %s (%s); security LOCKED; P2=%u ms P2*=%u ms",
              oldRow->name, newRow->name, reason, (unsigned)newRow->p2ServerMaxMs,
              (unsigned)newRow->p2StarServerMaxMs);
    /* Mode switch so BswM / SW-Cs can react (SWS_Dcm_00311). */
    SchM_Switch_Dcm_DcmDiagnosticSessionControl(newRow->level);
}

void Dcm_DslRequestSessionChange(uint8 sessionRowIdx)
{
    Dcm_Dsl.pendingSessionRow = sessionRowIdx;
}

void Dcm_DslRequestEcuReset(uint8 resetType)
{
    Dcm_Dsl.resetPending = TRUE;
    Dcm_Dsl.resetType = resetType;
}

void Dcm_DslResetToDefaultSession(void)
{
    uint8 def = Dcm_DslFindSessionRow(DCM_DEFAULT_SESSION);
    if (def != DCM_SESSION_ROW_INVALID) {
        Dcm_DslSetSession(def, "Dcm_ResetToDefaultSession");
    }
}

/* ------------------------------------------------------------------ */
/* init                                                                */
/* ------------------------------------------------------------------ */

void Dcm_DslInit(void)
{
    (void)memset(&Dcm_Dsl, 0, sizeof(Dcm_Dsl));
    Dcm_Dsl.state = DCM_DSL_IDLE;
    Dcm_Dsl.sessionRow = Dcm_DslFindSessionRow(DCM_DEFAULT_SESSION);   /* SWS_Dcm_00034 */
    if (Dcm_Dsl.sessionRow == DCM_SESSION_ROW_INVALID) {
        Dcm_Dsl.sessionRow = 0u;
    }
    Dcm_Dsl.secLevel = DCM_SEC_LEV_LOCKED;                                /* SWS_Dcm_00033 */
    Dcm_Dsl.pendingSessionRow = DCM_SESSION_ROW_INVALID;
}

/* ------------------------------------------------------------------ */
/* request finished / transmit helpers                                 */
/* ------------------------------------------------------------------ */

static void Dcm_DslFinishRequest(boolean responseOk)
{
    Dcm_Dsl.state = DCM_DSL_IDLE;
    if (Dcm_Dsl.pendingSessionRow != DCM_SESSION_ROW_INVALID) {
        uint8 row = Dcm_Dsl.pendingSessionRow;
        Dcm_Dsl.pendingSessionRow = DCM_SESSION_ROW_INVALID;
        if (responseOk) {
            Dcm_DslSetSession(row, "0x10 response confirmed");
        }
    }
    if (Dcm_Dsl.resetPending) {
        Dcm_Dsl.resetPending = FALSE;
        if (responseOk) {
            /* SWS_Dcm_00594: only after the positive response is on the bus. */
            UDS_TRACE("Dcm/DSL", "0x11 response confirmed -> SchM_Switch DcmEcuReset = EXECUTE");
            SchM_Switch_Dcm_DcmEcuReset(RTE_MODE_DcmEcuReset_EXECUTE);
        }
    }
    /* SWS_Dcm_00141: (re)start S3 when the response was sent / not needed. */
    Dcm_Dsl.s3Running = TRUE;
    Dcm_Dsl.s3TimerMs = (sint32)DCM_S3_SERVER_MS;
    UDS_TRACE("Dcm/DSL", "request finished; S3 timer (re)started (%u ms)", (unsigned)DCM_S3_SERVER_MS);
}

static void Dcm_DslTransmitFinal(PduLengthType length)
{
    PduInfoType info;

    Dcm_Dsl.txLen = length;
    Dcm_Dsl.txCopied = 0u;
    Dcm_Dsl.state = DCM_DSL_TRANSMITTING;
    info.SduDataPtr = NULL_PTR;    /* TP API: data is pulled later via Dcm_CopyTxData */
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = length;
    UDS_TRACE("Dcm/DSL", "response [%s] -> PduR_DcmTransmit(%u, len=%u)",
              UdsTrace_Hex(Dcm_DslTxBuffer, length), (unsigned)PduRConf_PduRSrcPdu_Dcm_DiagResp, (unsigned)length);
    if (PduR_DcmTransmit(PduRConf_PduRSrcPdu_Dcm_DiagResp, &info) != E_OK) {
        /* SWS_Dcm_00118: no retry of the response */
        UDS_TRACE("Dcm/DSL", "PduR_DcmTransmit failed -> response dropped");
        Dcm_DslFinishRequest(FALSE);
    }
}

static void Dcm_DslSendResponsePending(void)
{
    PduInfoType info;

    Dcm_DslRcrrpBuffer[0] = 0x7Fu;
    Dcm_DslRcrrpBuffer[1] = (uint8)Dcm_Dsl.msgContext.idContext;
    Dcm_DslRcrrpBuffer[2] = DCM_INTERNAL_NRC_RESPONSEPENDING;
    Dcm_Dsl.rcrrpInFlight = TRUE;
    Dcm_Dsl.rcrrpCopied = 0u;
    Dcm_Dsl.rcrrpSentForRequest = TRUE;
    info.SduDataPtr = NULL_PTR;
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = 3u;
    UDS_TRACE("Dcm/DSL", "P2 expired while service pending -> NRC 0x78 [7F %02X 78] (#%u) from separate buffer",
              (unsigned)Dcm_DslRcrrpBuffer[1], (unsigned)(Dcm_Dsl.respPendCount + 1u));
    if (PduR_DcmTransmit(PduRConf_PduRSrcPdu_Dcm_DiagResp, &info) != E_OK) {
        Dcm_Dsl.rcrrpInFlight = FALSE;
    }
}

static void Dcm_DslHandleDsdResult(Dcm_DsdResultType res, PduLengthType txLen)
{
    switch (res) {
    case DCM_DSD_RESULT_SEND:
        if (Dcm_Dsl.rcrrpInFlight) {
            Dcm_Dsl.finalWaiting = TRUE;    /* send after the 0x78 is confirmed */
            Dcm_Dsl.finalLen = txLen;
        } else {
            Dcm_DslTransmitFinal(txLen);
        }
        break;
    case DCM_DSD_RESULT_NO_RESPONSE:
        Dcm_DslFinishRequest(TRUE);
        break;
    default:
        break;   /* pending: keep PROCESSING */
    }
}

/* ------------------------------------------------------------------ */
/* main function                                                       */
/* ------------------------------------------------------------------ */

void Dcm_DslMainFunction(void)
{
    PduLengthType txLen = 0u;
    Dcm_DsdResultType res;

    if (Dcm_Dsl.state == DCM_DSL_REQ_RECEIVED) {
        Dcm_Dsl.state = DCM_DSL_PROCESSING;
        res = Dcm_DsdProcessRequest(DCM_INITIAL, &Dcm_Dsl.msgContext, Dcm_DslTxBuffer, &txLen);
        Dcm_DslHandleDsdResult(res, txLen);
    } else if ((Dcm_Dsl.state == DCM_DSL_PROCESSING) && !Dcm_Dsl.finalWaiting) {
        /* SWS_Dcm_00530: re-call the pending operation with DCM_PENDING. */
        res = Dcm_DsdProcessRequest(DCM_PENDING, &Dcm_Dsl.msgContext, Dcm_DslTxBuffer, &txLen);
        Dcm_DslHandleDsdResult(res, txLen);
    } else {
        /* nothing to process */
    }

    /* P2 / P2* supervision while the service is still running. */
    if ((Dcm_Dsl.state == DCM_DSL_PROCESSING) && !Dcm_Dsl.finalWaiting) {
        Dcm_Dsl.p2TimerMs -= (sint32)DCM_TASK_TIME_MS;
        if (Dcm_Dsl.p2TimerMs <= 0) {
            if (Dcm_Dsl.respPendCount < DCM_DSL_MAX_NUM_RESP_PEND) {
                if (!Dcm_Dsl.rcrrpInFlight) {
                    Dcm_DslSendResponsePending();
                }
                Dcm_Dsl.respPendCount++;
                /* after 0x78 the tester waits P2*ServerMax (SWS_Dcm_00024) */
                Dcm_Dsl.p2TimerMs = (sint32)Dcm_DslRow()->p2StarServerMaxMs - (sint32)DCM_TIM_P2STAR_SERVER_ADJUST_MS;
            } else {
                /* SWS_Dcm_00120: give up -> cancel the operation, NRC 0x10 */
                UDS_TRACE("Dcm/DSL", "DcmDslDiagRespMaxNumRespPend reached -> cancel service, NRC 0x10");
                Dcm_DsdCancel(&Dcm_Dsl.msgContext);
                (void)Det_ReportRuntimeError(DET_MODULE_ID_DCM, 0u, 0x25u, 0x01u /* DCM_E_INTERFACE_TIMEOUT */);
                txLen = Dcm_DsdBuildNegativeResponse((uint8)Dcm_Dsl.msgContext.idContext, DCM_E_GENERALREJECT,
                                                     Dcm_DslTxBuffer);
                Dcm_DslHandleDsdResult(DCM_DSD_RESULT_SEND, txLen);
            }
        }
    }

    /* S3: only meaningful outside the default session (SWS_Dcm_00140). */
    if ((Dcm_Dsl.state == DCM_DSL_IDLE) && Dcm_Dsl.s3Running) {
        Dcm_Dsl.s3TimerMs -= (sint32)DCM_TASK_TIME_MS;
        if (Dcm_Dsl.s3TimerMs <= 0) {
            Dcm_Dsl.s3Running = FALSE;
            if (Dcm_DslGetSesCtrlType() != DCM_DEFAULT_SESSION) {
                uint8 def = Dcm_DslFindSessionRow(DCM_DEFAULT_SESSION);
                UDS_TRACE("Dcm/DSL", "S3 timeout (%u ms without request) -> back to default session",
                          (unsigned)DCM_S3_SERVER_MS);
                Dcm_DslSetSession(def, "S3 timeout");
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* PduR -> Dcm TP interface (may run in interrupt context)             */
/* ------------------------------------------------------------------ */

/* [AUTOSAR API] SWS_Dcm_00094 */
BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info,
                                       PduLengthType TpSduLength, PduLengthType *bufferSizePtr)
{
    (void)info;   /* SF payload is delivered again with CopyRxData */
    if (Dcm_CfgPtr == NULL_PTR) {
        (void)Det_ReportError(DET_MODULE_ID_DCM, 0u, 0x46u, 0x05u /* DCM_E_UNINIT */);
        return BUFREQ_E_NOT_OK;
    }
    if ((bufferSizePtr == NULL_PTR) || (id > DcmConf_DcmDslProtocolRx_DiagFunc)) {
        (void)Det_ReportError(DET_MODULE_ID_DCM, 0u, 0x46u, 0x06u /* DCM_E_PARAM */);
        return BUFREQ_E_NOT_OK;
    }
    if (TpSduLength == 0u) {
        return BUFREQ_E_NOT_OK;                                   /* SWS_Dcm_00642 */
    }
    if (Dcm_Dsl.state != DCM_DSL_IDLE) {
        if ((id == DcmConf_DcmDslProtocolRx_DiagFunc) && (TpSduLength == 2u) && !Dcm_Dsl.tpActive) {
            Dcm_Dsl.tpActive = TRUE;                             /* SWS_Dcm_00557 exception */
            Dcm_Dsl.tpCopied = 0u;
            *bufferSizePtr = (PduLengthType)sizeof(Dcm_DslTpBuffer);
            return BUFREQ_OK;
        }
        UDS_TRACE("Dcm/DSL", "StartOfReception(%u) while busy -> BUFREQ_E_NOT_OK", (unsigned)id);
        return BUFREQ_E_NOT_OK;
    }
    if (TpSduLength > DCM_DSL_BUFFER_SIZE) {
        UDS_TRACE("Dcm/DSL", "StartOfReception len=%u > buffer %u -> BUFREQ_E_OVFL",
                  (unsigned)TpSduLength, (unsigned)DCM_DSL_BUFFER_SIZE);
        return BUFREQ_E_OVFL;                                     /* SWS_Dcm_00444 */
    }
    Dcm_Dsl.state = DCM_DSL_RECEIVING;
    Dcm_Dsl.rxPduId = id;
    Dcm_Dsl.rxLen = TpSduLength;
    Dcm_Dsl.rxCopied = 0u;
    Dcm_Dsl.s3Running = FALSE;                                    /* SWS_Dcm_00141: stop S3 */
    *bufferSizePtr = DCM_DSL_BUFFER_SIZE;
    UDS_TRACE("Dcm/DSL", "StartOfReception(DcmRxPduId %u %s, len=%u) -> BUFREQ_OK, buffer=%u, S3 stopped",
              (unsigned)id, (id == DcmConf_DcmDslProtocolRx_DiagFunc) ? "functional" : "physical",
              (unsigned)TpSduLength, (unsigned)DCM_DSL_BUFFER_SIZE);
    return BUFREQ_OK;
}

/* [AUTOSAR API] SWS_Dcm_00556 */
BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr)
{
    uint8 *dst;
    PduLengthType *copied;
    PduLengthType size;

    if ((info == NULL_PTR) || (bufferSizePtr == NULL_PTR)) {
        return BUFREQ_E_NOT_OK;
    }
    if ((Dcm_Dsl.state == DCM_DSL_RECEIVING) && (id == Dcm_Dsl.rxPduId)) {
        dst = Dcm_DslRxBuffer;
        copied = &Dcm_Dsl.rxCopied;
        size = DCM_DSL_BUFFER_SIZE;
    } else if (Dcm_Dsl.tpActive && (id == DcmConf_DcmDslProtocolRx_DiagFunc)) {
        dst = Dcm_DslTpBuffer;
        copied = &Dcm_Dsl.tpCopied;
        size = (PduLengthType)sizeof(Dcm_DslTpBuffer);
    } else {
        return BUFREQ_E_NOT_OK;
    }
    if (info->SduLength > (PduLengthType)(size - *copied)) {
        return BUFREQ_E_NOT_OK;
    }
    if (info->SduLength != 0u) {        /* SduLength 0 = "how much buffer is left?" (SWS_Dcm_00996) */
        (void)memcpy(&dst[*copied], info->SduDataPtr, info->SduLength);
        *copied = (PduLengthType)(*copied + info->SduLength);
    }
    *bufferSizePtr = (PduLengthType)(size - *copied);
    return BUFREQ_OK;
}

/* [AUTOSAR API] SWS_Dcm_00093 */
void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result)
{
    Dcm_MsgContextType *ctx = &Dcm_Dsl.msgContext;
    const Dcm_DspSessionRowType *row;

    if (Dcm_Dsl.tpActive && (id == DcmConf_DcmDslProtocolRx_DiagFunc) &&
        !((Dcm_Dsl.state == DCM_DSL_RECEIVING) && (Dcm_Dsl.rxPduId == id))) {
        Dcm_Dsl.tpActive = FALSE;
        if ((result == E_OK) && (Dcm_DslTpBuffer[0] == 0x3Eu) && (Dcm_DslTpBuffer[1] == 0x80u)) {
            UDS_TRACE("Dcm/DSL", "concurrent functional 3E 80 while busy: accepted, not processed (SWS_Dcm_00557)");
        }
        return;
    }
    if ((Dcm_Dsl.state != DCM_DSL_RECEIVING) || (id != Dcm_Dsl.rxPduId)) {
        return;
    }
    if (result != E_OK) {
        /* SWS_Dcm_00344: buffer content is not evaluated; S3 restarts. */
        UDS_TRACE("Dcm/DSL", "TpRxIndication(E_NOT_OK): reception failed, request discarded");
        Dcm_Dsl.state = DCM_DSL_IDLE;
        Dcm_Dsl.s3Running = TRUE;
        Dcm_Dsl.s3TimerMs = (sint32)DCM_S3_SERVER_MS;
        return;
    }
    if ((id == DcmConf_DcmDslProtocolRx_DiagFunc) && (Dcm_Dsl.rxLen == 2u) &&
        (Dcm_DslRxBuffer[0] == 0x3Eu) && (Dcm_DslRxBuffer[1] == 0x80u)) {
        /* SWS_Dcm_00112/00113: functional TesterPresent with SPRMIB is handled
         * by the DSL alone: just keep the session alive. */
        UDS_TRACE("Dcm/DSL", "functional 3E 80: S3 restarted, request NOT passed to DSD, no response");
        Dcm_Dsl.state = DCM_DSL_IDLE;
        Dcm_Dsl.s3Running = TRUE;
        Dcm_Dsl.s3TimerMs = (sint32)DCM_S3_SERVER_MS;
        return;
    }

    ctx->idContext = Dcm_DslRxBuffer[0];                 /* SID */
    ctx->reqData = &Dcm_DslRxBuffer[1];
    ctx->reqDataLen = (Dcm_MsgLenType)(Dcm_Dsl.rxLen - 1u);
    ctx->resData = &Dcm_DslTxBuffer[1];
    ctx->resDataLen = 0u;
    ctx->resMaxDataLen = (Dcm_MsgLenType)(DCM_DSL_BUFFER_SIZE - 1u);
    ctx->msgAddInfo.reqType = (id == DcmConf_DcmDslProtocolRx_DiagFunc) ? DCM_FUNCTIONAL_REQUEST : DCM_PHYSICAL_REQUEST;
    ctx->msgAddInfo.suppressPosResponse = 0u;
    ctx->dcmRxPduId = id;

    row = Dcm_DslRow();
    Dcm_Dsl.respPendCount = 0u;
    Dcm_Dsl.rcrrpSentForRequest = FALSE;
    Dcm_Dsl.finalWaiting = FALSE;
    Dcm_Dsl.p2TimerMs = (sint32)row->p2ServerMaxMs - (sint32)DCM_TIM_P2_SERVER_ADJUST_MS;
    Dcm_Dsl.state = DCM_DSL_REQ_RECEIVED;
    UDS_TRACE("Dcm/DSL", "TpRxIndication(E_OK): request [%s] complete; P2 timer = %u-%u ms; DSD runs in next Dcm_MainFunction",
              UdsTrace_Hex(Dcm_DslRxBuffer, Dcm_Dsl.rxLen), (unsigned)row->p2ServerMaxMs,
              (unsigned)DCM_TIM_P2_SERVER_ADJUST_MS);
}

/* [AUTOSAR API] SWS_Dcm_00092: CanTp pulls the response piece by piece. */
BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr)
{
    const uint8 *src;
    PduLengthType total;
    PduLengthType *copied;

    if ((id != DcmConf_DcmDslProtocolTx_DiagResp) || (info == NULL_PTR) || (availableDataPtr == NULL_PTR)) {
        return BUFREQ_E_NOT_OK;
    }
    if (Dcm_Dsl.rcrrpInFlight) {
        src = Dcm_DslRcrrpBuffer;
        total = 3u;
        copied = &Dcm_Dsl.rcrrpCopied;
    } else if (Dcm_Dsl.state == DCM_DSL_TRANSMITTING) {
        src = Dcm_DslTxBuffer;
        total = Dcm_Dsl.txLen;
        copied = &Dcm_Dsl.txCopied;
    } else {
        return BUFREQ_E_NOT_OK;
    }
    if ((retry != NULL_PTR) && (retry->TpDataState == TP_DATARETRY) && (retry->TxTpDataCnt <= *copied)) {
        *copied = (PduLengthType)(*copied - retry->TxTpDataCnt);
    }
    if (info->SduLength > (PduLengthType)(total - *copied)) {
        return BUFREQ_E_NOT_OK;
    }
    if (info->SduLength != 0u) {
        (void)memcpy(info->SduDataPtr, &src[*copied], info->SduLength);
        *copied = (PduLengthType)(*copied + info->SduLength);
    }
    *availableDataPtr = (PduLengthType)(total - *copied);
    return BUFREQ_OK;
}

/* [AUTOSAR API] SWS_Dcm_00351 */
void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result)
{
    if (id != DcmConf_DcmDslProtocolTx_DiagResp) {
        return;
    }
    if (Dcm_Dsl.rcrrpInFlight) {
        Dcm_Dsl.rcrrpInFlight = FALSE;
        UDS_TRACE("Dcm/DSL", "TpTxConfirmation(0x78, %s); service keeps running",
                  (result == E_OK) ? "E_OK" : "E_NOT_OK");
        if (Dcm_Dsl.finalWaiting) {
            Dcm_Dsl.finalWaiting = FALSE;
            Dcm_DslTransmitFinal(Dcm_Dsl.finalLen);
        }
        return;
    }
    if (Dcm_Dsl.state == DCM_DSL_TRANSMITTING) {
        UDS_TRACE("Dcm/DSL", "TpTxConfirmation(%s): response on the bus (SWS_Dcm_00353: P2 monitoring stops)",
                  (result == E_OK) ? "E_OK" : "E_NOT_OK");
        Dcm_DslFinishRequest((result == E_OK) ? TRUE : FALSE);
    }
}
