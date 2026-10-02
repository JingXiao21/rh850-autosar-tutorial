/*
 * Can_GeneralTypes.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Can_GeneralTypes.h, shared by Can / CanIf / CanTrcv
 * (CAN SWS R22-11, SWS_Can_00436 p.24; types p.57-61).
 */
#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#include "ComStack_Types.h"

/* [AUTOSAR API] SWS_Can_00416: uint32, the two MSBs encode the frame type
 * (00 = standard CAN, 01 = standard CAN FD, 10 = extended CAN, 11 = extended CAN FD). */
typedef uint32 Can_IdType;

/* [AUTOSAR API] SWS_Can_00429: uint8 or uint16 depending on the HOH count. */
typedef uint16 Can_HwHandleType;

/* [AUTOSAR API] SWS_Can_00415: argument of Can_Write. */
typedef struct {
    PduIdType  swPduHandle;  /* CanIf L-PDU handle, echoed in CanIf_TxConfirmation */
    uint8      length;
    Can_IdType id;
    uint8     *sdu;
} Can_PduType;

/* [AUTOSAR API] SWS_CAN_00496: "Mailbox" argument of CanIf_RxIndication. */
typedef struct {
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

/* [AUTOSAR API] SWS_Can_91013 (introduced in 4.3.0). */
typedef enum {
    CAN_CS_UNINIT  = 0x00,
    CAN_CS_STARTED = 0x01,
    CAN_CS_STOPPED = 0x02,
    CAN_CS_SLEEP   = 0x03
} Can_ControllerStateType;

/* [AUTOSAR API] SWS_Can_91003. */
typedef enum {
    CAN_ERRORSTATE_ACTIVE = 0,
    CAN_ERRORSTATE_PASSIVE,
    CAN_ERRORSTATE_BUSOFF
} Can_ErrorStateType;

/* [AUTOSAR API] SWS_Can_00039: Std_ReturnType extension used by Can_Write only.
 * CAN_BUSY is not an error: CanIf is expected to buffer and retry. */
#define CAN_BUSY ((Std_ReturnType)0x02u)

#endif /* CAN_GENERAL_TYPES_H */
