// SOURCES: ecual/canif/CanIf.c
/*
 * test_canif_routing.c
 *
 * [Educational Implementation]
 * Unit test of the CAN Interface (host build). CanIf.c is linked unmodified; the CAN driver (Can_Write, Can_SetControllerMode) and
 * the PDU router (PduR_CanIfRxIndication, PduR_CanIfTxConfirmation) are recording stubs defined below, with a hand written L-PDU
 * table. Real AUTOSAR counterpart: CanIf module test (SWS_CANIF_00005 CanIf_Transmit, 00006 CanIf_RxIndication, 00007
 * CanIf_TxConfirmation, 00003 CanIf_SetControllerMode, 00218 CanIf_ControllerBusOff).
 * Checks: Det errors, controller mode handling (mode known only after CanIf_ControllerModeIndication), TxPdu -> HTH/id/DLC mapping,
 * CAN_BUSY / CAN_NOT_OK mapping to E_NOT_OK, length checks, RxPdu lookup by CAN id + HRH, routing to PduR with the configured upper
 * PDU id, short frames dropped with a Det runtime error, TX confirmation routing, bus-off recovery, trace lines.
 */
#include "mcal_test_support.h"
#include "CanIf.h"
#include "CanIf_Cbk.h"
#include "Can.h"
#include "PduR.h"

/* ------------------------------------------------------------------------------------------------ stubs below CanIf */
static int              w_count;
static Can_HwHandleType w_hth;
static Can_PduType      w_pdu;
static uint8            w_data[8];
static Can_ReturnType   w_result = CAN_OK;
static int              scm_start, scm_stop;
static Std_ReturnType   scm_result = E_OK;

Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    w_count++;
    w_hth = Hth;
    w_pdu = *PduInfo;
    memcpy(w_data, PduInfo->sdu, PduInfo->length);
    return w_result;
}
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    (void)Controller;
    if (scm_result != E_OK) { return scm_result; }
    if (Transition == CAN_T_START) { scm_start++; CanIf_ControllerModeIndication(0u, CAN_CS_STARTED); }
    else                           { scm_stop++;  CanIf_ControllerModeIndication(0u, CAN_CS_STOPPED); }
    return E_OK;
}

/* ------------------------------------------------------------------------------------------------ stubs above CanIf */
static int       rxi_count;
static PduIdType rxi_id;
static PduLengthType rxi_len;
static uint8     rxi_data[8];
static int       txc_count;
static PduIdType txc_id;
static Std_ReturnType txc_result;

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    rxi_count++; rxi_id = RxPduId; rxi_len = PduInfoPtr->SduLength;
    memcpy(rxi_data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
}
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result) { txc_count++; txc_id = TxPduId; txc_result = result; }

/* ------------------------------------------------------------------------------------------------ L-PDU tables */
static const CanIf_TxPduConfigType txp[] = {
    { 0x201u, 3u, 0u, 6u, 4u },        /* LightStatus: id, HTH 3, controller 0, DLC 6, PduR tx path 4 */
    { 0x101u, 5u, 0u, 2u, 9u },
};
static const CanIf_RxPduConfigType rxp[] = {
    { 0x101u, 0u, 2u, 11u },           /* VehicleSpeed: id, HRH 0, DLC 2, PduR rx path 11 */
    { 0x301u, 1u, 1u, 12u },
};
static const CanIf_ConfigType cfg = { 2u, txp, 2u, rxp };

static Can_HwType mb(Can_IdType id, Can_HwHandleType hoh) { Can_HwType m; m.CanId = id; m.Hoh = hoh; m.ControllerId = 0u; return m; }

