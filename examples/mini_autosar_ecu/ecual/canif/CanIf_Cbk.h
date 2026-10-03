/*
 * CanIf_Cbk.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CanIf_Cbk.h, the "callback" header of the CAN Interface: functions
 * that the CAN DRIVER (and only it) calls. R25-11 signatures (CANInterface 8.4.x):
 *   CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
 *   CanIf_TxConfirmation(PduIdType CanTxPduId)
 * May be called from ISR context (RX via Cat2 ISR) or from Can_MainFunction_Write (task context).
 * Implemented by agent B (CanIf.c).
 */
#ifndef CANIF_CBK_H
#define CANIF_CBK_H

#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
void CanIf_TxConfirmation(PduIdType CanTxPduId);
void CanIf_ControllerBusOff(uint8 ControllerId);
void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);

#endif /* CANIF_CBK_H */
