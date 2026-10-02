/*
 * Can.c
 *
 * [Educational Implementation] Can driver MOCK.
 * Real AUTOSAR counterpart: vendor CAN MCAL (RH850 RS-CANFD driver).
 *
 * Processing model chosen for this demo (CAN SWS R22-11 p.50, p.109-110):
 *   CanRxProcessing = INTERRUPT : frames are delivered from Can_Isr_GlobalRxFifo
 *   CanTxProcessing = POLLING   : TX completion is detected in Can_MainFunction_Write
 *   Mode changes    = asynchronous, confirmed in Can_MainFunction_Mode
 * Mixing both styles on purpose shows that CanIf callbacks can come from ISR
 * *or* task context and must be written for both (SWS p.51).
 *
 * Every place where a real RS-CANFD driver would touch registers is marked
 * with "[RH850 Hardware]". Register names are from the P1M-E hardware notes
 * (docs/reference/research/04-rh850-hardware-notes.md); exact bit positions
 * must be confirmed against the hardware manual of the actual derivative.
 */
#include "Can.h"
#include "CanIf_Cbk.h"
#include "Det.h"
#include "UdsTrace.h"
#include "VirtualCanBus.h"
#include <string.h>

typedef enum { CAN_DRV_UNINIT = 0, CAN_DRV_READY } Can_DriverStateType;

typedef struct {
    boolean   busy;
    PduIdType swPduHandle;   /* SWS_Can_00276: kept until TX confirmation */
} Can_TxObjectStateType;

static Can_DriverStateType      Can_DriverState = CAN_DRV_UNINIT;
static const Can_ConfigType    *Can_CfgPtr;
static Can_ControllerStateType  Can_CtrlState[CAN_NUM_CONTROLLERS];
static boolean                  Can_ModeChangePending[CAN_NUM_CONTROLLERS];
static Can_ControllerStateType  Can_ModeRequested[CAN_NUM_CONTROLLERS];
static Can_TxObjectStateType    Can_TxObj[CAN_NUM_HOH];
static boolean                  Can_RxIrqEnabled;

static const char *Can_StateName(Can_ControllerStateType s)
{
    switch (s) {
    case CAN_CS_STARTED: return "STARTED";
    case CAN_CS_STOPPED: return "STOPPED";
    case CAN_CS_SLEEP:   return "SLEEP";
    default:             return "UNINIT";
    }
}

void Can_Init(const Can_ConfigType *Config)
{
    uint8 i;

    if (Can_DriverState != CAN_DRV_UNINIT) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_INIT, CAN_E_TRANSITION);
        return;
    }
    if (Config == NULL_PTR) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_INIT, CAN_E_PARAM_POINTER);
        return;
    }
    Can_CfgPtr = Config;

    /* [RH850 Hardware] Real driver sequence (conceptual, confirm in HW manual):
     *  1. wait for CAN RAM init to finish (GSTS.GRAMINIT == 0)
     *  2. global reset mode: GCTR.GMDC, then GCFG (clock source DCS: clkc 40 MHz
     *     or clk_xincan 16 MHz on P1M-E), channel reset mode CmCTR.CHMDC
     *  3. bit timing CmNCFG (see Can_BitTiming.c in rh850_mcal_reference)
     *  4. receive rules GAFLIDj/GAFLMj/GAFLP0j/GAFLP1j from every RECEIVE HOH
     *  5. RX FIFO RFCCx (depth RFDC, RFIE, RFIM) -> interrupt INTRCANGRECC (EI190)
     *  6. global operating mode; channels stay in reset = CAN_CS_STOPPED
     */
    for (i = 0u; i < Config->numHoh; i++) {
        const Can_HardwareObjectConfigType *hoh = &Config->hoh[i];
        if (hoh->objectType == CAN_OBJECT_TYPE_RECEIVE) {
            /* The "label" stored with the rule is the HRH, so the ISR can tell
             * CanIf which hardware object received the frame. */
            VirtualCanBus_HwSetRxRule(hoh->hwBufferIdx, hoh->filterCode, hoh->filterMask, hoh->objectId);
            UDS_TRACE("Can", "Init: receive rule %u <- %s (code=0x%03lX mask=0x%03lX)",
                      (unsigned)hoh->hwBufferIdx, hoh->name,
                      (unsigned long)hoh->filterCode, (unsigned long)hoh->filterMask);
        }
        Can_TxObj[i].busy = FALSE;
    }
    for (i = 0u; i < CAN_NUM_CONTROLLERS; i++) {
        Can_CtrlState[i] = CAN_CS_STOPPED;          /* SWS_Can_00259 */
        Can_ModeChangePending[i] = FALSE;
    }
    VirtualCanBus_HwSetOnline(FALSE);
    Can_RxIrqEnabled = TRUE;   /* [RH850 Hardware] RFCCx.RFIE + EIC190 unmasked */
    Can_DriverState = CAN_DRV_READY;
    UDS_TRACE("Can", "Init done: controller 0 -> STOPPED (RS-CANFD channel reset mode)");
}

