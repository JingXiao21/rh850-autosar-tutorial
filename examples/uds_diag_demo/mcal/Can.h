/*
 * Can.h
 *
 * [Educational Implementation] Can driver MOCK.
 * Real AUTOSAR counterpart: the vendor CAN MCAL driver (for RH850/P1M-E:
 * the Renesas RS-CANFD Can driver). API names/signatures follow CAN SWS
 * R22-11 (Can_Write p.80, Can_SetControllerMode p.66, Can_Init p.62).
 *
 * What is real here:   the API contract towards CanIf (HOH, Can_PduType,
 *                      CAN_BUSY, async mode change, callbacks).
 * What is simulated:   all hardware access goes to VirtualCanBus_Hw*,
 *                      the RX "interrupt" is called by the simulated
 *                      scheduler (SchM_Tick) instead of the INTC.
 *
 * Replace with the RH850 MCAL: see examples/uds_diag_demo/README.md and
 * docs/04-can-mcal/14-can-driver-from-scratch.md.
 */
#ifndef CAN_H
#define CAN_H

#include "Can_GeneralTypes.h"
#include "Can_Cfg.h"

#define CAN_E_PARAM_POINTER     0x01u
#define CAN_E_PARAM_HANDLE      0x02u
#define CAN_E_PARAM_DATA_LENGTH 0x03u
#define CAN_E_PARAM_CONTROLLER  0x04u
#define CAN_E_UNINIT            0x05u
#define CAN_E_TRANSITION        0x06u

#define CAN_SID_INIT                 0x00u
#define CAN_SID_SET_CONTROLLER_MODE  0x03u
#define CAN_SID_WRITE                0x06u

typedef enum {
    CAN_OBJECT_TYPE_RECEIVE = 0,
    CAN_OBJECT_TYPE_TRANSMIT
} Can_ObjectTypeType;

/* Subset of CanHardwareObject (ECUC_Can_00324). */
typedef struct {
    Can_HwHandleType   objectId;     /* CanObjectId                        */
    Can_ObjectTypeType objectType;   /* CanObjectType                      */
    uint8              controllerId; /* CanControllerRef                   */
    Can_IdType         filterCode;   /* CanHwFilterCode (RECEIVE only)     */
    Can_IdType         filterMask;   /* CanHwFilterMask (RECEIVE only)     */
    uint8              hwBufferIdx;  /* RS-CANFD rule index / TX buffer p  */
    const char        *name;
} Can_HardwareObjectConfigType;

/* Subset of CanController (ECUC_Can_00354). */
typedef struct {
    uint8  controllerId;
    uint32 baudRate;    /* kbit/s; real bit timing: see Can_BitTiming.c in rh850_mcal_reference */
} Can_ControllerConfigType;

typedef struct {
    const Can_ControllerConfigType     *controllers;
    uint8                               numControllers;
    const Can_HardwareObjectConfigType *hoh;
    uint8                               numHoh;
} Can_ConfigType;

extern const Can_ConfigType Can_Config;

void Can_Init(const Can_ConfigType *Config);
void Can_DeInit(void);
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition);
Std_ReturnType Can_GetControllerMode(uint8 Controller, Can_ControllerStateType *ControllerModePtr);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
void Can_MainFunction_Write(void);
void Can_MainFunction_Read(void);
void Can_MainFunction_Mode(void);

/* [Educational Implementation] ISR body for the global RX FIFO interrupt.
 * On RH850/P1M-E this is INTRCANGRECC, EI channel 190 (not EI184, which is
 * the CAN0 TX/RX FIFO receive interrupt). In a real project the OS (RTA-OS)
 * owns the vector and calls the MCAL ISR from a Cat-2 ISR wrapper. */
void Can_Isr_GlobalRxFifo(void);
boolean Can_IsRxInterruptEnabled(void);

#endif /* CAN_H */
