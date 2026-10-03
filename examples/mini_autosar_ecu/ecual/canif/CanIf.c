/*
 * CanIf.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CAN Interface (AUTOSAR_CP_SWS_CANInterface, R25-11). The ECU abstraction layer between the CAN driver
 * (Can, MCAL) and the PDU router (PduR): it knows the L-PDUs (CAN id + DLC + HTH/HRH) and hides the hardware objects from the
 * upper layers. Implemented: one controller, static TxPdu/RxPdu tables from CanIf_Config, CanIf_Init, CanIf_Transmit, the driver
 * callbacks CanIf_RxIndication / CanIf_TxConfirmation / CanIf_ControllerBusOff / CanIf_ControllerModeIndication, controller mode
 * wrappers CanIf_SetControllerMode / CanIf_GetControllerMode, CanIf_GetVersionInfo, Det checks.
 * Not implemented: PDU channel modes (online/offline), TX buffering/queue (CAN_BUSY is reported to PduR as E_NOT_OK), dynamic CAN id,
 * TX confirmation polling/trigger-transmit, transceivers, wakeup, partial networking, DLC check variants (always: shorter is dropped).
 * Deviation: CanIf_ControllerBusOff recovers by restarting the controller at once; in a real stack CanSM decides (bus-off recovery
 * time, L1/L2 handling, DEM event).
 *
 * Spec: AUTOSAR_CP_SWS_CANInterface  SWS_CANIF_00001 CanIf_Init, 00003 CanIf_SetControllerMode, 00229 CanIf_GetControllerMode,
 *   00005 CanIf_Transmit, 00006 CanIf_RxIndication (callback), 00007 CanIf_TxConfirmation (callback), 00218 CanIf_ControllerBusOff,
 *   00699 CanIf_ControllerModeIndication; Det: CANIF_E_UNINIT 30, _PARAM_POINTER 20, _PARAM_CONTROLLERID 15, _INVALID_TXPDUID 50,
 *   _INVALID_RXPDUID 60, runtime CANIF_E_INVALID_DATA_LENGTH 61 / CANIF_E_DATA_LENGTH_MISMATCH 62.
 * RH850/AUTOSAR context: docs/04-can-mcal/07-hoh-hrh-hth.md (HOH handles), 10/11 write/rx implementation chapters.
 */
#include "CanIf.h"
#include "CanIf_Cbk.h"
#include "Can.h"
#include "PduR.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_ModuleIds.h"

#define CANIF_E_PARAM_CONTROLLERID   15u
#define CANIF_E_PARAM_POINTER        20u
#define CANIF_E_PARAM_CTRLMODE       21u
#define CANIF_E_UNINIT               30u
#define CANIF_E_INVALID_TXPDUID      50u
#define CANIF_E_INVALID_RXPDUID      60u
#define CANIF_E_INVALID_DATA_LENGTH  61u
#define CANIF_E_DATA_LENGTH_MISMATCH 62u

#define CANIF_SID_INIT               0x01u
#define CANIF_SID_SETCTRLMODE        0x03u
#define CANIF_SID_GETCTRLMODE        0x04u
#define CANIF_SID_TRANSMIT           0x05u
#define CANIF_SID_GETVERSIONINFO     0x0Au
#define CANIF_SID_TXCONFIRMATION     0x13u
#define CANIF_SID_RXINDICATION       0x14u
#define CANIF_SID_CTRLBUSOFF         0x16u
#define CANIF_SID_CTRLMODEINDICATION 0x17u

#define CANIF_CONTROLLER_ID          0u

#define CANIF_DET(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_CANIF, 0u, (api), (err)); } } while (0)

static const CanIf_ConfigType  *canif_cfg;
static Can_ControllerStateType  canif_mode = CAN_CS_UNINIT;