void Can_DeInit(void)
{
    Can_DriverState = CAN_DRV_UNINIT;
    Can_CtrlState[0] = CAN_CS_UNINIT;
    VirtualCanBus_HwSetOnline(FALSE);
}

/* [AUTOSAR API] Asynchronous (SWS_Can_00230): only *requests* the transition.
 * The new state is reported later by CanIf_ControllerModeIndication from
 * Can_MainFunction_Mode, exactly like a real driver polling CmSTS. */
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition)
{
    Can_ControllerStateType cur;
    boolean valid;

    if (Can_DriverState != CAN_DRV_READY) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_SET_CONTROLLER_MODE, CAN_E_UNINIT);
        return E_NOT_OK;
    }
    if (Controller >= CAN_NUM_CONTROLLERS) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_SET_CONTROLLER_MODE, CAN_E_PARAM_CONTROLLER);
        return E_NOT_OK;
    }
    cur = Can_CtrlState[Controller];
    valid = (boolean)(((cur == CAN_CS_STOPPED) && (Transition == CAN_CS_STARTED)) ||
                      ((cur == CAN_CS_STARTED) && (Transition == CAN_CS_STOPPED)) ||
                      ((cur == CAN_CS_STOPPED) && (Transition == CAN_CS_SLEEP))   ||
                      ((cur == CAN_CS_SLEEP)   && (Transition == CAN_CS_STOPPED)) ||
                      (cur == Transition));
    if (!valid) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_SET_CONTROLLER_MODE, CAN_E_TRANSITION);
        return E_NOT_OK;
    }
    /* [RH850 Hardware] write CmCTR.CHMDC (communication / reset / halt);
     * the mode is reached when CmSTS reports it. */
    Can_ModeRequested[Controller] = Transition;
    Can_ModeChangePending[Controller] = TRUE;
    UDS_TRACE("Can", "SetControllerMode(%u, %s) requested (CmCTR.CHMDC written)",
              (unsigned)Controller, Can_StateName(Transition));
    return E_OK;
}

Std_ReturnType Can_GetControllerMode(uint8 Controller, Can_ControllerStateType *ControllerModePtr)
{
    if ((Controller >= CAN_NUM_CONTROLLERS) || (ControllerModePtr == NULL_PTR)) {
        return E_NOT_OK;
    }
    *ControllerModePtr = Can_CtrlState[Controller];
    return E_OK;
}

void Can_MainFunction_Mode(void)
{
    uint8 c;
    for (c = 0u; c < CAN_NUM_CONTROLLERS; c++) {
        if (Can_ModeChangePending[c]) {
            uint8 h;
            Can_ModeChangePending[c] = FALSE;
            Can_CtrlState[c] = Can_ModeRequested[c];
            VirtualCanBus_HwSetOnline((Can_CtrlState[c] == CAN_CS_STARTED) ? TRUE : FALSE);
            if (Can_CtrlState[c] != CAN_CS_STARTED) {
                /* SWS_Can_00282: pending transmissions are cancelled, no confirmation. */
                for (h = 0u; h < CAN_NUM_HOH; h++) {
                    Can_TxObj[h].busy = FALSE;
                }
            }
            UDS_TRACE("Can", "MainFunction_Mode: CmSTS shows %s -> CanIf_ControllerModeIndication",
                      Can_StateName(Can_CtrlState[c]));
            CanIf_ControllerModeIndication(c, Can_CtrlState[c]);
        }
    }
}

