/*
 * Can.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CAN Driver (AUTOSAR_CP_SWS_CANDriver, R25-11). Hardware independent half of the driver; the
 * controller registers live behind Can_Hw.h (Can_Hw_Stm32.c on target, Can_Hw_Sim.c on host).
 *
 * Implemented:  ONE controller (index 0), standard 11 bit ids, classic frames; Can_Init, Can_SetControllerMode (STOPPED <-> STARTED
 *               state machine with Det CAN_E_TRANSITION), Can_Write (HTH -> hardware TX buffer, CAN_BUSY semantics, swPduHandle
 *               kept per buffer for the TX confirmation), TX confirmation by polling (Can_MainFunction_Write), RX by Cat2 interrupt
 *               (Can_Isr_Rx) or polling (Can_MainFunction_Read) as selected by Can_Config.rxInterrupt, bus-off detection
 *               (Can_MainFunction_BusOff), Can_Disable/EnableControllerInterrupts with nesting counter, trace lines.
 * Not implemented: CAN FD, extended ids, sleep/wakeup, baudrate change, TX cancellation of lower priority frames, error counters API,
 *               timeout monitoring with the OS counter (SWS_Can_00398 asks for GetCounterValue; a bounded loop in the backend is used).
 * Deviation:    Can_Write returns CAN_NOT_OK in STOPPED state (the SWS lets such requests be lost silently) so tests see the error.
 *
 * Spec: AUTOSAR_CP_SWS_CANDriver
 *   Can_Init                      SWS_Can_00223 (8.3.1), SWS_Can_00259 controllers STOPPED after init
 *   Can_SetControllerMode         SWS_Can_00230, SWS_Can_00261/00262 (START), SWS_Can_00409 invalid transition,
 *                                 SWS_Can_00372/00373 indication via CanIf_ControllerModeIndication
 *   Can_Write                     SWS_Can_00233 (8.3.3.1), SWS_Can_00212 free object, SWS_Can_00213/00214 CAN_BUSY,
 *                                 SWS_Can_00276 swPduHandle stored for confirmation, SWS_Can_00216/00217 Det
 *   Can_MainFunction_Write/Read/BusOff/Mode  SWS_Can_00225 / 00226 / 00227 / 00368
 *   Can_Disable/EnableControllerInterrupts   SWS_Can_00231 / 00232
 * RH850 mapping (docs/04-can-mcal/): FDCAN Rx FIFO0 <-> RS-CANFD RX FIFO, FDCAN standard filters <-> RS-CANFD receive rules,
 * FDCAN CCCR.INIT <-> RS-CANFD channel "reset/halt" mode, HTH <-> TX message buffer (docs 07-hoh-hrh-hth.md, 10/11 write/rx impl.).
 */
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf_Cbk.h"
#include "Det.h"
#include "SchM.h"
#include "Trace.h"
#include "Mini_ModuleIds.h"

/* ---- Det error / service ids (SWS_Can chapter 7.x, 8.x) ---- */
#define CAN_E_PARAM_POINTER      0x01u
#define CAN_E_PARAM_HANDLE       0x02u
#define CAN_E_PARAM_DATA_LENGTH  0x03u
#define CAN_E_PARAM_CONTROLLER   0x04u
#define CAN_E_UNINIT             0x05u
#define CAN_E_TRANSITION         0x06u
#define CAN_E_INIT_FAILED        0x09u
#define CAN_E_DATALOST           0x01u     /* runtime error: RX message lost */

#define CAN_SID_INIT             0x00u
#define CAN_SID_MAINFN_WRITE     0x01u
#define CAN_SID_SETCTRLMODE      0x03u
#define CAN_SID_DISABLE_INT      0x04u
#define CAN_SID_ENABLE_INT       0x05u
#define CAN_SID_WRITE            0x06u
#define CAN_SID_GETVERSIONINFO   0x07u
#define CAN_SID_MAINFN_READ      0x08u
#define CAN_SID_MAINFN_BUSOFF    0x09u

#define CAN_CONTROLLER_ID        0u         /* the only controller (CanIf ControllerId = 0) */

#define CAN_DET_ERROR(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_CAN, 0u, (api), (err)); } } while (0)

