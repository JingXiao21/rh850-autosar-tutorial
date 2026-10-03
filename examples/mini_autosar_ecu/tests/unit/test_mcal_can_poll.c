// SOURCES: mcal/can/Can.c sim/host/Can_Hw_Sim.c sim/host/SimCan.c
/*
 * test_mcal_can_poll.c
 *
 * [Educational Implementation]
 * Unit test of the Can MCAL driver in POLLING receive mode (Can_Config.rxInterrupt = FALSE): same Can.c and host backend as
 * test_mcal_can.c, but frames are fetched by Can_MainFunction_Read instead of the RX interrupt. Real AUTOSAR counterpart: CanDriver
 * polling operation (CanRxProcessing = POLLING, SWS_Can_00226 Can_MainFunction_Read). Checks: no ISR is ever raised, the
 * frames wait in the 3-deep FIFO until the main function runs, Can_MainFunction_Read is a no-op while STOPPED, frames arriving
 * while the controller is STOPPED are lost, Enable/DisableControllerInterrupts do not enable an interrupt that is configured off.
 * Separate program because Can_Init can be called only once per process (SWS_Can_00103 CAN_UNINIT precondition).
 */
#include "mcal_test_support.h"
#include "Can.h"
#include "CanIf_Cbk.h"
#include "SimCan.h"

typedef struct { Can_IdType id; Can_HwHandleType hoh; uint8 len; uint8 data[8]; } rx_rec;
static rx_rec rx[8];
static int    rx_count;

void CanIf_RxIndication(const Can_HwType *mb, const PduInfoType *pdu)
{
    if (rx_count < 8) {
        rx[rx_count].id = mb->CanId; rx[rx_count].hoh = mb->Hoh; rx[rx_count].len = (uint8)pdu->SduLength;
        memcpy(rx[rx_count].data, pdu->SduDataPtr, pdu->SduLength);
    }
    rx_count++;
}
void CanIf_TxConfirmation(PduIdType h) { (void)h; }
void CanIf_ControllerBusOff(uint8 c) { (void)c; }
void CanIf_ControllerModeIndication(uint8 c, Can_ControllerStateType m) { (void)c; (void)m; }

static const Can_HohConfigType hoh[] = {
    { 0u, CAN_HOH_RECEIVE,  0x100u, 0x7F0u, 0u },                      /* accepts 0x100..0x10F (mask) */
    { 1u, CAN_HOH_TRANSMIT, 0x201u, 0x7FFu, 0u },
};
static const Can_ConfigType cfg = { 500000u, FALSE, 2u, hoh };

int main(void)
{
    uint8 d[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    int c;

    SimCan_Init(NULL_PTR, NULL_PTR);
    Can_Init(&cfg);
    T_CHECK(SimCan_Inject(0x105u, 8u, d) == FALSE);                    /* STOPPED: frame lost */
    T_CHECK(Can_SetControllerMode(0u, CAN_T_START) == E_OK);

    T_CHECK(SimCan_Inject(0x105u, 8u, d) == TRUE);
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == 0);                /* polling: no interrupt */
    T_CHECK(rx_count == 0);
    Can_MainFunction_Read();
    T_CHECK(rx_count == 1);
    T_CHECK((rx[0].id == 0x105u) && (rx[0].hoh == 0u) && (rx[0].len == 8u) && (memcmp(rx[0].data, d, 8u) == 0));
    T_CHECK(ts_trace_has("RX id=0x105 dlc=8 data=01 02 03 04 05 06 07 08"));

    /* the acceptance mask lets 0x100..0x10F in, everything else is rejected */
    T_CHECK(SimCan_Inject(0x10Fu, 0u, d) == TRUE);
    T_CHECK(SimCan_Inject(0x110u, 1u, d) == TRUE);
    Can_MainFunction_Read();
    T_CHECK(rx_count == 2);
    T_CHECK((rx[1].id == 0x10Fu) && (rx[1].len == 0u));

    /* interrupt enable calls must not turn on an interrupt configured off */
    Can_DisableControllerInterrupts(0u);
    Can_EnableControllerInterrupts(0u);
    T_CHECK(SimCan_Inject(0x101u, 1u, d) == TRUE);
    T_CHECK(ts_irq_raised[SIMCAN_IRQ_FDCAN1_IT0] == 0);
    Can_MainFunction_Read();
    T_CHECK(rx_count == 3);

    /* frames wait in the FIFO (depth 3) while the main function does not run: 4th is lost */
    c = rx_count;
    T_CHECK(SimCan_Inject(0x101u, 1u, d) == TRUE);
    T_CHECK(SimCan_Inject(0x102u, 1u, d) == TRUE);
    T_CHECK(SimCan_Inject(0x103u, 1u, d) == TRUE);
    T_CHECK(SimCan_Inject(0x104u, 1u, d) == FALSE);
    Can_MainFunction_Read();
    T_CHECK(rx_count == c + 3);
    T_CHECK((rx[c].id == 0x101u) && (rx[c + 2].id == 0x103u));         /* FIFO order */

    /* STOPPED: main function does nothing */
    T_CHECK(Can_SetControllerMode(0u, CAN_T_STOP) == E_OK);
    c = rx_count;
    Can_MainFunction_Read();
    T_CHECK(rx_count == c);
    T_CHECK(ts_os_nesting == 0);
    T_DONE();
}
