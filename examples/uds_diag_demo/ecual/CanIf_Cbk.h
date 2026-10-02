/*
 * CanIf_Cbk.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CanIf_Cbk.h / CanIf_Can.h - the callbacks the Can
 * driver is required to call (CAN SWS R22-11 SWS_Can_00234 p.88).
 * The full signatures are defined in the CanIf SWS, which is not in this
 * repository: signature per R4.x convention (>= 4.2 Mailbox form,
 * >= 4.3 ControllerModeIndication with Can_ControllerStateType),
 * confirm against project release.
 */
#ifndef CANIF_CBK_H
#define CANIF_CBK_H

#include "Can_GeneralTypes.h"

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
void CanIf_TxConfirmation(PduIdType CanTxPduId);
void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);
void CanIf_ControllerBusOff(uint8 ControllerId);

#endif /* CANIF_CBK_H */
