/*
 * Can.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: CAN Driver (AUTOSAR_CP_SWS_CANDriver; Can_Init 8.3.1, Can_SetControllerMode
 * 8.3.x, Can_Write 8.3.3.1, Can_MainFunction_* 8.5). Implemented: ONE controller (STM32L552 FDCAN1 in
 * classic-CAN mode, 500 kbit/s from config), standard ids, TX by polling confirmation
 * (Can_MainFunction_Write), RX by Cat2 interrupt (Can_Isr_Rx) or polling (Can_MainFunction_Read) as
 * selected in Can_Config. Not implemented: CAN FD, extended ids, wakeup, baudrate change, error states.
 *
 * Two backends behind one hardware-independent Can.c (owner agent B):
 *   can/Can.c           : HOH/HTH handling, TX queue of depth 1 per HTH, id/filter checks, upcalls
 *   can/Can_Hw_Stm32.c  : FDCAN1 registers + message RAM (target only)
 *   sim/host/Can_Hw_Sim.c: virtual bus via SimCan (host only)
 */
#ifndef CAN_H
#define CAN_H

#include "Can_GeneralTypes.h"
#include "Can_Cfg.h"          /* generated: CanConf_* hth/hrh ids */

typedef enum { CAN_HOH_RECEIVE = 0, CAN_HOH_TRANSMIT = 1 } Can_HohTypeType;

typedef struct {
    Can_HwHandleType hoh;          /* handle: HRH ids and HTH ids share one number space */
    Can_HohTypeType  type;
    Can_IdType       canId;        /* tx: not used (CanIf passes id in Can_PduType); rx: filter id */
    Can_IdType       filterMask;   /* rx: acceptance mask (0x7FF = exact match) */
    uint8            controller;   /* 0 */
} Can_HohConfigType;

typedef struct {
    uint32                   baudrate;      /* bit/s, 500000 */
    boolean                  rxInterrupt;   /* TRUE: RX via Cat2 ISR (Can_Isr_Rx); FALSE: Can_MainFunction_Read polls */
    uint8                    numHoh;
    const Can_HohConfigType *hoh;
} Can_ConfigType;
extern const Can_ConfigType Can_Config;          /* generated Can_Cfg.c */

void             Can_Init(const Can_ConfigType *Config);
Std_ReturnType   Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Can_ReturnType   Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
void             Can_DisableControllerInterrupts(uint8 Controller);
void             Can_EnableControllerInterrupts(uint8 Controller);
void             Can_GetVersionInfo(Std_VersionInfoType *versioninfo);

/* scheduled functions (called from a BSW task by the generated Rte_Tasks.c) */
void             Can_MainFunction_Write(void);     /* polls TX completion -> CanIf_TxConfirmation */
void             Can_MainFunction_Read(void);      /* polls RX FIFO -> CanIf_RxIndication (polling mode) */
void             Can_MainFunction_BusOff(void);
void             Can_MainFunction_Mode(void);

/* interrupt service body: drains RX FIFO0 -> CanIf_RxIndication. Wrapped by the Cat2 ISR
 * `ISR(CanRxIsr) { Can_Isr_Rx(); }` defined in generated code (Os_Cfg / Rte_Tasks.c). */
void             Can_Isr_Rx(void);

#endif /* CAN_H */