int main(void)
{
    uint8 sdu[8] = { 0xE8u, 0x03u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u };
    PduInfoType pdu;
    PduInfoType rxpdu;
    Can_ControllerStateType m = CAN_CS_UNINIT;
    int c;

    pdu.SduDataPtr = sdu; pdu.MetaDataPtr = NULL_PTR; pdu.SduLength = 6u;

    /* ---- not initialised ---- */
    T_CHECK(CanIf_Transmit(0u, &pdu) == E_NOT_OK);
    T_CHECK((ts_det_count == 1) && (ts_det_last_module == MINI_MODULE_CANIF) && (ts_det_last_error == 30u));   /* CANIF_E_UNINIT */
    T_CHECK(CanIf_SetControllerMode(0u, CAN_CS_STARTED) == E_NOT_OK);
    T_CHECK(ts_det_count == 2);
    CanIf_Init(NULL_PTR);
    T_CHECK((ts_det_count == 3) && (ts_det_last_error == 20u));                                               /* CANIF_E_PARAM_POINTER */

    /* ---- init: controller STOPPED ---- */
    CanIf_Init(&cfg);
    T_CHECK(CanIf_GetControllerMode(0u, &m) == E_OK);
    T_CHECK(m == CAN_CS_STOPPED);
    T_CHECK(CanIf_Transmit(0u, &pdu) == E_NOT_OK);                     /* not started: no Can_Write */
    T_CHECK(w_count == 0);
    T_CHECK(CanIf_GetControllerMode(1u, &m) == E_NOT_OK);
    T_CHECK((ts_det_count == 4) && (ts_det_last_error == 15u));        /* CANIF_E_PARAM_CONTROLLERID */
    T_CHECK(CanIf_GetControllerMode(0u, NULL_PTR) == E_NOT_OK);
    T_CHECK((ts_det_count == 5) && (ts_det_last_error == 20u));

    /* ---- controller mode ---- */
    T_CHECK(CanIf_SetControllerMode(1u, CAN_CS_STARTED) == E_NOT_OK);
    T_CHECK((ts_det_count == 6) && (ts_det_last_error == 15u));
    T_CHECK(CanIf_SetControllerMode(0u, CAN_CS_SLEEP) == E_NOT_OK);
    T_CHECK((ts_det_count == 7) && (ts_det_last_error == 21u));        /* CANIF_E_PARAM_CTRLMODE */
    scm_result = E_NOT_OK;
    T_CHECK(CanIf_SetControllerMode(0u, CAN_CS_STARTED) == E_NOT_OK);  /* driver refuses: mode unchanged */
    (void)CanIf_GetControllerMode(0u, &m);
    T_CHECK(m == CAN_CS_STOPPED);
    scm_result = E_OK;
    ts_trace_clear();
    T_CHECK(CanIf_SetControllerMode(0u, CAN_CS_STARTED) == E_OK);
    T_CHECK(scm_start == 1);
    (void)CanIf_GetControllerMode(0u, &m);
    T_CHECK(m == CAN_CS_STARTED);                                      /* set by CanIf_ControllerModeIndication */
    T_CHECK(ts_trace_has("MODE ctrl=0 mode=1"));
    T_CHECK(CanIf_SetControllerMode(0u, CAN_CS_STARTED) == E_OK);      /* already started: no second driver call */
    T_CHECK(scm_start == 1);

    /* ---- CanIf_Transmit ---- */
    c = ts_det_count;
    T_CHECK(CanIf_Transmit(2u, &pdu) == E_NOT_OK);                     /* invalid L-PDU id */
    T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 50u));    /* CANIF_E_INVALID_TXPDUID */
    T_CHECK(CanIf_Transmit(0u, NULL_PTR) == E_NOT_OK);
    T_CHECK((ts_det_count == c + 2) && (ts_det_last_error == 20u));
    pdu.SduLength = 7u;                                                /* longer than the configured DLC 6 */
    T_CHECK(CanIf_Transmit(0u, &pdu) == E_NOT_OK);
    T_CHECK((ts_det_runtime_count == 1) && (ts_det_last_error == 62u));   /* CANIF_E_DATA_LENGTH_MISMATCH */
    T_CHECK(w_count == 0);
    pdu.SduLength = 6u;
    ts_trace_clear();
    T_CHECK(CanIf_Transmit(0u, &pdu) == E_OK);
    T_CHECK(w_count == 1);
    T_CHECK((w_hth == 3u) && (w_pdu.id == 0x201u) && (w_pdu.length == 6u) && (w_pdu.swPduHandle == 0u));
    T_CHECK(memcmp(w_data, sdu, 6u) == 0);
    T_CHECK(ts_trace_has("TX pdu=0"));
    pdu.SduLength = 2u;
    T_CHECK(CanIf_Transmit(1u, &pdu) == E_OK);                         /* shorter than DLC is allowed, sent as given */
    T_CHECK((w_hth == 5u) && (w_pdu.id == 0x101u) && (w_pdu.length == 2u) && (w_pdu.swPduHandle == 1u));
    w_result = CAN_BUSY;
    T_CHECK(CanIf_Transmit(1u, &pdu) == E_NOT_OK);                     /* no queue: BUSY is reported upwards */
    w_result = CAN_NOT_OK;
    T_CHECK(CanIf_Transmit(1u, &pdu) == E_NOT_OK);
    w_result = CAN_OK;

    /* ---- TX confirmation routing ---- */
    CanIf_TxConfirmation(0u);
    T_CHECK((txc_count == 1) && (txc_id == 4u) && (txc_result == E_OK));    /* upperPduId of TxPdu 0 */
    CanIf_TxConfirmation(1u);
    T_CHECK((txc_count == 2) && (txc_id == 9u));
    c = ts_det_count;
    CanIf_TxConfirmation(2u);
    T_CHECK((txc_count == 2) && (ts_det_count == c + 1) && (ts_det_last_error == 50u));

    /* ---- RX routing ---- */
    rxpdu.SduDataPtr = sdu; rxpdu.MetaDataPtr = NULL_PTR; rxpdu.SduLength = 2u;
    {
        Can_HwType h = mb(0x101u, 0u);
        ts_trace_clear();
        CanIf_RxIndication(&h, &rxpdu);
        T_CHECK((rxi_count == 1) && (rxi_id == 11u) && (rxi_len == 2u) && (rxi_data[0] == 0xE8u) && (rxi_data[1] == 0x03u));
        T_CHECK(ts_trace_has("RX pdu=0"));
        h = mb(0x301u, 1u);
        rxpdu.SduLength = 8u;                                          /* longer than DLC: passed on with its real length */
        CanIf_RxIndication(&h, &rxpdu);
        T_CHECK((rxi_count == 2) && (rxi_id == 12u) && (rxi_len == 8u));
        T_CHECK(ts_trace_has("RX pdu=1"));

        c = rxi_count;
        h = mb(0x555u, 0u);                                            /* unknown id: ignored */
        CanIf_RxIndication(&h, &rxpdu);
        h = mb(0x101u, 1u);                                            /* right id, wrong HRH: ignored */
        CanIf_RxIndication(&h, &rxpdu);
        T_CHECK(rxi_count == c);
        T_CHECK(ts_det_runtime_count == 1);                            /* only the earlier length mismatch */

        rxpdu.SduLength = 1u;                                          /* 0x101 needs DLC 2: dropped + runtime error */
        h = mb(0x101u, 0u);
        CanIf_RxIndication(&h, &rxpdu);
        T_CHECK(rxi_count == c);
        T_CHECK((ts_det_runtime_count == 2) && (ts_det_last_error == 61u));    /* CANIF_E_INVALID_DATA_LENGTH */

        c = ts_det_count;
        CanIf_RxIndication(NULL_PTR, &rxpdu);
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 20u));
        h.ControllerId = 1u;
        CanIf_RxIndication(&h, &rxpdu);
        T_CHECK((ts_det_count == c + 2) && (ts_det_last_error == 15u));
    }

    /* ---- bus-off: STOPPED, then restarted (simplified recovery) ---- */
    scm_start = 0;
    CanIf_ControllerBusOff(0u);
    T_CHECK(scm_start == 1);
    (void)CanIf_GetControllerMode(0u, &m);
    T_CHECK(m == CAN_CS_STARTED);
    T_CHECK(ts_trace_has("MODE ctrl=0 mode=2"));                       /* STOPPED indicated on the way */
    c = ts_det_count;
    CanIf_ControllerBusOff(3u);
    T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 15u));

    /* ---- stop and version ---- */
    T_CHECK(CanIf_SetControllerMode(0u, CAN_CS_STOPPED) == E_OK);
    T_CHECK(scm_stop == 1);
    c = w_count;
    T_CHECK(CanIf_Transmit(0u, &pdu) == E_NOT_OK);
    T_CHECK(w_count == c);
    {
        Std_VersionInfoType vi;
        CanIf_GetVersionInfo(&vi);
        T_CHECK(vi.moduleID == MINI_MODULE_CANIF);
    }
    T_DONE();
}
