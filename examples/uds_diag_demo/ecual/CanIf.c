/*
 * CanIf.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CanIf (ECU Abstraction Layer). See CanIf.h.
 */
#include "CanIf.h"
#include "Can.h"
#include "Det.h"
#include "UdsTrace.h"
#include <string.h>

typedef struct {
    PduIdType txPduId;
    uint8     length;
    uint8     data[8];
} CanIf_TxBufferEntryType;

static const CanIf_ConfigType *CanIf_CfgPtr;
static Can_ControllerStateType CanIf_CtrlMode[CAN_NUM_CONTROLLERS];
static CanIf_TxBufferEntryType CanIf_TxBuf[CANIF_TX_BUFFER_DEPTH];
static uint8 CanIf_TxBufHead;
static uint8 CanIf_TxBufCount;

void CanIf_Init(const CanIf_ConfigType *ConfigPtr)
{
    uint8 i;
    CanIf_CfgPtr = ConfigPtr;
    for (i = 0u; i < CAN_NUM_CONTROLLERS; i++) {
        CanIf_CtrlMode[i] = CAN_CS_STOPPED;
    }
    CanIf_TxBufHead = 0u;
    CanIf_TxBufCount = 0u;
    UDS_TRACE("CanIf", "Init: %u Rx L-PDUs, %u Tx L-PDUs", (unsigned)ConfigPtr->numRxPdus,
              (unsigned)ConfigPtr->numTxPdus);
}

Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    if ((CanIf_CfgPtr == NULL_PTR) || (ControllerId >= CAN_NUM_CONTROLLERS)) {
        return E_NOT_OK;
    }
    UDS_TRACE("CanIf", "SetControllerMode(%u) -> Can_SetControllerMode", (unsigned)ControllerId);
    return Can_SetControllerMode(ControllerId, ControllerMode);
}

Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType *ControllerModePtr)
{
    if ((ControllerId >= CAN_NUM_CONTROLLERS) || (ControllerModePtr == NULL_PTR)) {
        return E_NOT_OK;
    }
    *ControllerModePtr = CanIf_CtrlMode[ControllerId];
    return E_OK;
}

void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    if (ControllerId < CAN_NUM_CONTROLLERS) {
        CanIf_CtrlMode[ControllerId] = ControllerMode;
        if (ControllerMode != CAN_CS_STARTED) {
            CanIf_TxBufCount = 0u;   /* buffered frames are dropped when going offline */
        }
        /* Real stack: forwards to CanSM_ControllerModeIndication. */
        UDS_TRACE("CanIf", "ControllerModeIndication(%u, %s) (real stack: -> CanSM)", (unsigned)ControllerId,
                  (ControllerMode == CAN_CS_STARTED) ? "STARTED" : "not started");
    }
}

void CanIf_ControllerBusOff(uint8 ControllerId)
{
    /* Real stack: -> CanSM_ControllerBusOff, CanSM decides on recovery
     * (SWS_Can_00274 forbids automatic recovery in the driver). */
    if (ControllerId < CAN_NUM_CONTROLLERS) {
        CanIf_CtrlMode[ControllerId] = CAN_CS_STOPPED;
    }
}

static Std_ReturnType CanIf_WriteToDriver(PduIdType TxPduId, const uint8 *data, uint8 length)
{
    const CanIf_TxPduConfigType *tx = &CanIf_CfgPtr->txPdus[TxPduId];
    Can_PduType canPdu;
    uint8 local[8];

    (void)memcpy(local, data, length);
    canPdu.swPduHandle = TxPduId;
    canPdu.length = length;
    canPdu.id = tx->canId;
    canPdu.sdu = local;
    return Can_Write(tx->hth, &canPdu);
}

