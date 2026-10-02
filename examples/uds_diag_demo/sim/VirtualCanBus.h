/*
 * VirtualCanBus.h
 *
 * [Educational Implementation]
 * Real counterpart: the physical CAN bus + the RS-CANFD peripheral's
 * message RAM on RH850 + a PC CAN interface (Vector/PEAK) used by the tester.
 *
 * Two kinds of API exist and they must not be mixed up:
 *   - VirtualCanBus_Hw*     : "register level" view, used ONLY by the Can
 *                             driver mock (mcal/Can.c). They stand in for
 *                             RS-CANFD receive rules (GAFL), the shared RX FIFO
 *                             and a transmit buffer (TMIDp/TMDFp/TMCp/TMSTSp).
 *   - VirtualCanBus_Tester* : the PC side (tester tool) that injects request
 *                             frames and captures ECU response frames.
 *
 * Nothing above the Can driver may call VirtualCanBus_Hw*: in a real ECU only
 * the Can MCAL touches the peripheral (SWS_Can_00058 / SRS_SPAL_12092).
 */
#ifndef VIRTUAL_CAN_BUS_H
#define VIRTUAL_CAN_BUS_H

#include "Std_Types.h"

typedef struct {
    uint32 id;       /* 11-bit standard identifier (extended IDs not used) */
    uint8  dlc;      /* 0..8 (classical CAN only in this demo)             */
    uint8  data[8];
} VirtualCanBus_FrameType;

#define VCAN_RX_FIFO_DEPTH    16u   /* models one RS-CANFD RX FIFO (RFCCx.RFDC) */
#define VCAN_NUM_RX_RULES      4u   /* models receive rule table entries (GAFL) */
#define VCAN_NUM_TX_BUFFERS    4u   /* models TX buffers TMp                    */
#define VCAN_TESTER_RX_DEPTH  64u

void VirtualCanBus_Reset(void);

/* ---- hardware view (Can driver only) -------------------------------- */
/* Program one acceptance rule. label is returned with every frame matched
 * by this rule (RS-CANFD: receive rule pointer GAFLPTR -> read back with the
 * frame from the RX FIFO pointer register; confirm field names in the HW manual). */
void VirtualCanBus_HwSetRxRule(uint8 rule, uint32 code, uint32 mask, uint16 label);
void VirtualCanBus_HwSetOnline(boolean online);   /* channel communication mode */
boolean VirtualCanBus_HwRxPending(void);          /* RX FIFO interrupt request flag */
boolean VirtualCanBus_HwRxFifoRead(VirtualCanBus_FrameType *frame, uint16 *label);
Std_ReturnType VirtualCanBus_HwTxBufferWrite(uint8 txBuffer, const VirtualCanBus_FrameType *frame);
boolean VirtualCanBus_HwTxBufferBusy(uint8 txBuffer);
boolean VirtualCanBus_HwTxCompleteFlag(uint8 txBuffer);  /* reads and clears */

/* ---- bus timing ---------------------------------------------------- */
/* One simulated millisecond of bus activity: every requested TX buffer is
 * put on the wire (captured for the tester) and its completion flag is set. */
void VirtualCanBus_Tick(void);

/* ---- PC tester view ------------------------------------------------- */
void VirtualCanBus_TesterSend(uint32 id, const uint8 *data, uint8 dlc);
boolean VirtualCanBus_TesterReceive(VirtualCanBus_FrameType *frame);

/* statistics for tests */
uint32 VirtualCanBus_GetEcuTxCount(void);
uint32 VirtualCanBus_GetDroppedCount(void);

#endif /* VIRTUAL_CAN_BUS_H */
