// SOURCES: mcal/can/Can.c sim/host/Can_Hw_Sim.c sim/host/SimCan.c
/*
 * test_mcal_can.c
 *
 * [Educational Implementation]
 * Unit test of the Can MCAL driver (interrupt RX mode) on the HOST build: the unmodified hardware-independent Can.c runs on the host
 * backend Can_Hw_Sim.c and the file based virtual bus SimCan.c; CanIf, Det, SchM/OS and SimTime are recording stubs
 * (mcal_test_support.h). Real AUTOSAR counterpart: CAN driver module test (SWS_Can_00223 Can_Init, 00230 Can_SetControllerMode,
 * 00233 Can_Write, 00225 Can_MainFunction_Write, RX indication SWS_Can_00279/CanIf_RxIndication).
 * Checks: Det errors, STOPPED/STARTED state machine incl. invalid transitions and CanIf_ControllerModeIndication, Can_Write
 * (HTH checks, CAN_BUSY on a busy HTH, TX log line, trace line), TX confirmation by Can_MainFunction_Write with the swPduHandle,
 * RX via IRQ 39 (ISR raised, Can_Isr_Rx drains), hardware filter, HRH lookup, FIFO overflow = lost frame + runtime error,
 * Disable/EnableControllerInterrupts nesting, RX script file with timestamps, SchM exclusive-area balance.
 */
#include "mcal_test_support.h"
#include "Can.h"
#include "CanIf_Cbk.h"
#include "SimCan.h"

/* ------------------------------------------------------------------------------------------------ CanIf stubs */
typedef struct { Can_IdType id; Can_HwHandleType hoh; uint8 ctrl; uint8 len; uint8 data[8]; } rx_rec;
static rx_rec  rx[16];
static int     rx_count;
static int     txconf_count;
static PduIdType txconf_last;
static Can_ControllerStateType mode_last = CAN_CS_UNINIT;
static int     mode_ind_count;
static int     busoff_count;

void CanIf_RxIndication(const Can_HwType *mb, const PduInfoType *pdu)
{
    if (rx_count < 16) {
        rx[rx_count].id = mb->CanId; rx[rx_count].hoh = mb->Hoh; rx[rx_count].ctrl = mb->ControllerId;
        rx[rx_count].len = (uint8)pdu->SduLength;
        memcpy(rx[rx_count].data, pdu->SduDataPtr, pdu->SduLength);
    }
    rx_count++;
}
void CanIf_TxConfirmation(PduIdType h) { txconf_count++; txconf_last = h; }
void CanIf_ControllerBusOff(uint8 c) { (void)c; busoff_count++; }
void CanIf_ControllerModeIndication(uint8 c, Can_ControllerStateType m) { (void)c; mode_last = m; mode_ind_count++; }

/* ------------------------------------------------------------------------------------------------ configuration */
static const Can_HohConfigType hoh[] = {
    { 0u, CAN_HOH_RECEIVE,  0x101u, 0x7FFu, 0u },
    { 1u, CAN_HOH_RECEIVE,  0x301u, 0x7FFu, 0u },
    { 2u, CAN_HOH_RECEIVE,  0x3F0u, 0x7FFu, 0u },
    { 3u, CAN_HOH_TRANSMIT, 0x201u, 0x7FFu, 0u },
};
static const Can_ConfigType cfg = { 500000u, TRUE, 4u, hoh };

static Can_ReturnType write(Can_HwHandleType hth, PduIdType handle, Can_IdType id, uint8 len, uint8 *sdu)
{
    Can_PduType p;
    p.swPduHandle = handle; p.length = len; p.id = id; p.sdu = sdu;
    return Can_Write(hth, &p);
}

