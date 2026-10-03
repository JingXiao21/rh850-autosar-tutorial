/*
 * PduR.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: PDU Router (AUTOSAR_CP_SWS_PDURouter). Function names follow the real
 * R4.x/R25-11 convention PduR_<Module><Service>: PduR_ComTransmit (upper layer -> PduR),
 * PduR_CanIfRxIndication / PduR_CanIfTxConfirmation (lower layer -> PduR). So NO naming deviation.
 * Simplification: interface (IF) routing only, strictly 1:1, one PduR tx path per Com tx I-PDU and
 * one rx path per CanIf rx L-PDU; no gateway, no TP, no buffering/FIFO, no routing groups
 * (routing is always enabled after PduR_Init), no multicast.
 * Handle convention: PduR handle = index of the path in the config table; Com stores the tx path
 * index in Com_IpduConfigType.pdurPduId and CanIf stores rx/tx path index in upperPduId.
 * Owner: agent C. Config: below + generated PduR_Cfg.c/.h.
 */
#ifndef PDUR_H
#define PDUR_H

#include "ComStack_Types.h"
#include "PduR_Cfg.h"    /* generated: PduRConf_PduRSrcPdu_* ids */

typedef struct {
    PduIdType comTxPduId;        /* Com I-PDU handle (source), used for Com_TxConfirmation */
    PduIdType canIfTxPduId;      /* CanIf TxPduId (destination) for CanIf_Transmit */
} PduR_TxPathType;

typedef struct {
    PduIdType comRxPduId;        /* Com I-PDU handle (destination) for Com_RxIndication */
} PduR_RxPathType;

typedef struct {
    uint8                  numTxPaths;
    const PduR_TxPathType *txPaths;
    uint8                  numRxPaths;
    const PduR_RxPathType *rxPaths;
} PduR_PBConfigType;
extern const PduR_PBConfigType PduR_Config;      /* generated PduR_Cfg.c */

typedef enum { PDUR_UNINIT = 0, PDUR_ONLINE } PduR_StateType;

void           PduR_Init(const PduR_PBConfigType *ConfigPtr);
PduR_StateType PduR_GetState(void);
void           PduR_GetVersionInfo(Std_VersionInfoType *versioninfo);

/* upper layer (Com) -> PduR ; id = PduR tx path index */
Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *PduInfoPtr);

/* lower layer (CanIf) -> PduR ; id = PduR path index (CanIf_*PduConfigType.upperPduId) */
void           PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void           PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#endif /* PDUR_H */
