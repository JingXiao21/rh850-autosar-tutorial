/*
 * CanIf.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CAN Interface (AUTOSAR_CP_SWS_CANInterface; CanIf_Transmit,
 * CanIf_RxIndication 8.4.x, CanIf_TxConfirmation 8.4.2). Implemented: one controller, static
 * L-PDU tables (CanIfTxPduCfg / CanIfRxPduCfg), exact-id receive lookup, no buffering/queuing,
 * no dynamic CanId, no PN/wakeup, no TX offline/online channel modes beyond controller mode.
 * Callbacks that the CAN driver calls are in CanIf_Cbk.h (as in the real standard).
 * Owner: agent B. Config tables generated into CanIf_Cfg.c (agent D).
 */
#ifndef CANIF_H
#define CANIF_H

#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "CanIf_Cfg.h"        /* generated: CanIfConf_CanIfTxPduCfg_* / CanIfRxPduCfg ids */

typedef struct {
    Can_IdType       canId;            /* CanIfTxPduCanId */
    Can_HwHandleType hth;              /* CanIfTxPduBufferRef -> HTH */
    uint8            controller;
    uint8            dlc;              /* CanIfTxPduDlc (fixed) */
    PduIdType        upperPduId;       /* handle in PduR (tx path index) for PduR_CanIfTxConfirmation */
} CanIf_TxPduConfigType;

typedef struct {
    Can_IdType       canId;            /* CanIfRxPduCanId, exact match */
    Can_HwHandleType hrh;              /* CanIfRxPduHrhIdRef */
    uint8            dlc;              /* CanIfRxPduDlc: shorter frames are dropped (Det runtime error) */
    PduIdType        upperPduId;       /* handle in PduR (rx path index) for PduR_CanIfRxIndication */
} CanIf_RxPduConfigType;

typedef struct {
    uint8                        numTxPdus;
    const CanIf_TxPduConfigType *txPdus;     /* index = CanIf TxPduId (the id Pdur passes to CanIf_Transmit) */
    uint8                        numRxPdus;
    const CanIf_RxPduConfigType *rxPdus;
} CanIf_ConfigType;
extern const CanIf_ConfigType CanIf_Config;      /* generated CanIf_Cfg.c */

void           CanIf_Init(const CanIf_ConfigType *ConfigPtr);
Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode); /* STARTED / STOPPED */
Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType *ControllerModePtr);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
void           CanIf_GetVersionInfo(Std_VersionInfoType *VersionInfo);

#endif /* CANIF_H */
