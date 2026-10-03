/*
 * Can_Hw.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none. AUTOSAR leaves the inside of a CAN driver to the MCAL vendor; this PRIVATE header is the seam
 * between the hardware-independent part (Can.c: HOH handling, state machine, upcalls to CanIf, Det, trace) and one of two
 * hardware backends:
 *   mcal/can/Can_Hw_Stm32.c   (target)  STM32L552 FDCAN1 registers + message RAM, verified against Renode's STM32_FDCAN model
 *   sim/host/Can_Hw_Sim.c     (host)    behavioural model: 3 TX buffers, RX FIFO0 (depth 3), SimCan file bus
 * Real counterpart of the FDCAN peripheral on the Renesas RH850 is the RS-CANFD macro (see docs/04-can-mcal/02-rh850-can-peripheral.md:
 * RS-CANFD global/channel registers ~ FDCAN CCCR/NBTP, RX rules ~ standard ID filters, RX FIFO ~ Rx FIFO0, TX message buffers ~
 * FDCAN Tx buffers/queue, "channel stop/reset/communication mode" ~ CCCR.INIT).
 * Only Can.c includes this header. Owner: agent B.
 */
#ifndef CAN_HW_H
#define CAN_HW_H

#include "Can.h"

#define CAN_HW_TX_BUFFERS   3u        /* FDCAN message RAM on STM32L5/G4: 3 TX elements (fixed) */
#define CAN_HW_RX_FIFO_SIZE 3u        /* FDCAN RX FIFO0: 3 elements (fixed) */

typedef struct {
    Can_IdType id;                    /* 11 bit standard id */
    uint8      dlc;                   /* 0..8 */
    uint8      data[8];
} CanHw_FrameType;

/* Program bit timing, filters (one standard-id classic filter per CAN_HOH_RECEIVE object of cfg), RX FIFO0 and the
 * interrupt lines; leaves the controller STOPPED (init mode). Returns E_NOT_OK if the hardware does not respond. */
Std_ReturnType CanHw_Init(const Can_ConfigType *cfg);
/* STOPPED -> STARTED: bounded wait until the controller is operational (SWS_Can_00262 / SWS_Can_00398). */
Std_ReturnType CanHw_Start(void);
/* STARTED -> STOPPED: pending transmit requests are cancelled. */
Std_ReturnType CanHw_Stop(void);

/* Put one frame in a free hardware TX buffer and request transmission. E_NOT_OK = no free buffer (=> CAN_BUSY). */
Std_ReturnType CanHw_TxRequest(Can_IdType id, uint8 dlc, const uint8 *data, uint8 *bufIdx);
/* TRUE exactly once after the frame requested in buffer bufIdx has left the controller (clears the done indication). */
boolean        CanHw_TxIsDone(uint8 bufIdx);

/* Pop one frame from RX FIFO0 and acknowledge it. FALSE = FIFO empty. */
boolean        CanHw_RxFetch(CanHw_FrameType *frame);
/* Acknowledge the RX interrupt flags (call BEFORE draining the FIFO so a frame arriving meanwhile re-triggers). */
void           CanHw_RxIrqAck(void);
void           CanHw_RxIrqEnable(boolean enable);

boolean        CanHw_IsBusOff(void);
uint32         CanHw_GetRxLostCount(void);       /* frames lost because RX FIFO0 was full (educational counter) */

#endif /* CAN_HW_H */
