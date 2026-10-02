/*
 * CanTp.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CanTp (ISO 15765-2). See CanTp.h for scope.
 *
 * Why CanTp exists: one CAN frame carries 8 bytes, a UDS message up to 4095
 * (here). CanTp cuts the message into frames, adds the PCI byte(s), runs the
 * flow-control handshake (FC with BS/STmin) and supervises every step with
 * N_xx timers. The upper layer (PduR -> Dcm) never sees frames, only:
 *   RX: StartOfReception -> CopyRxData (n times) -> RxIndication
 *   TX: Transmit -> CopyTxData (n times) -> TxConfirmation
 *
 * PCI encoding (normal addressing, classical CAN):
 *   SF  0x0L               L = 1..7 payload bytes
 *   FF  0x1L LL            12-bit total length, 6 payload bytes
 *   CF  0x2N               N = sequence number 0..15 (first CF has N=1)
 *   FC  0x3S BS STmin      S = 0 CTS, 1 WAIT, 2 OVFLW
 */
#include "CanTp.h"
#include "CanIf.h"
#include "PduR.h"
#include "Det.h"
#include "UdsTrace.h"
#include <string.h>

#define CANTP_PCI_SF 0x0u
#define CANTP_PCI_FF 0x1u
#define CANTP_PCI_CF 0x2u
#define CANTP_PCI_FC 0x3u

#define CANTP_FC_CTS   0x0u
#define CANTP_FC_WAIT  0x1u
#define CANTP_FC_OVFLW 0x2u

typedef enum {
    CANTP_RX_IDLE = 0,
    CANTP_RX_WAIT_FC_CONF,   /* our FC was handed to CanIf, N_Ar running        */
    CANTP_RX_WAIT_CF,        /* N_Cr running                                    */
    CANTP_RX_WAIT_BUFFER     /* upper layer has no room, FC.WAIT sent, N_Br     */
} CanTp_RxStateType;

typedef struct {
    CanTp_RxStateType state;
    PduLengthType     total;
    PduLengthType     received;
    PduLengthType     upperBufferSize;   /* last bufferSize reported by PduR/Dcm */
    uint8             nextSn;
    uint8             blockRemaining;
    uint8             wftCount;
    uint8             fcInFlight;        /* FS of the FC waiting for TX confirmation */
    uint16            timerMs;
} CanTp_RxRuntimeType;

typedef enum {
    CANTP_TX_IDLE = 0,
    CANTP_TX_WAIT_DATA,      /* CopyTxData returned BUSY, retry (N_Cs)          */
    CANTP_TX_WAIT_CONF,      /* frame handed to CanIf, N_As running             */
    CANTP_TX_WAIT_FC,        /* FF or last CF of a block sent, N_Bs running     */
    CANTP_TX_WAIT_STMIN      /* next CF is due after STmin                      */
} CanTp_TxStateType;

typedef enum { CANTP_FRAME_SF, CANTP_FRAME_FF, CANTP_FRAME_CF } CanTp_FrameKindType;

typedef struct {
    CanTp_TxStateType   state;
    CanTp_FrameKindType nextKind;
    CanTp_FrameKindType inFlightKind;
    PduLengthType       total;
    PduLengthType       sent;           /* payload bytes confirmed              */
    uint8               inFlightLen;    /* payload bytes in the frame in flight */
    uint8               sn;
    uint8               bs;             /* from the receiver's FC               */
    uint8               bsRemaining;
    uint8               stMinMs;
    uint16              timerMs;
} CanTp_TxRuntimeType;

static const CanTp_ConfigType *CanTp_CfgPtr;
static CanTp_RxRuntimeType CanTp_Rx[CANTP_NUM_RX_NSDU];
static CanTp_TxRuntimeType CanTp_Tx[CANTP_NUM_TX_NSDU];

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static uint8 CanTp_DecodeStMin(uint8 raw)
{
    if (raw <= 0x7Fu) {
        return raw;                       /* 0..127 ms */
    }
    if ((raw >= 0xF1u) && (raw <= 0xF9u)) {
        return 1u;                        /* 100..900 us rounded up to the 1 ms tick */
    }
    return 0x7Fu;                         /* reserved values: ISO says use 127 ms */
}