/* One entry per hardware TX buffer: which L-PDU (swPduHandle) and which HTH is in flight. */
typedef struct {
    boolean          used;
    Can_HwHandleType hth;
    PduIdType        swPduHandle;
} Can_TxSlotType;

static const Can_ConfigType   *can_cfg;                         /* NULL_PTR = CAN_UNINIT */
static Can_ControllerStateType can_state = CAN_CS_UNINIT;
static Can_TxSlotType          can_tx[CAN_HW_TX_BUFFERS];
static uint8                   can_irqDisableNesting;
static boolean                 can_busOffReported;
static uint32                  can_lostReported;

/* ------------------------------------------------------------------------------------------------ helpers */
static const Can_HohConfigType *can_find_hoh(Can_HwHandleType hoh, Can_HohTypeType type)
{
    uint8 i;
    for (i = 0u; i < can_cfg->numHoh; i++) {
        if ((can_cfg->hoh[i].hoh == hoh) && (can_cfg->hoh[i].type == type)) {
            return &can_cfg->hoh[i];
        }
    }
    return NULL_PTR;
}

static void can_notify_mode(Can_ControllerStateType mode)
{
    CanIf_ControllerModeIndication(CAN_CONTROLLER_ID, mode);
}

/* RX path shared by the ISR and the polling function: drain FIFO0, find the HRH, upcall CanIf. */
static void can_rx_drain(void)
{
    CanHw_FrameType f;
    uint32 lost;
    while (CanHw_RxFetch(&f)) {
        const Can_HohConfigType *h = NULL_PTR;
        uint8 i;
        for (i = 0u; i < can_cfg->numHoh; i++) {            /* software repeat of the hardware filter: which HRH is it? */
            const Can_HohConfigType *c = &can_cfg->hoh[i];
            if ((c->type == CAN_HOH_RECEIVE) && (((f.id ^ c->canId) & c->filterMask) == 0u)) {
                h = c;
                break;
            }
        }
        if (h == NULL_PTR) {
            continue;                                      /* not for us (hardware filter should have rejected it) */
        }
        {
            Can_HwType  mb;
            PduInfoType pdu;
            TRACE(TRACE_CAT_CAN, "RX id=0x%03x dlc=%u data=%B", (unsigned)f.id, (unsigned)f.dlc, f.data, (unsigned)f.dlc);
            mb.CanId        = f.id;
            mb.Hoh          = h->hoh;
            mb.ControllerId = CAN_CONTROLLER_ID;
            pdu.SduDataPtr  = f.data;
            pdu.MetaDataPtr = NULL_PTR;
            pdu.SduLength   = f.dlc;
            CanIf_RxIndication(&mb, &pdu);                 /* may run in ISR context: CanIf only passes the data on */
        }
    }
    lost = CanHw_GetRxLostCount();
    if (lost != can_lostReported) {                        /* FIFO overrun in the meantime: runtime error CAN_E_DATALOST */
        can_lostReported = lost;
        (void)Det_ReportRuntimeError(MINI_MODULE_CAN, 0u, CAN_SID_MAINFN_READ, CAN_E_DATALOST);
    }
}

