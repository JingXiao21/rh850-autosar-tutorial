/*
 * Rte_Common.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the fixed part of Rte_Type.h / Rte.h (AUTOSAR_CP_SWS_RTE, "Standardized
 * Rte API": error codes, Rte_Start/Rte_Stop 5.7.x). Hand-written; the generated Rte_Type.h and
 * Rte.h include it. Contains nothing that depends on the configuration.
 *
 * RTE API NAMING used by generated contract headers Rte_<Swc>.h (instance-less, single instance per
 * SWC type; R4.x naming rules Rte_<API>_<port>_<element>):
 *   explicit S/R : Rte_Read_<port>_<de>(<type>* data)        Rte_Write_<port>_<de>(<type> data)
 *   implicit S/R : Rte_IRead_<runnable>_<port>_<de>()        Rte_IWrite_<runnable>_<port>_<de>(<type> data)
 *   client/server: Rte_Call_<port>_<operation>(args...)      server runnable: <runnable>(args...)
 *   mode         : Rte_Mode_<port>_<mode group>()            Rte_Switch_<port>_<mode group>(mode)
 *   per-instance : Rte_Pim_<pimName>()   returns <pimType>*
 *   exclusive    : Rte_Enter_<areaName>() / Rte_Exit_<areaName>()
 * Runnable prototypes: void <runnable>(void) (S/R, timing, mode events) or
 * Std_ReturnType <runnable>(<args>) (server operations).
 */
#ifndef RTE_COMMON_H
#define RTE_COMMON_H

#include "Std_Types.h"

/* application error codes (Rte SWS 5.x "Rte_Type.h"; values as in the standard) */
#define RTE_E_OK                    ((Std_ReturnType)0u)
#define RTE_E_INVALID               ((Std_ReturnType)1u)
#define RTE_E_LOST_DATA             ((Std_ReturnType)64u)
#define RTE_E_MAX_AGE_EXCEEDED      ((Std_ReturnType)64u)
#define RTE_E_COM_STOPPED           ((Std_ReturnType)128u)
#define RTE_E_TIMEOUT               ((Std_ReturnType)129u)
#define RTE_E_LIMIT                 ((Std_ReturnType)130u)
#define RTE_E_NO_DATA               ((Std_ReturnType)131u)
#define RTE_E_TRANSMIT_ACK          ((Std_ReturnType)132u)
#define RTE_E_NEVER_RECEIVED        ((Std_ReturnType)133u)
#define RTE_E_UNCONNECTED           ((Std_ReturnType)134u)
#define RTE_E_IN_EXCLUSIVE_AREA     ((Std_ReturnType)135u)

/* lifecycle (generated Rte.c): Rte_Start enables Com notifications/mode machines, initialises RTE
 * buffers with their init values; Rte_Stop is the reverse. Called by BswM actions (BswM_Cfg.c). */
Std_ReturnType Rte_Start(void);
Std_ReturnType Rte_Stop(void);

#endif /* RTE_COMMON_H */