static Std_ReturnType CanTp_SendFrame(PduIdType canIfTxPduId, uint8 *frame, uint8 usedBytes)
{
    PduInfoType pdu;
    uint8 i;
    for (i = usedBytes; i < 8u; i++) {
        frame[i] = CANTP_PADDING_BYTE;    /* CanTpPaddingActivation = ON */
    }
    pdu.SduDataPtr = frame;
    pdu.MetaDataPtr = NULL_PTR;
    pdu.SduLength = 8u;
    return CanIf_Transmit(canIfTxPduId, &pdu);
}

static void CanTp_RxAbort(uint8 idx, const char *reason)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    UDS_TRACE("CanTp", "RX %s aborted (%s) -> PduR_CanTpRxIndication(E_NOT_OK)", cfg->name, reason);
    CanTp_Rx[idx].state = CANTP_RX_IDLE;
    (void)Det_ReportRuntimeError(DET_MODULE_ID_CANTP, 0u, CANTP_SID_MAIN, CANTP_E_RX_COM);
    PduR_CanTpRxIndication(cfg->pdurSduId, E_NOT_OK);
}

static void CanTp_TxAbort(uint8 idx, const char *reason)
{
    const CanTp_TxNSduConfigType *cfg = &CanTp_CfgPtr->txNSdus[idx];
    UDS_TRACE("CanTp", "TX %s aborted (%s) -> PduR_CanTpTxConfirmation(E_NOT_OK)", cfg->name, reason);
    CanTp_Tx[idx].state = CANTP_TX_IDLE;
    (void)Det_ReportRuntimeError(DET_MODULE_ID_CANTP, 0u, CANTP_SID_MAIN, CANTP_E_TX_COM);
    PduR_CanTpTxConfirmation(cfg->pdurSduId, E_NOT_OK);
}

/* ------------------------------------------------------------------ */
/* receive side                                                        */
/* ------------------------------------------------------------------ */

static void CanTp_RxSendFc(uint8 idx, uint8 fs)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    CanTp_RxRuntimeType *rt = &CanTp_Rx[idx];
    uint8 frame[8];

    frame[0] = (uint8)((CANTP_PCI_FC << 4) | fs);
    frame[1] = cfg->bs;
    frame[2] = cfg->stMin;
    UDS_TRACE("CanTp", "RX %s: send FC %s (BS=%u STmin=%u ms)", cfg->name,
              (fs == CANTP_FC_CTS) ? "CTS" : ((fs == CANTP_FC_WAIT) ? "WAIT" : "OVFLW"),
              (unsigned)cfg->bs, (unsigned)cfg->stMin);
    rt->fcInFlight = fs;
    if (CanTp_SendFrame(cfg->canIfFcTxPduId, frame, 3u) != E_OK) {
        CanTp_RxAbort(idx, "CanIf_Transmit(FC) failed");
        return;
    }
    rt->state = CANTP_RX_WAIT_FC_CONF;
    rt->timerMs = cfg->nArMs;
}

/* Bytes the upper layer must be able to take before we may send FC.CTS:
 * the next block (BS * 7) or the rest of the message, whichever is smaller. */
static PduLengthType CanTp_RxBytesNeeded(uint8 idx)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    const CanTp_RxRuntimeType *rt = &CanTp_Rx[idx];
    PduLengthType remaining = (PduLengthType)(rt->total - rt->received);
    PduLengthType needed = (cfg->bs == 0u) ? remaining : (PduLengthType)(cfg->bs * 7u);
    return (needed > remaining) ? remaining : needed;
}

/* Decide CTS or WAIT depending on how much buffer the upper layer offers. */
static void CanTp_RxContinueOrWait(uint8 idx)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    CanTp_RxRuntimeType *rt = &CanTp_Rx[idx];

    if (rt->upperBufferSize >= CanTp_RxBytesNeeded(idx)) {
        rt->wftCount = 0u;
        rt->blockRemaining = cfg->bs;
        CanTp_RxSendFc(idx, CANTP_FC_CTS);
    } else if (rt->wftCount < cfg->wftMax) {
        rt->wftCount++;
        CanTp_RxSendFc(idx, CANTP_FC_WAIT);
    } else {
        CanTp_RxAbort(idx, "CanTpRxWftMax exceeded");
    }
}

