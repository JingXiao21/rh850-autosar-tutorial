/*
 * EcuM.h
 *
 * [Educational Implementation] EcuM-like startup.
 * Real AUTOSAR counterpart: EcuM (ECU State Manager) - EcuM_Init runs
 * EcuM_AL_DriverInitZero/One (MCAL: Mcu, Port, Can, ...), starts the OS, then
 * BswM performs the rest of the BSW init (CanIf, CanTp, PduR, NvM_ReadAll,
 * Dem, Dcm, ComM/CanSM) and Rte_Start. Here everything is one function.
 */
#ifndef ECUM_H
#define ECUM_H

#include "Std_Types.h"

void EcuM_Init(void);

/* [Educational Implementation] simulated reset (stands in for Mcu_PerformReset). */
void EcuM_SimRequestReset(void);
boolean EcuM_SimIsResetRequested(void);
void EcuM_SimPerformReset(void);
uint32 EcuM_SimGetResetCount(void);

#endif /* ECUM_H */