int main(void)
{
    uint8 d[8] = { 0xE8u, 0x03u, 0u, 0u, 0u, 0u, 0u, 0u };
    uint8 f[8];
    char  text[2048];
    const char *txlog = ts_tmpfile("mini_autosar_can_tx.log");
    const char *script = ts_tmpfile("mini_autosar_can_rx.txt");
    int c;

    /* ---- not initialised ---- */
    T_CHECK(write(3u, 1u, 0x201u, 2u, d) == CAN_NOT_OK);
    T_CHECK((ts_det_count == 1) && (ts_det_last_module == MINI_MODULE_CAN) && (ts_det_last_error == 0x05u));   /* CAN_E_UNINIT */
    T_CHECK(Can_SetControllerMode(0u, CAN_T_START) == E_NOT_OK);
    T_CHECK((ts_det_count == 2) && (ts_det_last_error == 0x05u));
    Can_Init(NULL_PTR);
    T_CHECK((ts_det_count == 3) && (ts_det_last_error == 0x01u));                                             /* CAN_E_PARAM_POINTER */

    /* ---- init ---- */
    SimCan_Init(txlog, NULL_PTR);
    Can_Init(&cfg);
    T_CHECK(ts_det_count == 3);
    Can_Init(&cfg);
    T_CHECK((ts_det_count == 4) && (ts_det_last_error == 0x06u));                                             /* CAN_E_TRANSITION */
    T_CHECK(write(3u, 1u, 0x201u, 2u, d) == CAN_NOT_OK);              /* STOPPED: nothing is sent */
    T_CHECK(ts_det_count == 4);
    T_CHECK(SimCan_GetTxCount() == 0u);
    T_CHECK(SimCan_Inject(0x101u, 2u, d) == FALSE);                    /* controller in init mode does not receive */
    T_CHECK(SimCan_GetDroppedCount() == 1u);

    /* ---- state machine ---- */
    T_CHECK(Can_SetControllerMode(1u, CAN_T_START) == E_NOT_OK);       /* only controller 0 exists */
    T_CHECK((ts_det_count == 5) && (ts_det_last_error == 0x04u));
    T_CHECK(Can_SetControllerMode(0u, CAN_T_STOP) == E_NOT_OK);        /* STOPPED -> STOPPED is invalid */
    T_CHECK((ts_det_count == 6) && (ts_det_last_error == 0x06u));
    T_CHECK(Can_SetControllerMode(0u, CAN_T_START) == E_OK);
    T_CHECK((mode_last == CAN_CS_STARTED) && (mode_ind_count == 1));
    T_CHECK(Can_SetControllerMode(0u, CAN_T_START) == E_NOT_OK);       /* SWS_Can_00409 */
    T_CHECK((ts_det_count == 7) && (ts_det_last_error == 0x06u));
    T_CHECK(Can_SetControllerMode(0u, CAN_T_SLEEP) == E_NOT_OK);       /* sleep is not implemented */
    T_CHECK(ts_det_count == 8);

    /* ---- Can_Write parameter checks ---- */
    c = ts_det_count;
    T_CHECK(write(0u, 1u, 0x101u, 2u, d) == CAN_NOT_OK);               /* HOH 0 is a receive object */
    T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x02u));  /* CAN_E_PARAM_HANDLE */
    T_CHECK(write(9u, 1u, 0x101u, 2u, d) == CAN_NOT_OK);
    T_CHECK((ts_det_count == c + 2) && (ts_det_last_error == 0x02u));
    T_CHECK(Can_Write(3u, NULL_PTR) == CAN_NOT_OK);
    T_CHECK((ts_det_count == c + 3) && (ts_det_last_error == 0x01u));
    T_CHECK(write(3u, 1u, 0x201u, 9u, d) == CAN_NOT_OK);               /* classic CAN: max 8 bytes */
    T_CHECK((ts_det_count == c + 4) && (ts_det_last_error == 0x03u));
    T_CHECK(write(3u, 1u, 0x201u | CAN_ID_EXTENDED_FLAG, 2u, d) == CAN_NOT_OK);
    T_CHECK(write(3u, 1u, 0x800u, 2u, d) == CAN_NOT_OK);               /* > 11 bit */
    T_CHECK(SimCan_GetTxCount() == 0u);

    /* ---- Can_Write / CAN_BUSY / TX confirmation ---- */
    ts_now_us = 20000u;
    ts_trace_clear();
    T_CHECK(write(3u, 5u, 0x101u, 2u, d) == CAN_OK);
    T_CHECK(ts_trace_has("TX id=0x101 dlc=2 data=e8 03"));
    T_CHECK(SimCan_GetTxCount() == 1u);
    T_CHECK(write(3u, 6u, 0x101u, 2u, d) == CAN_BUSY);                 /* HTH still waiting for its confirmation (SWS_Can_00213) */
    T_CHECK(SimCan_GetTxCount() == 1u);
    T_CHECK(txconf_count == 0);                                        /* confirmation only from Can_MainFunction_Write */
    Can_MainFunction_Write();
    T_CHECK((txconf_count == 1) && (txconf_last == 5u));               /* swPduHandle of the confirmed L-PDU */
    Can_MainFunction_Write();
    T_CHECK(txconf_count == 1);                                        /* exactly once */
    ts_now_us = 40000u;
    T_CHECK(write(3u, 7u, 0x201u, 6u, d) == CAN_OK);                   /* HTH free again */
    Can_MainFunction_Write();
    T_CHECK((txconf_count == 2) && (txconf_last == 7u));
    T_CHECK(ts_os_nesting == 0);                                       /* SchM exclusive areas are balanced */
    SimCan_Deinit();                                                   /* flush + close the TX log */
    (void)ts_read_file(txlog, text, sizeof(text));
    T_CHECK(strstr(text, "20000 101 2 e8 03\n") != NULL);              /* log line = "time_us id dlc data..." */
    T_CHECK(strstr(text, "40000 201 6 e8 03 00 00 00 00\n") != NULL);

    /* ---- STOP cancels pending requests ---- */
    SimCan_Init(txlog, NULL_PTR);
    T_CHECK(write(3u, 8u, 0x101u, 2u, d) == CAN_OK);
    T_CHECK(Can_SetControllerMode(0u, CAN_T_STOP) == E_OK);
    T_CHECK(mode_last == CAN_CS_STOPPED);
    c = txconf_count;
    Can_MainFunction_Write();
    T_CHECK(txconf_count == c);
    T_CHECK(write(3u, 9u, 0x101u, 2u, d) == CAN_NOT_OK);
    T_CHECK(Can_SetControllerMode(0u, CAN_T_START) == E_OK);
    T_CHECK(write(3u, 9u, 0x101u, 2u, d) == CAN_OK);                   /* HTH was released by STOP */
    Can_MainFunction_Write();

    /* ---- RX by interrupt ---- */
    memset(f, 0, sizeof(f));
    f[0] = 0xE8u; f[1] = 0x03u;
    ts_trace_clear();
    T_CHECK(SimCan_Inject(0x101u, 2u, f) == TRUE);
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == 1);                /* Cat2 ISR requested */
    T_CHECK(rx_count == 0);                                            /* nothing happens before the ISR runs */
    Can_Isr_Rx();
    T_CHECK(rx_count == 1);
    T_CHECK((rx[0].id == 0x101u) && (rx[0].hoh == 0u) && (rx[0].ctrl == 0u) && (rx[0].len == 2u));
    T_CHECK((rx[0].data[0] == 0xE8u) && (rx[0].data[1] == 0x03u));
    T_CHECK(ts_trace_has("RX id=0x101 dlc=2 data=e8 03"));
    Can_Isr_Rx();                                                      /* empty FIFO: nothing more */
    T_CHECK(rx_count == 1);

    T_CHECK(SimCan_Inject(0x7AAu, 1u, f) == TRUE);                     /* filtered by the hardware: no flag, no IRQ */
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == 1);
    Can_Isr_Rx();
    T_CHECK(rx_count == 1);

    f[0] = 0x2Au;
    T_CHECK(SimCan_Inject(0x301u, 1u, f) == TRUE);
    T_CHECK(SimCan_Inject(0x3F0u, 1u, f) == TRUE);
    Can_Isr_Rx();                                                      /* one ISR drains the whole FIFO, in order */
    T_CHECK(rx_count == 3);
    T_CHECK((rx[1].id == 0x301u) && (rx[1].hoh == 1u) && (rx[1].len == 1u) && (rx[1].data[0] == 0x2Au));
    T_CHECK((rx[2].id == 0x3F0u) && (rx[2].hoh == 2u));

    /* ---- FIFO overflow: depth 3, the 4th frame is lost, runtime error CAN_E_DATALOST ---- */
    rx_count = 0;
    T_CHECK(SimCan_Inject(0x101u, 2u, f) == TRUE);
    T_CHECK(SimCan_Inject(0x101u, 2u, f) == TRUE);
    T_CHECK(SimCan_Inject(0x101u, 2u, f) == TRUE);
    T_CHECK(SimCan_Inject(0x101u, 2u, f) == FALSE);
    c = ts_det_runtime_count;
    Can_Isr_Rx();
    T_CHECK(rx_count == 3);
    T_CHECK((ts_det_runtime_count == c + 1) && (ts_det_last_error == 0x01u) && (ts_det_last_module == MINI_MODULE_CAN));

    /* ---- interrupt enable/disable with nesting ---- */
    rx_count = 0;
    c = ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0];
    Can_DisableControllerInterrupts(0u);
    Can_DisableControllerInterrupts(0u);
    T_CHECK(SimCan_Inject(0x101u, 2u, f) == TRUE);
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == c);                /* masked: frame stored, no interrupt */
    Can_EnableControllerInterrupts(0u);
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == c);                /* nesting: still disabled */
    Can_EnableControllerInterrupts(0u);
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == c + 1);            /* pending flag fires when enabled again */
    Can_Isr_Rx();
    T_CHECK(rx_count == 1);

    /* ---- RX script with timestamps (same format as the TX log) ---- */
    {
        FILE *fp = fopen(script, "w");
        T_CHECK(fp != NULL);
        if (fp != NULL) {
            fputs("# comment\n\n2000 301 1 55\n1000 101 2 10 00\nbad line\n2000 3f0 1 01\n3000 7aa 0\n", fp);
            fclose(fp);
        }
    }
    SimCan_Init(NULL_PTR, script);
    T_CHECK(SimCan_GetScriptFrames() == 4u);                           /* comments and malformed lines skipped */
    rx_count = 0;
    SimCan_Step(999u);
    T_CHECK(SimCan_GetInjectedCount() == 0u);
    SimCan_Step(1000u);
    T_CHECK(SimCan_GetInjectedCount() == 1u);
    SimCan_Step(2500u);                                                /* 2000 us frames in file order, sorted by time first */
    T_CHECK(SimCan_GetInjectedCount() == 3u);
    Can_Isr_Rx();
    T_CHECK(rx_count == 3);
    T_CHECK((rx[0].id == 0x101u) && (rx[0].data[0] == 0x10u) && (rx[0].data[1] == 0x00u));
    T_CHECK((rx[1].id == 0x301u) && (rx[1].data[0] == 0x55u));
    T_CHECK((rx[2].id == 0x3F0u) && (rx[2].data[0] == 0x01u));
    SimCan_Step(10000u);                                               /* 0x7AA: filtered */
    T_CHECK(SimCan_GetInjectedCount() == 4u);
    Can_Isr_Rx();
    T_CHECK(rx_count == 3);
    SimCan_Step(20000u);
    T_CHECK(SimCan_GetInjectedCount() == 4u);                          /* every frame only once */

    /* ---- misc ---- */
    {
        Std_VersionInfoType vi;
        Can_GetVersionInfo(&vi);
        T_CHECK((vi.moduleID == MINI_MODULE_CAN) && (vi.vendorID == MINI_VENDOR_ID));
        c = ts_det_count;
        Can_GetVersionInfo(NULL_PTR);
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x01u));
    }
    Can_MainFunction_BusOff();                                         /* host model never goes bus-off */
    T_CHECK(busoff_count == 0);
    Can_MainFunction_Read();                                           /* interrupt mode: must not read */
    Can_MainFunction_Mode();
    T_CHECK(ts_os_nesting == 0);
    SimCan_Deinit();
    T_DONE();
}