static void CanTp_RxSingleFrame(uint8 idx, const PduInfoType *info)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    CanTp_RxRuntimeType *rt = &CanTp_Rx[idx];
    PduLengthType len = (PduLengthType)(info->SduDataPtr[0] & 0x0Fu);
    PduLengthType bufSize = 0u;
    PduInfoType payload;
    BufReq_ReturnType br;

    if ((len == 0u) || (len > 7u) || (len > (PduLengthType)(info->SduLength - 1u))) {
        UDS_TRACE("CanTp", "RX %s: invalid SF_DL %u -> ignored", cfg->name, (unsigned)len);
        return;
    }
    if (rt->state != CANTP_RX_IDLE) {
        CanTp_RxAbort(idx, "new SF while segmented reception in progress");
    }
    payload.SduDataPtr = &info->SduDataPtr[1];
    payload.MetaDataPtr = NULL_PTR;
    payload.SduLength = len;
    UDS_TRACE("CanTp", "RX %s: SF len=%u [%s] -> PduR_CanTpStartOfReception(%u)", cfg->name,
              (unsigned)len, UdsTrace_Hex(payload.SduDataPtr, len), (unsigned)cfg->pdurSduId);
    br = PduR_CanTpStartOfReception(cfg->pdurSduId, &payload, len, &bufSize);
    if (br != BUFREQ_OK) {
        UDS_TRACE("CanTp", "RX %s: StartOfReception refused (%d) -> SF dropped", cfg->name, (int)br);
        return;   /* reception never started: no RxIndication */
    }
    if (bufSize < len) {
        PduR_CanTpRxIndication(cfg->pdurSduId, E_NOT_OK);
        return;
    }
    br = PduR_CanTpCopyRxData(cfg->pdurSduId, &payload, &bufSize);
    PduR_CanTpRxIndication(cfg->pdurSduId, (br == BUFREQ_OK) ? E_OK : E_NOT_OK);
}

static void CanTp_RxFirstFrame(uint8 idx, const PduInfoType *info)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    CanTp_RxRuntimeType *rt = &CanTp_Rx[idx];
    PduLengthType total;
    PduLengthType bufSize = 0u;
    PduInfoType payload;
    BufReq_ReturnType br;

    if (cfg->taType == CANTP_FUNCTIONAL) {
        UDS_TRACE("CanTp", "RX %s: FF on functional address -> ignored (ISO 15765-2)", cfg->name);
        return;
    }
    if (info->SduLength < 8u) {
        return;
    }
    total = (PduLengthType)(((PduLengthType)(info->SduDataPtr[0] & 0x0Fu) << 8) | info->SduDataPtr[1]);
    if (total <= 7u) {
        UDS_TRACE("CanTp", "RX %s: FF_DL %u <= 7 invalid -> ignored", cfg->name, (unsigned)total);
        return;
    }
    if (rt->state != CANTP_RX_IDLE) {
        CanTp_RxAbort(idx, "new FF while segmented reception in progress");
    }
    payload.SduDataPtr = &info->SduDataPtr[2];
    payload.MetaDataPtr = NULL_PTR;
    payload.SduLength = 6u;
    UDS_TRACE("CanTp", "RX %s: FF total=%u -> PduR_CanTpStartOfReception(%u)", cfg->name,
              (unsigned)total, (unsigned)cfg->pdurSduId);
    br = PduR_CanTpStartOfReception(cfg->pdurSduId, &payload, total, &bufSize);
    if (br == BUFREQ_E_OVFL) {
        rt->total = total;
        rt->received = 0u;
        CanTp_RxSendFc(idx, CANTP_FC_OVFLW);
        return;
    }
    if (br != BUFREQ_OK) {
        UDS_TRACE("CanTp", "RX %s: StartOfReception refused -> no FC, frame dropped", cfg->name);
        return;
    }
    br = PduR_CanTpCopyRxData(cfg->pdurSduId, &payload, &bufSize);
    if (br != BUFREQ_OK) {
        rt->state = CANTP_RX_WAIT_CF;   /* so that RxAbort reports it */
        CanTp_RxAbort(idx, "CopyRxData(FF) failed");
        return;
    }
    rt->total = total;
    rt->received = 6u;
    rt->nextSn = 1u;
    rt->wftCount = 0u;
    rt->upperBufferSize = bufSize;
    CanTp_RxContinueOrWait(idx);
}

