/*
 * PduR.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: PduR (TP routing CanTp <-> Dcm only). See PduR.h.
 */
#include "PduR.h"
#include "Det.h"
#include "UdsTrace.h"

static const PduR_PBConfigType *PduR_CfgPtr;

void PduR_Init(const PduR_PBConfigType *ConfigPtr)
{
    PduR_CfgPtr = ConfigPtr;
    UDS_TRACE("PduR", "Init: %u RX routing paths, %u TX routing paths",
              (unsigned)ConfigPtr->numRxPaths, (unsigned)ConfigPtr->numTxPaths);
}

static const PduR_RxRoutingPathType *PduR_FindRx(PduIdType id)
{
    uint8 i;
    if (PduR_CfgPtr == NULL_PTR) {
        return NULL_PTR;
    }
    for (i = 0u; i < PduR_CfgPtr->numRxPaths; i++) {
        if (PduR_CfgPtr->rxPaths[i].srcPduId == id) {
            return &PduR_CfgPtr->rxPaths[i];
        }
    }
    (void)Det_ReportError(DET_MODULE_ID_PDUR, 0u, 0x00u, 0x02u /* PDUR_E_PDU_ID_INVALID */);
    return NULL_PTR;
}

static const PduR_TxRoutingPathType *PduR_FindTxBySrc(PduIdType id)
{
    uint8 i;
    if (PduR_CfgPtr == NULL_PTR) {
        return NULL_PTR;
    }
    for (i = 0u; i < PduR_CfgPtr->numTxPaths; i++) {
        if (PduR_CfgPtr->txPaths[i].srcPduId == id) {
            return &PduR_CfgPtr->txPaths[i];
        }
    }
    (void)Det_ReportError(DET_MODULE_ID_PDUR, 0u, 0x00u, 0x02u);
    return NULL_PTR;
}

static const PduR_TxRoutingPathType *PduR_FindTxByLower(PduIdType id)
{
    uint8 i;
    if (PduR_CfgPtr == NULL_PTR) {
        return NULL_PTR;
    }
    for (i = 0u; i < PduR_CfgPtr->numTxPaths; i++) {
        if (PduR_CfgPtr->txPaths[i].lowerCbkPduId == id) {
            return &PduR_CfgPtr->txPaths[i];
        }
    }
    (void)Det_ReportError(DET_MODULE_ID_PDUR, 0u, 0x00u, 0x02u);
    return NULL_PTR;
}

/* ---------------- upper interface ---------------- */

Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    const PduR_TxRoutingPathType *p = PduR_FindTxBySrc(TxPduId);
    if (p == NULL_PTR) {
        return E_NOT_OK;
    }
    UDS_TRACE("PduR", "DcmTransmit(%u) len=%u -> route '%s' -> CanTp_Transmit(N-SDU %u)",
              (unsigned)TxPduId, (unsigned)PduInfoPtr->SduLength, p->name, (unsigned)p->lowerPduId);
    return p->lowerTransmit(p->lowerPduId, PduInfoPtr);
}

/* ---------------- lower TP interface ---------------- */

BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength, PduLengthType *bufferSizePtr)
{
    const PduR_RxRoutingPathType *p = PduR_FindRx(id);
    if (p == NULL_PTR) {
        return BUFREQ_E_NOT_OK;
    }
    UDS_TRACE("PduR", "CanTpStartOfReception(%u) -> route '%s' -> %s_StartOfReception(DcmRxPduId %u)",
              (unsigned)id, p->name, p->dest->name, (unsigned)p->destPduId);
    return p->dest->StartOfReception(p->destPduId, info, TpSduLength, bufferSizePtr);
}

BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr)
{
    const PduR_RxRoutingPathType *p = PduR_FindRx(id);
    if (p == NULL_PTR) {
        return BUFREQ_E_NOT_OK;
    }
    return p->dest->CopyRxData(p->destPduId, info, bufferSizePtr);
}

void PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result)
{
    const PduR_RxRoutingPathType *p = PduR_FindRx(id);
    if (p != NULL_PTR) {
        UDS_TRACE("PduR", "CanTpRxIndication(%u, %s) -> %s_TpRxIndication(DcmRxPduId %u)",
                  (unsigned)id, (result == E_OK) ? "E_OK" : "E_NOT_OK", p->dest->name, (unsigned)p->destPduId);
        p->dest->TpRxIndication(p->destPduId, result);
    }
}

BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                       PduLengthType *availableDataPtr)
{
    const PduR_TxRoutingPathType *p = PduR_FindTxByLower(id);
    if (p == NULL_PTR) {
        return BUFREQ_E_NOT_OK;
    }
    return p->upper->CopyTxData(p->upperPduId, info, retry, availableDataPtr);
}

void PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result)
{
    const PduR_TxRoutingPathType *p = PduR_FindTxByLower(id);
    if (p != NULL_PTR) {
        UDS_TRACE("PduR", "CanTpTxConfirmation(%u, %s) -> %s_TpTxConfirmation(DcmTxPduId %u)",
                  (unsigned)id, (result == E_OK) ? "E_OK" : "E_NOT_OK", p->upper->name, (unsigned)p->upperPduId);
        p->upper->TpTxConfirmation(p->upperPduId, result);
    }
}