/* ------------------------------------------------------------------------------------------------ API */
void Can_Init(const Can_ConfigType *Config)
{
    uint8 i;
    if (Config == NULL_PTR) {
        CAN_DET_ERROR(CAN_SID_INIT, CAN_E_PARAM_POINTER);
        return;
    }
    if (can_cfg != NULL_PTR) {                             /* SWS: must be CAN_UNINIT */
        CAN_DET_ERROR(CAN_SID_INIT, CAN_E_TRANSITION);
        return;
    }
    for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {
        can_tx[i].used = FALSE;
    }
    can_irqDisableNesting = 0u;
    can_busOffReported    = FALSE;
    can_lostReported      = 0u;
    if (CanHw_Init(Config) != E_OK) {
        CAN_DET_ERROR(CAN_SID_INIT, CAN_E_INIT_FAILED);
        return;
    }
    can_cfg   = Config;
    can_state = CAN_CS_STOPPED;                            /* SWS_Can_00259 */
}

Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    Std_ReturnType r = E_NOT_OK;
    if (can_cfg == NULL_PTR) {
        CAN_DET_ERROR(CAN_SID_SETCTRLMODE, CAN_E_UNINIT);
        return E_NOT_OK;
    }
    if (Controller != CAN_CONTROLLER_ID) {
        CAN_DET_ERROR(CAN_SID_SETCTRLMODE, CAN_E_PARAM_CONTROLLER);
        return E_NOT_OK;
    }
    switch (Transition) {
    case CAN_T_START:
        if (can_state != CAN_CS_STOPPED) {                 /* SWS_Can_00409 */
            CAN_DET_ERROR(CAN_SID_SETCTRLMODE, CAN_E_TRANSITION);
            return E_NOT_OK;
        }
        r = CanHw_Start();                                 /* waits (bounded) until operational, SWS_Can_00262 */
        if (r == E_OK) {
            can_state          = CAN_CS_STARTED;
            can_busOffReported = FALSE;
            can_notify_mode(CAN_CS_STARTED);
        }
        break;
    case CAN_T_STOP:
        if (can_state != CAN_CS_STARTED) {
            CAN_DET_ERROR(CAN_SID_SETCTRLMODE, CAN_E_TRANSITION);
            return E_NOT_OK;
        }
        r = CanHw_Stop();
        if (r == E_OK) {
            uint8 i;
            SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
            for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {     /* pending requests are lost (no confirmation) */
                can_tx[i].used = FALSE;
            }
            can_state = CAN_CS_STOPPED;
            SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
            can_notify_mode(CAN_CS_STOPPED);
        }
        break;
    default:                                               /* SLEEP / WAKEUP not supported */
        CAN_DET_ERROR(CAN_SID_SETCTRLMODE, CAN_E_TRANSITION);
        break;
    }
    return r;
}

Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    const Can_HohConfigType *h;
    Can_ReturnType ret = CAN_BUSY;
    uint8 idx = 0u;
    uint8 i;

    if (can_cfg == NULL_PTR) {                             /* SWS_Can_00216 */
        CAN_DET_ERROR(CAN_SID_WRITE, CAN_E_UNINIT);
        return CAN_NOT_OK;
    }
    h = can_find_hoh(Hth, CAN_HOH_TRANSMIT);
    if (h == NULL_PTR) {                                   /* SWS_Can_00217 */
        CAN_DET_ERROR(CAN_SID_WRITE, CAN_E_PARAM_HANDLE);
        return CAN_NOT_OK;
    }
    if ((PduInfo == NULL_PTR) || ((PduInfo->sdu == NULL_PTR) && (PduInfo->length != 0u))) {
        CAN_DET_ERROR(CAN_SID_WRITE, CAN_E_PARAM_POINTER);
        return CAN_NOT_OK;
    }
    if (PduInfo->length > 8u) {                            /* classic CAN only */
        CAN_DET_ERROR(CAN_SID_WRITE, CAN_E_PARAM_DATA_LENGTH);
        return CAN_NOT_OK;
    }
    if (((PduInfo->id & CAN_ID_EXTENDED_FLAG) != 0u) || (PduInfo->id > CAN_ID_VALUE_MASK_STD)) {
        return CAN_NOT_OK;                                 /* extended ids not implemented */
    }
    if (can_state != CAN_CS_STARTED) {
        return CAN_NOT_OK;
    }

    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();                 /* HTH "mutex" (SWS_Can_00212/00214): Can_Write vs. MainFunction */
    for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {
        if (can_tx[i].used && (can_tx[i].hth == Hth)) {    /* depth 1 per HTH: object busy, do not cancel (SWS_Can_00213) */
            SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
            return CAN_BUSY;
        }
    }
    if (CanHw_TxRequest(PduInfo->id, PduInfo->length, PduInfo->sdu, &idx) == E_OK) {
        can_tx[idx].used        = TRUE;                    /* remember the L-PDU for the confirmation (SWS_Can_00276) */
        can_tx[idx].hth         = Hth;
        can_tx[idx].swPduHandle = PduInfo->swPduHandle;
        ret = CAN_OK;
    }                                                      /* else: all hardware buffers occupied -> CAN_BUSY */
    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();

    if (ret == CAN_OK) {
        TRACE(TRACE_CAT_CAN, "TX id=0x%03x dlc=%u data=%B", (unsigned)PduInfo->id, (unsigned)PduInfo->length,
              PduInfo->sdu, (unsigned)PduInfo->length);
    }
    return ret;
}