static void CanTp_RxConsecutiveFrame(uint8 idx, const PduInfoType *info)
{
    const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[idx];
    CanTp_RxRuntimeType *rt = &CanTp_Rx[idx];
    uint8 sn = (uint8)(info->SduDataPtr[0] & 0x0Fu);
    PduLengthType remaining;
    PduLengthType bufSize = 0u;
    PduInfoType payload;

    if (rt->state != CANTP_RX_WAIT_CF) {
        UDS_TRACE("CanTp", "RX %s: unexpected CF -> ignored", cfg->name);
        return;
    }
    if (sn != rt->nextSn) {
        CanTp_RxAbort(idx, "wrong sequence number");
        return;
    }
    remaining = (PduLengthType)(rt->total - rt->received);
    payload.SduDataPtr = &info->SduDataPtr[1];
    payload.MetaDataPtr = NULL_PTR;
    payload.SduLength = (remaining > 7u) ? 7u : remaining;
    if ((PduLengthType)(info->SduLength - 1u) < payload.SduLength) {
        CanTp_RxAbort(idx, "CF too short");
        return;
    }
    if (PduR_CanTpCopyRxData(cfg->pdurSduId, &payload, &bufSize) != BUFREQ_OK) {
        CanTp_RxAbort(idx, "CopyRxData(CF) failed");
        return;
    }
    rt->received = (PduLengthType)(rt->received + payload.SduLength);
    rt->upperBufferSize = bufSize;
    rt->nextSn = (uint8)((rt->nextSn + 1u) & 0x0Fu);
    UDS_TRACE("CanTp", "RX %s: CF SN=%u (%u/%u bytes)", cfg->name, (unsigned)sn,
              (unsigned)rt->received, (unsigned)rt->total);

    if (rt->received >= rt->total) {
        rt->state = CANTP_RX_IDLE;
        UDS_TRACE("CanTp", "RX %s: complete -> PduR_CanTpRxIndication(E_OK)", cfg->name);
        PduR_CanTpRxIndication(cfg->pdurSduId, E_OK);
        return;
    }
    if (cfg->bs != 0u) {
        rt->blockRemaining--;
        if (rt->blockRemaining == 0u) {
            CanTp_RxContinueOrWait(idx);   /* block finished: next FC */
            return;
        }
    }
    rt->timerMs = cfg->nCrMs;
}

/* ------------------------------------------------------------------ */
/* transmit side                                                       */
/* ------------------------------------------------------------------ */

/* Builds and sends the next SF/FF/CF. Data comes from the upper layer via
 * PduR_CanTpCopyTxData - CanTp has no copy of the full message. */
static void CanTp_TxSendNext(uint8 idx)
{
    const CanTp_TxNSduConfigType *cfg = &CanTp_CfgPtr->txNSdus[idx];
    CanTp_TxRuntimeType *rt = &CanTp_Tx[idx];
    uint8 frame[8];
    uint8 pciLen;
    PduLengthType payloadLen;
    PduLengthType available = 0u;
    PduInfoType pdu;
    BufReq_ReturnType br;

    switch (rt->nextKind) {
    case CANTP_FRAME_SF:
        frame[0] = (uint8)(CANTP_PCI_SF << 4 | (uint8)rt->total);
        pciLen = 1u;
        payloadLen = rt->total;
        break;
    case CANTP_FRAME_FF:
        frame[0] = (uint8)((CANTP_PCI_FF << 4) | (uint8)((rt->total >> 8) & 0x0Fu));
        frame[1] = (uint8)(rt->total & 0xFFu);
        pciLen = 2u;
        payloadLen = 6u;
        break;
    default:
        frame[0] = (uint8)((CANTP_PCI_CF << 4) | rt->sn);
        pciLen = 1u;
        payloadLen = (PduLengthType)(rt->total - rt->sent);
        if (payloadLen > 7u) {
            payloadLen = 7u;
        }
        break;
    }

    pdu.SduDataPtr = &frame[pciLen];
    pdu.MetaDataPtr = NULL_PTR;
    pdu.SduLength = payloadLen;
    /* RetryInfo NULL: this CanTp never asks for re-transmission of data. */
    br = PduR_CanTpCopyTxData(cfg->pdurSduId, &pdu, (const RetryInfoType *)NULL_PTR, &available);
    if (br == BUFREQ_E_BUSY) {
        if (rt->state != CANTP_TX_WAIT_DATA) {
            rt->state = CANTP_TX_WAIT_DATA;
            rt->timerMs = cfg->nCsMs;
        }
        return;
    }
    if (br != BUFREQ_OK) {
        CanTp_TxAbort(idx, "CopyTxData failed");
        return;
    }
    UDS_TRACE("CanTp", "TX %s: %s payload=%u [%s] -> CanIf_Transmit(L-PDU %u)", cfg->name,
              (rt->nextKind == CANTP_FRAME_SF) ? "SF" : ((rt->nextKind == CANTP_FRAME_FF) ? "FF" : "CF"),
              (unsigned)payloadLen, UdsTrace_Hex(pdu.SduDataPtr, payloadLen), (unsigned)cfg->canIfTxPduId);
    rt->inFlightKind = rt->nextKind;
    rt->inFlightLen = (uint8)payloadLen;
    if (CanTp_SendFrame(cfg->canIfTxPduId, frame, (uint8)(pciLen + payloadLen)) != E_OK) {
        CanTp_TxAbort(idx, "CanIf_Transmit failed");
        return;
    }
    rt->state = CANTP_TX_WAIT_CONF;
    rt->timerMs = cfg->nAsMs;
}

