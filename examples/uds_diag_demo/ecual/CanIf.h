/*
 * CanIf.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CAN Interface (CanIf), ECU Abstraction Layer.
 * Responsibilities kept in this demo:
 *   - map (HRH, CAN ID) -> Rx L-PDU -> upper layer (CanTp) + upper PduId
 *   - map Tx L-PDU -> (HTH, CAN ID) and call Can_Write
 *   - buffer a Tx L-PDU when Can_Write returns CAN_BUSY and retry on confirmation
 *   - forward TX confirmation to the upper layer
 *   - controller mode bookkeeping (CanIf_SetControllerMode)
 * Not implemented: PduMode (online/offline), DLC check config, CanSM bus-off
 * handling, wake-up, multiple drivers, dynamic IDs, trigger transmit.
 *
 * No CanIf SWS in this repository: signature per R4.x convention,
 * confirm against project release.
 */
#ifndef CANIF_H
#define CANIF_H

#include "Can_GeneralTypes.h"
#include "CanIf_Cbk.h"
#include "CanIf_Cfg.h"

/* Upper layer callback shapes (CanTp_RxIndication / CanTp_TxConfirmation). */
typedef void (*CanIf_UpperRxIndicationFctType)(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
typedef void (*CanIf_UpperTxConfirmationFctType)(PduIdType TxPduId, Std_ReturnType result);

/* CanIfRxPduCfg */
typedef struct {
    Can_IdType                       canId;          /* CanIfRxPduCanId          */
    Can_HwHandleType                 hrh;            /* CanIfRxPduHrhIdRef       */
    uint8                            dlcMin;         /* CanIfRxPduDataLength     */
    PduIdType                        upperPduId;     /* id in the upper module   */
    CanIf_UpperRxIndicationFctType   rxIndication;   /* CanIfRxPduUserRxIndicationUL = CAN_TP */
    const char                      *name;
} CanIf_RxPduConfigType;

/* CanIfTxPduCfg */
typedef struct {
    Can_IdType                       canId;          /* CanIfTxPduCanId          */
    Can_HwHandleType                 hth;            /* CanIfTxPduBufferRef -> HTH */
    PduIdType                        upperPduId;     /* id echoed to upper layer */
    CanIf_UpperTxConfirmationFctType txConfirmation; /* CanIfTxPduUserTxConfirmationUL */
    const char                      *name;
} CanIf_TxPduConfigType;

typedef struct {
    const CanIf_RxPduConfigType *rxPdus;
    uint8                        numRxPdus;
    const CanIf_TxPduConfigType *txPdus;
    uint8                        numTxPdus;
} CanIf_ConfigType;

extern const CanIf_ConfigType CanIf_Config;

#define CANIF_E_PARAM_LPDU     0x0Du
#define CANIF_E_UNINIT         0x30u
#define CANIF_SID_TRANSMIT     0x49u
#define CANIF_SID_RXINDICATION 0x14u

void CanIf_Init(const CanIf_ConfigType *ConfigPtr);
Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode);
Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType *ControllerModePtr);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

#endif /* CANIF_H */