void Can_DisableControllerInterrupts(uint8 Controller)
{
    if (can_cfg == NULL_PTR) {
        CAN_DET_ERROR(CAN_SID_DISABLE_INT, CAN_E_UNINIT);
        return;
    }
    if (Controller != CAN_CONTROLLER_ID) {
        CAN_DET_ERROR(CAN_SID_DISABLE_INT, CAN_E_PARAM_CONTROLLER);
        return;
    }
    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
    if (can_irqDisableNesting < 255u) {
        can_irqDisableNesting++;
    }
    if (can_cfg->rxInterrupt) {
        CanHw_RxIrqEnable(FALSE);
    }
    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
}

void Can_EnableControllerInterrupts(uint8 Controller)
{
    if (can_cfg == NULL_PTR) {
        CAN_DET_ERROR(CAN_SID_ENABLE_INT, CAN_E_UNINIT);
        return;
    }
    if (Controller != CAN_CONTROLLER_ID) {
        CAN_DET_ERROR(CAN_SID_ENABLE_INT, CAN_E_PARAM_CONTROLLER);
        return;
    }
    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
    if (can_irqDisableNesting > 0u) {
        can_irqDisableNesting--;
    }
    if ((can_irqDisableNesting == 0u) && can_cfg->rxInterrupt) {
        CanHw_RxIrqEnable(TRUE);
    }
    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
}

void Can_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo == NULL_PTR) {
        CAN_DET_ERROR(CAN_SID_GETVERSIONINFO, CAN_E_PARAM_POINTER);
        return;
    }
    versioninfo->vendorID         = MINI_VENDOR_ID;
    versioninfo->moduleID         = MINI_MODULE_CAN;
    versioninfo->sw_major_version = 1u;
    versioninfo->sw_minor_version = 0u;
    versioninfo->sw_patch_version = 0u;
}

/* ------------------------------------------------------------------------------------------------ scheduled functions */
void Can_MainFunction_Write(void)                          /* SWS_Can_00225: polling of TX confirmation */
{
    uint8 i;
    if (can_cfg == NULL_PTR) {
        return;
    }
    for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {
        PduIdType handle = 0u;
        boolean   done   = FALSE;
        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        if (can_tx[i].used && CanHw_TxIsDone(i)) {
            handle         = can_tx[i].swPduHandle;
            can_tx[i].used = FALSE;                        /* HTH free again before the upcall (CanIf may transmit at once) */
            done           = TRUE;
        }
        SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
        if (done) {
            CanIf_TxConfirmation(handle);
        }
    }
}

void Can_MainFunction_Read(void)                           /* SWS_Can_00226: only meaningful in polling mode */
{
    if ((can_cfg == NULL_PTR) || can_cfg->rxInterrupt || (can_state != CAN_CS_STARTED)) {
        return;
    }
    CanHw_RxIrqAck();                                      /* polling mode: flags are not consumed by an ISR */
    can_rx_drain();
}

void Can_MainFunction_BusOff(void)                         /* SWS_Can_00227 */
{
    if ((can_cfg == NULL_PTR) || (can_state != CAN_CS_STARTED)) {
        return;
    }
    if (CanHw_IsBusOff()) {
        if (!can_busOffReported) {
            can_busOffReported = TRUE;
            can_state          = CAN_CS_STOPPED;           /* hardware entered init mode by itself */
            TRACE(TRACE_CAT_CAN, "BUSOFF");
            CanIf_ControllerBusOff(CAN_CONTROLLER_ID);     /* the upper layer decides about recovery (CanSM in a real stack) */
        }
    }
}

void Can_MainFunction_Mode(void)                           /* SWS_Can_00368: mode changes are synchronous in this driver */
{
    /* nothing to poll: CanHw_Start/Stop wait for the hardware and Can_SetControllerMode already indicated the new mode */
}

void Can_Isr_Rx(void)                                      /* body of the Cat2 ISR bound to FDCAN1_IT0 (IRQ 39) */
{
    if (can_cfg == NULL_PTR) {
        return;
    }
    CanHw_RxIrqAck();                                      /* ack first: a frame arriving while draining raises the line again */
    can_rx_drain();
}