/* [AUTOSAR API] Called by PduR_DcmTransmit. Only the *length* is passed;
 * the data is pulled later with CopyTxData. */
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    CanTp_TxRuntimeType *rt;
    const CanTp_TxNSduConfigType *cfg;

    if (CanTp_CfgPtr == NULL_PTR) {
        (void)Det_ReportError(DET_MODULE_ID_CANTP, 0u, CANTP_SID_TRANSMIT, CANTP_E_UNINIT);
        return E_NOT_OK;
    }
    if ((TxPduId >= CanTp_CfgPtr->numTxNSdus) || (PduInfoPtr == NULL_PTR)) {
        (void)Det_ReportError(DET_MODULE_ID_CANTP, 0u, CANTP_SID_TRANSMIT, CANTP_E_PARAM_ID);
        return E_NOT_OK;
    }
    cfg = &CanTp_CfgPtr->txNSdus[TxPduId];
    rt = &CanTp_Tx[TxPduId];
    if ((rt->state != CANTP_TX_IDLE) || (PduInfoPtr->SduLength == 0u) || (PduInfoPtr->SduLength > 4095u) ||
        ((cfg->taType == CANTP_FUNCTIONAL) && (PduInfoPtr->SduLength > 7u))) {
        UDS_TRACE("CanTp", "Transmit %s rejected (busy or invalid length)", cfg->name);
        return E_NOT_OK;
    }
    rt->total = PduInfoPtr->SduLength;
    rt->sent = 0u;
    rt->sn = 1u;
    rt->nextKind = (rt->total <= 7u) ? CANTP_FRAME_SF : CANTP_FRAME_FF;
    UDS_TRACE("CanTp", "Transmit %s length=%u -> %s", cfg->name, (unsigned)rt->total,
              (rt->nextKind == CANTP_FRAME_SF) ? "single frame" : "segmented (FF + CFs, needs FC from tester)");
    rt->state = CANTP_TX_WAIT_DATA;
    rt->timerMs = cfg->nCsMs;
    CanTp_TxSendNext((uint8)TxPduId);
    return E_OK;
}

