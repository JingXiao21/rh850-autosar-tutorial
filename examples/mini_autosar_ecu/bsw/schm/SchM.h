/*
 * SchM.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: BSW Scheduler (SchM; AUTOSAR_CP_SWS_BSWGeneral / RTE SWS chapter "BSW
 * Scheduler"). In real tools the RTE generator also generates the SchM: SchM_Init/SchM_Start,
 * SchM_Enter_<Module>_<ExclusiveArea>() / SchM_Exit_..., and the OS task bodies that call the
 * BSW MainFunctions. Here:
 *   - exclusive areas are fixed, hand-written macros below (all map to SuspendOSInterrupts /
 *     ResumeOSInterrupts, i.e. ECUC "ALL_INTERRUPT_BLOCKING/OS_INTERRUPT_BLOCKING" implementation);
 *   - the task bodies calling the MainFunctions are generated into gen/<ecu>/Rte_Tasks.c.
 * BSW modules MUST use only these macros for critical sections (never Os.h directly).
 * Owner: agent C (SchM.c: SchM_Init/Deinit = trace line only).
 */
#ifndef SCHM_H
#define SCHM_H

#include "Std_Types.h"
#include "Os.h"

void SchM_Init(void);
void SchM_Deinit(void);
/* [ADDED by agent C] R25-11 StartPostOS sequence (EcuM SWS 7.3.3, Figure 7.5): SchM_Start() -> BswM_Init ->
 * SchM_Init -> SchM_StartTiming(). Here both are trace-only (alarms autostart in the OS config). */
Std_ReturnType SchM_Start(void);
void SchM_StartTiming(void);

#define SchM_Enter_Com_COM_EXCLUSIVE_AREA_0()      SuspendOSInterrupts()
#define SchM_Exit_Com_COM_EXCLUSIVE_AREA_0()       ResumeOSInterrupts()
#define SchM_Enter_PduR_PDUR_EXCLUSIVE_AREA_0()    SuspendOSInterrupts()
#define SchM_Exit_PduR_PDUR_EXCLUSIVE_AREA_0()     ResumeOSInterrupts()
#define SchM_Enter_CanIf_CANIF_EXCLUSIVE_AREA_0()  SuspendOSInterrupts()
#define SchM_Exit_CanIf_CANIF_EXCLUSIVE_AREA_0()   ResumeOSInterrupts()
#define SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0()      SuspendOSInterrupts()
#define SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0()       ResumeOSInterrupts()
#define SchM_Enter_BswM_BSWM_EXCLUSIVE_AREA_0()    SuspendOSInterrupts()
#define SchM_Exit_BswM_BSWM_EXCLUSIVE_AREA_0()     ResumeOSInterrupts()
#define SchM_Enter_Det_DET_EXCLUSIVE_AREA_0()      SuspendAllInterrupts()   /* Det may be called from any ISR */
#define SchM_Exit_Det_DET_EXCLUSIVE_AREA_0()       ResumeAllInterrupts()

#endif /* SCHM_H */