void CanIf_Init(const CanIf_ConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        CANIF_DET(CANIF_SID_INIT, CANIF_E_PARAM_POINTER);
        return;
    }
    canif_cfg  = ConfigPtr;
    canif_mode = CAN_CS_STOPPED;                           /* the Can driver was initialised before and is STOPPED */
}

Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    Can_StateTransitionType t;
    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_SETCTRLMODE, CANIF_E_UNINIT);
        return E_NOT_OK;
    }
    if (ControllerId != CANIF_CONTROLLER_ID) {
        CANIF_DET(CANIF_SID_SETCTRLMODE, CANIF_E_PARAM_CONTROLLERID);
        return E_NOT_OK;
    }
    if (ControllerMode == CAN_CS_STARTED)      { t = CAN_T_START; }
    else if (ControllerMode == CAN_CS_STOPPED) { t = CAN_T_STOP; }
    else {                                                 /* SLEEP is not supported */
        CANIF_DET(CANIF_SID_SETCTRLMODE, CANIF_E_PARAM_CTRLMODE);
        return E_NOT_OK;
    }
    if (canif_mode == ControllerMode) {
        return E_OK;                                       /* already there */
    }
    /* canif_mode is updated by CanIf_ControllerModeIndication() (called by Can_SetControllerMode on success) */
    return Can_SetControllerMode(CANIF_CONTROLLER_ID, t);
}

Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType *ControllerModePtr)
{
    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_GETCTRLMODE, CANIF_E_UNINIT);
        return E_NOT_OK;
    }
    if (ControllerId != CANIF_CONTROLLER_ID) {
        CANIF_DET(CANIF_SID_GETCTRLMODE, CANIF_E_PARAM_CONTROLLERID);
        return E_NOT_OK;
    }
    if (ControllerModePtr == NULL_PTR) {
        CANIF_DET(CANIF_SID_GETCTRLMODE, CANIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    *ControllerModePtr = canif_mode;
    return E_OK;
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    const CanIf_TxPduConfigType *p;
    Can_PduType   can;
    Can_ReturnType r;

    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_TRANSMIT, CANIF_E_UNINIT);
        return E_NOT_OK;
    }
    if (TxPduId >= canif_cfg->numTxPdus) {
        CANIF_DET(CANIF_SID_TRANSMIT, CANIF_E_INVALID_TXPDUID);
        return E_NOT_OK;
    }
    if ((PduInfoPtr == NULL_PTR) || ((PduInfoPtr->SduDataPtr == NULL_PTR) && (PduInfoPtr->SduLength != 0u))) {
        CANIF_DET(CANIF_SID_TRANSMIT, CANIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    p = &canif_cfg->txPdus[TxPduId];
    if (PduInfoPtr->SduLength > p->dlc) {                  /* longer than the configured L-PDU: refuse (DLC is fixed in this CanIf) */
        (void)Det_ReportRuntimeError(MINI_MODULE_CANIF, 0u, CANIF_SID_TRANSMIT, CANIF_E_DATA_LENGTH_MISMATCH);
        return E_NOT_OK;
    }
    if (canif_mode != CAN_CS_STARTED) {                    /* controller not started: no transmission */
        return E_NOT_OK;
    }
    can.swPduHandle = TxPduId;                             /* comes back in CanIf_TxConfirmation */
    can.length      = (uint8)PduInfoPtr->SduLength;
    can.id          = p->canId;
    can.sdu         = PduInfoPtr->SduDataPtr;
    r = Can_Write(p->hth, &can);
    if (r == CAN_OK) {
        TRACE(TRACE_CAT_CANIF, "TX pdu=%u", (unsigned)TxPduId);
        return E_OK;
    }
    return E_NOT_OK;                                       /* CAN_BUSY: no queue here -> upper layer retries (Com: next period) */
}

void CanIf_GetVersionInfo(Std_VersionInfoType *VersionInfo)
{
    if (VersionInfo == NULL_PTR) {
        CANIF_DET(CANIF_SID_GETVERSIONINFO, CANIF_E_PARAM_POINTER);
        return;
    }
    VersionInfo->vendorID         = MINI_VENDOR_ID;
    VersionInfo->moduleID         = MINI_MODULE_CANIF;
    VersionInfo->sw_major_version = 1u;
    VersionInfo->sw_minor_version = 0u;
    VersionInfo->sw_patch_version = 0u;
}

/* ------------------------------------------------------------------------------------------------ callbacks of the Can driver */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
    uint8 i;
    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_RXINDICATION, CANIF_E_UNINIT);
        return;
    }
    if ((Mailbox == NULL_PTR) || (PduInfoPtr == NULL_PTR) ||
        ((PduInfoPtr->SduDataPtr == NULL_PTR) && (PduInfoPtr->SduLength != 0u))) {
        CANIF_DET(CANIF_SID_RXINDICATION, CANIF_E_PARAM_POINTER);
        return;
    }
    if (Mailbox->ControllerId != CANIF_CONTROLLER_ID) {
        CANIF_DET(CANIF_SID_RXINDICATION, CANIF_E_PARAM_CONTROLLERID);
        return;
    }
    for (i = 0u; i < canif_cfg->numRxPdus; i++) {          /* exact CAN id + hardware object (HRH) match */
        const CanIf_RxPduConfigType *p = &canif_cfg->rxPdus[i];
        if ((p->canId == (Mailbox->CanId & CAN_ID_VALUE_MASK_STD)) && (p->hrh == Mailbox->Hoh)) {
            PduInfoType up;
            if (PduInfoPtr->SduLength < p->dlc) {          /* too short: drop, runtime error (SWS_CANIF DLC check) */
                (void)Det_ReportRuntimeError(MINI_MODULE_CANIF, 0u, CANIF_SID_RXINDICATION, CANIF_E_INVALID_DATA_LENGTH);
                return;
            }
            up.SduDataPtr  = PduInfoPtr->SduDataPtr;
            up.MetaDataPtr = NULL_PTR;
            up.SduLength   = PduInfoPtr->SduLength;
            TRACE(TRACE_CAT_CANIF, "RX pdu=%u", (unsigned)i);
            PduR_CanIfRxIndication(p->upperPduId, &up);    /* ISR context: PduR/Com only copy (DEFERRED) or notify (IMMEDIATE) */
            return;
        }
    }
    /* no L-PDU configured for this id: silently ignored (hardware filters normally keep these out) */
}