static void CanTp_TxFlowControl(uint8 idx, const PduInfoType *info)
{
    const CanTp_TxNSduConfigType *cfg = &CanTp_CfgPtr->txNSdus[idx];
    CanTp_TxRuntimeType *rt = &CanTp_Tx[idx];
    uint8 fs = (uint8)(info->SduDataPtr[0] & 0x0Fu);

    if (rt->state != CANTP_TX_WAIT_FC) {
        UDS_TRACE("CanTp", "TX %s: unexpected FC -> ignored", cfg->name);
        return;
    }
    if (info->SduLength < 3u) {
        return;
    }
    switch (fs) {
    case CANTP_FC_CTS:
        rt->bs = info->SduDataPtr[1];
        rt->bsRemaining = rt->bs;
        rt->stMinMs = CanTp_DecodeStMin(info->SduDataPtr[2]);
        UDS_TRACE("CanTp", "TX %s: FC CTS received (BS=%u STmin=%u ms) -> CFs from CanTp_MainFunction",
                  cfg->name, (unsigned)rt->bs, (unsigned)rt->stMinMs);
        rt->nextKind = CANTP_FRAME_CF;
        rt->state = CANTP_TX_WAIT_STMIN;
        rt->timerMs = 0u;   /* first CF of a block may go immediately */
        break;
    case CANTP_FC_WAIT:
        UDS_TRACE("CanTp", "TX %s: FC WAIT -> restart N_Bs", cfg->name);
        rt->timerMs = cfg->nBsMs;
        break;
    case CANTP_FC_OVFLW:
        CanTp_TxAbort(idx, "receiver reported FC OVFLW");
        break;
    default:
        CanTp_TxAbort(idx, "invalid FC flow status");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* lower layer callbacks                                               */
/* ------------------------------------------------------------------ */

/* [AUTOSAR API] Called by CanIf (ISR context in the real system). */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    uint8 pciType;
    uint8 i;

    if ((CanTp_CfgPtr == NULL_PTR) || (PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduLength == 0u)) {
        return;
    }
    pciType = (uint8)(PduInfoPtr->SduDataPtr[0] >> 4);

    if (pciType == CANTP_PCI_FC) {
        for (i = 0u; i < CanTp_CfgPtr->numTxNSdus; i++) {
            if (CanTp_CfgPtr->txNSdus[i].rxFcNPduId == RxPduId) {
                CanTp_TxFlowControl(i, PduInfoPtr);
                return;
            }
        }
        return;
    }
    for (i = 0u; i < CanTp_CfgPtr->numRxNSdus; i++) {
        if (CanTp_CfgPtr->rxNSdus[i].rxNPduId == RxPduId) {
            switch (pciType) {
            case CANTP_PCI_SF: CanTp_RxSingleFrame(i, PduInfoPtr); break;
            case CANTP_PCI_FF: CanTp_RxFirstFrame(i, PduInfoPtr); break;
            case CANTP_PCI_CF: CanTp_RxConsecutiveFrame(i, PduInfoPtr); break;
            default:
                UDS_TRACE("CanTp", "RX N-PDU %u: unknown PCI type %u -> ignored", (unsigned)RxPduId, (unsigned)pciType);
                break;
            }
            return;
        }
    }
}

/* [AUTOSAR API] Called by CanIf when a frame we sent is on the bus. */
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    uint8 i;

    if (CanTp_CfgPtr == NULL_PTR) {
        return;
    }
    /* Confirmation of one of our FC frames (receive direction). */
    for (i = 0u; i < CanTp_CfgPtr->numRxNSdus; i++) {
        CanTp_RxRuntimeType *rt = &CanTp_Rx[i];
        const CanTp_RxNSduConfigType *cfg = &CanTp_CfgPtr->rxNSdus[i];
        if ((cfg->txFcNPduId == TxPduId) && (rt->state == CANTP_RX_WAIT_FC_CONF)) {
            if (result != E_OK) {
                CanTp_RxAbort(i, "FC transmission failed");
            } else if (rt->fcInFlight == CANTP_FC_CTS) {
                rt->state = CANTP_RX_WAIT_CF;
                rt->timerMs = cfg->nCrMs;
            } else if (rt->fcInFlight == CANTP_FC_WAIT) {
                rt->state = CANTP_RX_WAIT_BUFFER;
                rt->timerMs = cfg->nBrMs;
            } else {
                rt->state = CANTP_RX_IDLE;   /* OVFLW: reception ends, upper layer refused it */
            }
            return;
        }
    }
    /* Confirmation of a data frame (transmit direction). */
    for (i = 0u; i < CanTp_CfgPtr->numTxNSdus; i++) {
        CanTp_TxRuntimeType *rt = &CanTp_Tx[i];
        const CanTp_TxNSduConfigType *cfg = &CanTp_CfgPtr->txNSdus[i];
        if ((cfg->txNPduId != TxPduId) || (rt->state != CANTP_TX_WAIT_CONF)) {
            continue;
        }
        if (result != E_OK) {
            CanTp_TxAbort(i, "CanIf TX confirmation negative");
            return;
        }
        rt->sent = (PduLengthType)(rt->sent + rt->inFlightLen);
        if (rt->sent >= rt->total) {
            rt->state = CANTP_TX_IDLE;
            UDS_TRACE("CanTp", "TX %s: complete (%u bytes) -> PduR_CanTpTxConfirmation(E_OK)",
                      cfg->name, (unsigned)rt->total);
            PduR_CanTpTxConfirmation(cfg->pdurSduId, E_OK);
            return;
        }
        if (rt->inFlightKind == CANTP_FRAME_FF) {
            rt->state = CANTP_TX_WAIT_FC;            /* N_Bs: wait for tester FC */
            rt->timerMs = cfg->nBsMs;
            return;
        }
        rt->sn = (uint8)((rt->sn + 1u) & 0x0Fu);
        if (rt->bs != 0u) {
            rt->bsRemaining--;
            if (rt->bsRemaining == 0u) {
                rt->state = CANTP_TX_WAIT_FC;        /* block done */
                rt->timerMs = cfg->nBsMs;
                return;
            }
        }
        rt->state = CANTP_TX_WAIT_STMIN;
        rt->timerMs = rt->stMinMs;
        return;
    }
}

