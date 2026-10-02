/*
 * CanTp.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CAN Transport Layer (CanTp), implementing
 * ISO 15765-2 segmentation. Implemented here:
 *   - SF / FF / CF / FC in both directions (classical CAN, 8 byte frames,
 *     normal addressing, 12-bit FF_DL i.e. max 4095 bytes)
 *   - BS / STmin on both sides, FC.WAIT when the upper layer has no buffer
 *   - N_As / N_Ar / N_Bs / N_Br / N_Cr timers in CanTp_MainFunction
 *   - physical (0x7E0 -> 0x7E8) and functional (0x7DF, SF only) addressing
 * Not implemented: CAN FD frames, extended/mixed addressing, escape FF_DL
 * (> 4095), CanTp_CancelTransmit/Receive, ChangeParameter, full-duplex
 * channel modelling rules, N_Cs as a separate timer (shares N_As handling).
 *
 * No CanTp SWS in this repository: signature per R4.x convention
 * (R4.4 form of CanTp_TxConfirmation with result), confirm against project release.
 */
#ifndef CANTP_H
#define CANTP_H

#include "ComStack_Types.h"
#include "CanTp_Cfg.h"

typedef enum {
    CANTP_PHYSICAL = 0,
    CANTP_FUNCTIONAL
} CanTp_TaTypeType;

/* CanTpRxNSdu (subset) */
typedef struct {
    PduIdType        rxNPduId;     /* CanTpRxNPdu: N-PDU carrying SF/FF/CF        */
    PduIdType        txFcNPduId;   /* CanTpTxFcNPdu: CanTp handle of our FC frame  */
    PduIdType        canIfFcTxPduId; /* CanIf L-PDU used for CanIf_Transmit of FC  */
    PduIdType        pdurSduId;    /* PduR handle for this N-SDU                   */
    CanTp_TaTypeType taType;       /* CanTpRxTaType                                */
    uint8            bs;           /* CanTpBs: block size we announce in FC        */
    uint8            stMin;        /* CanTpSTmin we announce in FC (ms)            */
    uint8            wftMax;       /* CanTpRxWftMax                                */
    uint16           nArMs;        /* CanTpNar: FC tx confirmation timeout          */
    uint16           nBrMs;        /* CanTpNbr: time until next FC (WAIT/CTS)       */
    uint16           nCrMs;        /* CanTpNcr: time until next CF                  */
    const char      *name;
} CanTp_RxNSduConfigType;

/* CanTpTxNSdu (subset) */
typedef struct {
    PduIdType        txNPduId;     /* CanTpTxNPdu: CanTp handle (TxConfirmation)   */
    PduIdType        canIfTxPduId; /* CanIf L-PDU used for CanIf_Transmit          */
    PduIdType        rxFcNPduId;   /* CanTpRxFcNPdu: where the tester's FC arrives */
    PduIdType        pdurSduId;    /* PduR handle for this N-SDU                   */
    CanTp_TaTypeType taType;
    uint16           nAsMs;        /* CanTpNas: frame tx confirmation timeout       */
    uint16           nBsMs;        /* CanTpNbs: wait for FC                         */
    uint16           nCsMs;        /* CanTpNcs: upper layer must provide data       */
    const char      *name;
} CanTp_TxNSduConfigType;

typedef struct {
    const CanTp_RxNSduConfigType *rxNSdus;
    uint8                         numRxNSdus;
    const CanTp_TxNSduConfigType *txNSdus;
    uint8                         numTxNSdus;
} CanTp_ConfigType;

extern const CanTp_ConfigType CanTp_Config;

#define CANTP_E_PARAM_ID     0x02u
#define CANTP_E_UNINIT       0x20u
#define CANTP_E_INVALID_TX_LENGTH 0x41u   /* (value illustrative) */
#define CANTP_E_RX_COM       0xC0u       /* runtime error: timeout / SN error  */
#define CANTP_E_TX_COM       0xC1u
#define CANTP_SID_TRANSMIT   0x49u
#define CANTP_SID_MAIN       0x06u

void CanTp_Init(const CanTp_ConfigType *CfgPtr);
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
void CanTp_MainFunction(void);

#endif /* CANTP_H */