void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_TXCONFIRMATION, CANIF_E_UNINIT);
        return;
    }
    if (CanTxPduId >= canif_cfg->numTxPdus) {
        CANIF_DET(CANIF_SID_TXCONFIRMATION, CANIF_E_INVALID_TXPDUID);
        return;
    }
    PduR_CanIfTxConfirmation(canif_cfg->txPdus[CanTxPduId].upperPduId, E_OK);
}

void CanIf_ControllerBusOff(uint8 ControllerId)
{
    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_CTRLBUSOFF, CANIF_E_UNINIT);
        return;
    }
    if (ControllerId != CANIF_CONTROLLER_ID) {
        CANIF_DET(CANIF_SID_CTRLBUSOFF, CANIF_E_PARAM_CONTROLLERID);
        return;
    }
    canif_mode = CAN_CS_STOPPED;
    TRACE(TRACE_CAT_CANIF, "MODE ctrl=%u mode=%u", (unsigned)ControllerId, (unsigned)CAN_CS_STOPPED);
    (void)Can_SetControllerMode(CANIF_CONTROLLER_ID, CAN_T_START);     /* simplified recovery, see header comment */
}

void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    if (canif_cfg == NULL_PTR) {
        CANIF_DET(CANIF_SID_CTRLMODEINDICATION, CANIF_E_UNINIT);
        return;
    }
    if (ControllerId != CANIF_CONTROLLER_ID) {
        CANIF_DET(CANIF_SID_CTRLMODEINDICATION, CANIF_E_PARAM_CONTROLLERID);
        return;
    }
    canif_mode = ControllerMode;
    TRACE(TRACE_CAT_CANIF, "MODE ctrl=%u mode=%u", (unsigned)ControllerId, (unsigned)ControllerMode);
}