/* [AUTOSAR API] CanIf_Transmit(TxPduId, PduInfoPtr): L-PDU -> HTH + CAN ID. */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    const CanIf_TxPduConfigType *tx;
    Std_ReturnType ret;
    uint8 length;

    if (CanIf_CfgPtr == NULL_PTR) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0u, CANIF_SID_TRANSMIT, CANIF_E_UNINIT);
        return E_NOT_OK;
    }
    if ((TxPduId >= CanIf_CfgPtr->numTxPdus) || (PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0u, CANIF_SID_TRANSMIT, CANIF_E_PARAM_LPDU);
        return E_NOT_OK;
    }
    tx = &CanIf_CfgPtr->txPdus[TxPduId];
    if (CanIf_CtrlMode[0] != CAN_CS_STARTED) {
        return E_NOT_OK;
    }
    length = (PduInfoPtr->SduLength > 8u) ? 8u : (uint8)PduInfoPtr->SduLength;
    UDS_TRACE("CanIf", "Transmit L-PDU %u (%s) -> Can_Write(HTH=%u, ID=0x%03lX)",
              (unsigned)TxPduId, tx->name, (unsigned)tx->hth, (unsigned long)tx->canId);

    /* Keep FIFO order: if older frames are buffered, queue behind them. */
    ret = (CanIf_TxBufCount == 0u) ? CanIf_WriteToDriver(TxPduId, PduInfoPtr->SduDataPtr, length) : CAN_BUSY;
    if (ret == CAN_BUSY) {
        if (CanIf_TxBufCount >= CANIF_TX_BUFFER_DEPTH) {
            UDS_TRACE("CanIf", "  Tx buffer full -> E_NOT_OK");
            return E_NOT_OK;
        } else {
            uint8 slot = (uint8)((CanIf_TxBufHead + CanIf_TxBufCount) % CANIF_TX_BUFFER_DEPTH);
            CanIf_TxBuf[slot].txPduId = TxPduId;
            CanIf_TxBuf[slot].length = length;
            (void)memcpy(CanIf_TxBuf[slot].data, PduInfoPtr->SduDataPtr, length);
            CanIf_TxBufCount++;
            UDS_TRACE("CanIf", "  HTH busy: L-PDU %u buffered in CanIf (depth now %u)",
                      (unsigned)TxPduId, (unsigned)CanIf_TxBufCount);
            ret = E_OK;   /* accepted: CanIf owns the retry */
        }
    }
    return ret;
}

/* [AUTOSAR API] Called by Can (ISR or Can_MainFunction_Write). */
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    const CanIf_TxPduConfigType *tx;

    if ((CanIf_CfgPtr == NULL_PTR) || (CanTxPduId >= CanIf_CfgPtr->numTxPdus)) {
        return;
    }
    tx = &CanIf_CfgPtr->txPdus[CanTxPduId];

    /* Hardware object is free again: send the oldest buffered L-PDU first. */
    if (CanIf_TxBufCount != 0u) {
        CanIf_TxBufferEntryType *e = &CanIf_TxBuf[CanIf_TxBufHead];
        if (CanIf_WriteToDriver(e->txPduId, e->data, e->length) == E_OK) {
            CanIf_TxBufHead = (uint8)((CanIf_TxBufHead + 1u) % CANIF_TX_BUFFER_DEPTH);
            CanIf_TxBufCount--;
        }
    }
    UDS_TRACE("CanIf", "TxConfirmation L-PDU %u (%s) -> CanTp_TxConfirmation(N-PDU %u)",
              (unsigned)CanTxPduId, tx->name, (unsigned)tx->upperPduId);
    tx->txConfirmation(tx->upperPduId, E_OK);
}

/* [AUTOSAR API] Called by Can from the RX ISR (or Can_MainFunction_Read).
 * Software filtering: search the Rx L-PDU whose HRH and CAN ID match. */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
    uint8 i;

    if (CanIf_CfgPtr == NULL_PTR) {
        (void)Det_ReportError(DET_MODULE_ID_CANIF, 0u, CANIF_SID_RXINDICATION, CANIF_E_UNINIT);
        return;
    }
    if (CanIf_CtrlMode[Mailbox->ControllerId] != CAN_CS_STARTED) {
        return;
    }
    for (i = 0u; i < CanIf_CfgPtr->numRxPdus; i++) {
        const CanIf_RxPduConfigType *rx = &CanIf_CfgPtr->rxPdus[i];
        if ((rx->hrh == Mailbox->Hoh) && (rx->canId == (Mailbox->CanId & 0x1FFFFFFFu))) {
            if (PduInfoPtr->SduLength < rx->dlcMin) {
                UDS_TRACE("CanIf", "RxIndication ID=0x%03lX DLC too short -> dropped",
                          (unsigned long)Mailbox->CanId);
                return;
            }
            UDS_TRACE("CanIf", "RxIndication HRH=%u ID=0x%03lX -> Rx L-PDU %u (%s) -> CanTp_RxIndication(N-PDU %u)",
                      (unsigned)Mailbox->Hoh, (unsigned long)Mailbox->CanId, (unsigned)i, rx->name,
                      (unsigned)rx->upperPduId);
            rx->rxIndication(rx->upperPduId, PduInfoPtr);
            return;
        }
    }
    UDS_TRACE("CanIf", "RxIndication HRH=%u ID=0x%03lX: no Rx L-PDU configured -> dropped",
              (unsigned)Mailbox->Hoh, (unsigned long)Mailbox->CanId);
}