/* ------------------------------------------------------------------ */
/* init / main                                                         */
/* ------------------------------------------------------------------ */

void CanTp_Init(const CanTp_ConfigType *CfgPtr)
{
    CanTp_CfgPtr = CfgPtr;
    (void)memset(CanTp_Rx, 0, sizeof(CanTp_Rx));
    (void)memset(CanTp_Tx, 0, sizeof(CanTp_Tx));
    UDS_TRACE("CanTp", "Init: %u Rx N-SDUs, %u Tx N-SDUs, MainFunction period %u ms",
              (unsigned)CfgPtr->numRxNSdus, (unsigned)CfgPtr->numTxNSdus, (unsigned)CANTP_MAIN_FUNCTION_PERIOD_MS);
}

static boolean CanTp_TimerExpired(uint16 *timerMs)
{
    if (*timerMs <= CANTP_MAIN_FUNCTION_PERIOD_MS) {
        *timerMs = 0u;
        return TRUE;
    }
    *timerMs = (uint16)(*timerMs - CANTP_MAIN_FUNCTION_PERIOD_MS);
    return FALSE;
}

/* [AUTOSAR API] Cyclic: timers, STmin pacing, retries. */
void CanTp_MainFunction(void)
{
    uint8 i;

    if (CanTp_CfgPtr == NULL_PTR) {
        return;
    }
    for (i = 0u; i < CanTp_CfgPtr->numRxNSdus; i++) {
        CanTp_RxRuntimeType *rt = &CanTp_Rx[i];
        switch (rt->state) {
        case CANTP_RX_WAIT_FC_CONF:
            if (CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_RxAbort(i, "N_Ar timeout");
            }
            break;
        case CANTP_RX_WAIT_CF:
            if (CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_RxAbort(i, "N_Cr timeout: tester stopped sending CFs");
            }
            break;
        case CANTP_RX_WAIT_BUFFER: {
            /* Ask the upper layer again how much buffer it has (SduLength = 0 query). */
            PduInfoType query;
            PduLengthType bufSize = 0u;
            query.SduDataPtr = NULL_PTR;
            query.MetaDataPtr = NULL_PTR;
            query.SduLength = 0u;
            if (PduR_CanTpCopyRxData(CanTp_CfgPtr->rxNSdus[i].pdurSduId, &query, &bufSize) != BUFREQ_OK) {
                CanTp_RxAbort(i, "CopyRxData query failed");
                break;
            }
            rt->upperBufferSize = bufSize;
            if ((bufSize >= CanTp_RxBytesNeeded(i)) || CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_RxContinueOrWait(i);
            }
            break;
        }
        default:
            break;
        }
    }
    for (i = 0u; i < CanTp_CfgPtr->numTxNSdus; i++) {
        CanTp_TxRuntimeType *rt = &CanTp_Tx[i];
        switch (rt->state) {
        case CANTP_TX_WAIT_CONF:
            if (CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_TxAbort(i, "N_As timeout: frame not confirmed by CanIf");
            }
            break;
        case CANTP_TX_WAIT_FC:
            if (CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_TxAbort(i, "N_Bs timeout: no FC from tester");
            }
            break;
        case CANTP_TX_WAIT_STMIN:
            if (CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_TxSendNext(i);
            }
            break;
        case CANTP_TX_WAIT_DATA:
            if (CanTp_TimerExpired(&rt->timerMs)) {
                CanTp_TxAbort(i, "N_Cs: upper layer did not provide data");
            } else {
                CanTp_TxSendNext(i);
            }
            break;
        default:
            break;
        }
    }
}
