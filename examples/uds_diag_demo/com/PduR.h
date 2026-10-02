/*
 * PduR.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: PDU Router (PduR), split in real stacks into
 * PduR_CanTp.h (lower TP interface) and PduR_Dcm.h (upper interface).
 *
 * Why PduR exists: Dcm must not know whether the request came via CanTp,
 * FrTp, DoIP (SoAd) or LinTp; CanTp must not know whether its N-SDU belongs
 * to Dcm or to some other TP user. PduR is the configurable switchboard
 * between them. In this demo it only forwards TP calls 1:1 (no gateway,
 * no fan-out, no buffering), which is also what most real ECUs configure
 * for diagnostics ("zero cost operation" may even replace it by macros).
 *
 * No PduR SWS in this repository: signature per R4.x convention
 * (R4.2+ TP API: StartOfReception/CopyRxData/CopyTxData), confirm against
 * project release. Pre-R4.0 stacks used PduR_CanTpProvideRxBuffer instead.
 */
#ifndef PDUR_H
#define PDUR_H

#include "ComStack_Types.h"
#include "PduR_Cfg.h"

/* Function table of a TP upper layer (here: Dcm). */
typedef struct {
    BufReq_ReturnType (*StartOfReception)(PduIdType id, const PduInfoType *info,
                                          PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
    BufReq_ReturnType (*CopyRxData)(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr);
    void (*TpRxIndication)(PduIdType id, Std_ReturnType result);
    BufReq_ReturnType (*CopyTxData)(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                    PduLengthType *availableDataPtr);
    void (*TpTxConfirmation)(PduIdType id, Std_ReturnType result);
    const char *name;
} PduR_TpUpperLayerApiType;

/* RX path: CanTp N-SDU -> upper layer PDU */
typedef struct {
    PduIdType                        srcPduId;    /* PduR handle used by CanTp */
    PduIdType                        destPduId;   /* e.g. DcmRxPduId           */
    const PduR_TpUpperLayerApiType  *dest;
    const char                      *name;
} PduR_RxRoutingPathType;

/* TX path: upper layer PDU -> CanTp N-SDU, plus the way back for CopyTxData/TxConfirmation */
typedef struct {
    PduIdType                        srcPduId;       /* PduR handle used by Dcm            */
    PduIdType                        lowerPduId;     /* CanTp TxNSdu id for CanTp_Transmit */
    Std_ReturnType                 (*lowerTransmit)(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
    PduIdType                        lowerCbkPduId;  /* PduR handle CanTp uses on callbacks */
    PduIdType                        upperPduId;     /* DcmTxPduId (confirmation id)       */
    const PduR_TpUpperLayerApiType  *upper;
    const char                      *name;
} PduR_TxRoutingPathType;

typedef struct {
    const PduR_RxRoutingPathType *rxPaths;
    uint8                         numRxPaths;
    const PduR_TxRoutingPathType *txPaths;
    uint8                         numTxPaths;
} PduR_PBConfigType;

extern const PduR_PBConfigType PduR_Config;

void PduR_Init(const PduR_PBConfigType *ConfigPtr);

/* ---- upper interface (PduR_Dcm.h) ---- */
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* ---- lower TP interface (PduR_CanTp.h) ---- */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr);
void PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result);
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                       PduLengthType *availableDataPtr);
void PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result);

#endif /* PDUR_H */
