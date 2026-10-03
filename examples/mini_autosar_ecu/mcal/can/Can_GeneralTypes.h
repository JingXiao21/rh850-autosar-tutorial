/*
 * Can_GeneralTypes.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Can_GeneralTypes.h (AUTOSAR_CP_SWS_CANDriver / Can General Types),
 * shared between Can (MCAL) and CanIf. Classic CAN frames only (no FD, no CAN XL).
 */
#ifndef CAN_GENERALTYPES_H
#define CAN_GENERALTYPES_H

#include "ComStack_Types.h"

/* Bit 31 = extended id flag as in AUTOSAR; standard ids are the plain 11-bit value. */
typedef uint32 Can_IdType;
#define CAN_ID_EXTENDED_FLAG   0x80000000u
#define CAN_ID_VALUE_MASK_STD  0x000007FFu

typedef uint16 Can_HwHandleType;           /* HOH (hardware object handle) id */

typedef enum { CAN_OK = 0, CAN_NOT_OK = 1, CAN_BUSY = 2 } Can_ReturnType;

typedef enum {
    CAN_CS_UNINIT = 0, CAN_CS_STARTED, CAN_CS_STOPPED, CAN_CS_SLEEP
} Can_ControllerStateType;

typedef enum {
    CAN_T_START = 1, CAN_T_STOP, CAN_T_SLEEP, CAN_T_WAKEUP
} Can_StateTransitionType;

typedef struct {
    PduIdType  swPduHandle;                /* CanIf tx L-PDU id, returned in CanIf_TxConfirmation */
    uint8      length;                     /* 0..8 */
    Can_IdType id;
    uint8     *sdu;
} Can_PduType;

typedef struct {                           /* identifies the receive mailbox for CanIf_RxIndication */
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

#endif /* CAN_GENERALTYPES_H */