/* [AUTOSAR API] SWS_Can_00233 Can_Write(Hth, PduInfo). Non-blocking:
 * copies the frame into the hardware object and requests transmission. */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    const Can_HardwareObjectConfigType *hoh;
    VirtualCanBus_FrameType frame;

    if (Can_DriverState != CAN_DRV_READY) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_WRITE, CAN_E_UNINIT);
        return E_NOT_OK;
    }
    if ((Hth >= Can_CfgPtr->numHoh) || (Can_CfgPtr->hoh[Hth].objectType != CAN_OBJECT_TYPE_TRANSMIT)) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_WRITE, CAN_E_PARAM_HANDLE);
        return E_NOT_OK;
    }
    if ((PduInfo == NULL_PTR) || ((PduInfo->sdu == NULL_PTR) && (PduInfo->length != 0u))) {
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_WRITE, CAN_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (PduInfo->length > 8u) {   /* SWS_Can_00218: classical CAN, no FD in this demo */
        (void)Det_ReportError(DET_MODULE_ID_CAN, 0u, CAN_SID_WRITE, CAN_E_PARAM_DATA_LENGTH);
        return E_NOT_OK;
    }
    hoh = &Can_CfgPtr->hoh[Hth];
    if (Can_CtrlState[hoh->controllerId] != CAN_CS_STARTED) {
        return E_NOT_OK;
    }
    /* SWS_Can_00213: hardware object occupied -> CAN_BUSY, do not abort it. */
    if (Can_TxObj[Hth].busy || VirtualCanBus_HwTxBufferBusy(hoh->hwBufferIdx)) {
        UDS_TRACE("Can", "Write HTH=%u busy -> CAN_BUSY (CanIf will buffer)", (unsigned)Hth);
        return CAN_BUSY;
    }

    (void)memset(&frame, 0, sizeof(frame));
    frame.id = PduInfo->id & 0x7FFu;
    frame.dlc = PduInfo->length;
    if (PduInfo->length != 0u) {
        (void)memcpy(frame.data, PduInfo->sdu, PduInfo->length);  /* SWS_Can_00011 */
    }
    /* [RH850 Hardware] real driver: write TMIDp (ID), TMPTRp (DLC), TMDF0_p/TMDF1_p
     * (payload) of TX buffer p, then set TMCp.TMTR = 1 (8-bit register). */
    (void)VirtualCanBus_HwTxBufferWrite(hoh->hwBufferIdx, &frame);
    Can_TxObj[Hth].busy = TRUE;
    Can_TxObj[Hth].swPduHandle = PduInfo->swPduHandle;
    UDS_TRACE("Can", "Write HTH=%u ID=0x%03lX DLC=%u -> TX buffer %u, TMC.TMTR=1  [%s]",
              (unsigned)Hth, (unsigned long)frame.id, (unsigned)frame.dlc,
              (unsigned)hoh->hwBufferIdx, UdsTrace_Hex(frame.data, frame.dlc));
    return E_OK;
}

/* [AUTOSAR API] SWS_Can_00225: polls TX completion (CanTxProcessing = POLLING). */
void Can_MainFunction_Write(void)
{
    uint8 h;
    if (Can_DriverState != CAN_DRV_READY) {
        return;
    }
    for (h = 0u; h < Can_CfgPtr->numHoh; h++) {
        const Can_HardwareObjectConfigType *hoh = &Can_CfgPtr->hoh[h];
        if ((hoh->objectType == CAN_OBJECT_TYPE_TRANSMIT) && Can_TxObj[h].busy) {
            /* [RH850 Hardware] real driver: read TMSTSp.TMTRF (transmit result),
             * then clear it. In interrupt mode this would be the TX ISR. */
            if (VirtualCanBus_HwTxCompleteFlag(hoh->hwBufferIdx)) {
                Can_TxObj[h].busy = FALSE;
                UDS_TRACE("Can", "MainFunction_Write: TX buffer %u done (TMSTS.TMTRF) -> CanIf_TxConfirmation(L-PDU %u)",
                          (unsigned)hoh->hwBufferIdx, (unsigned)Can_TxObj[h].swPduHandle);
                CanIf_TxConfirmation(Can_TxObj[h].swPduHandle);
            }
        }
    }
}

/* CanRxProcessing = INTERRUPT: nothing to poll (SWS_Can_00180 allows an empty body). */
void Can_MainFunction_Read(void)
{
}

boolean Can_IsRxInterruptEnabled(void)
{
    return (boolean)(Can_RxIrqEnabled && (Can_DriverState == CAN_DRV_READY));
}

/* [Educational Implementation] simulated ISR for INTRCANGRECC (EI190).
 * Real ISR: read RFSTSx, loop while RX FIFO not empty (RFEMP == 0), read
 * RFIDx/RFPTRx/RFDF0_x/RFDF1_x, advance with RFPCTRx = 0xFF, clear RFIF
 * (SWS_Can_00420: clear the interrupt flag at the end). */
void Can_Isr_GlobalRxFifo(void)
{
    VirtualCanBus_FrameType frame;
    uint16 label;

    if (Can_DriverState != CAN_DRV_READY) {
        return;
    }
    while (VirtualCanBus_HwRxFifoRead(&frame, &label)) {
        Can_HwType mailbox;
        PduInfoType pdu;

        mailbox.CanId = frame.id;           /* standard ID: bit31/30 = 00 */
        mailbox.Hoh = label;                /* HRH from the matching receive rule */
        mailbox.ControllerId = Can_CfgPtr->hoh[label].controllerId;
        pdu.SduDataPtr = frame.data;        /* local copy = "shadow buffer" (SWS_Can_00299) */
        pdu.MetaDataPtr = NULL_PTR;
        pdu.SduLength = frame.dlc;
        UDS_TRACE("Can", "ISR EI190 (RX FIFO): ID=0x%03lX HRH=%u DLC=%u -> CanIf_RxIndication",
                  (unsigned long)frame.id, (unsigned)label, (unsigned)frame.dlc);
        CanIf_RxIndication(&mailbox, &pdu);
    }
}
