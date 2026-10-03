/*
 * Com.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: COM (AUTOSAR_CP_SWS_COM; Com_Init, Com_IpduGroupStart 8.3.2.3,
 * Com_SendSignal, Com_ReceiveSignal, Com_RxIndication 8.4.2, Com_TxConfirmation, Com_TriggerTransmit,
 * Com_MainFunctionRx 8.5.1 / Com_MainFunctionTx). Implemented: signal pack/unpack (LE/BE, 1..32 bit,
 * unsigned/signed), I-PDU buffers, PERIODIC/DIRECT/MIXED transmit modes, TRIGGERED signals,
 * IMMEDIATE and DEFERRED reception with notification callbacks, I-PDU groups (start/stop).
 * Not implemented: signal groups, update bits, filters, gateway, TX/RX deadline monitoring, DM,
 * metadata, value-changed notifications, Com_ReceiveSignal "invalidation".
 * Called by: RTE (Rte.c: Com_SendSignal/Com_ReceiveSignal), PduR (Com_RxIndication,
 * Com_TxConfirmation, Com_TriggerTransmit), BswM (Com_IpduGroupStart/Stop), BSW task (MainFunctions).
 * Calls: PduR_ComTransmit (PduR.h). Critical sections: SchM_Enter_Com_COM_EXCLUSIVE_AREA_0.
 * Owner: agent C. Config: Com_Types.h + generated Com_Cfg.c/.h.
 */
#ifndef COM_H
#define COM_H

#include "ComStack_Types.h"
#include "Com_Types.h"
#include "Com_Cfg.h"     /* generated: ComConf_ComSignal_*, ComConf_ComIPdu_*, COM_NUM_IPDUS, COM_NUM_SIGNALS */

void           Com_Init(const Com_ConfigType *config);
void           Com_DeInit(void);
void           Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize);  /* initialize: reload init values */
void           Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId);
uint8          Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr);     /* E_OK / COM_SERVICE_NOT_AVAILABLE / COM_BUSY */
uint8          Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr);        /* E_OK / COM_SERVICE_NOT_AVAILABLE */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);                                      /* send a tx I-PDU now */
Com_StatusType Com_GetStatus(void);
void           Com_GetVersionInfo(Std_VersionInfoType *versioninfo);

/* scheduled functions: both are called every Com_Config.mainFunctionPeriodMs (10 ms) from a BSW task */
void           Com_MainFunctionRx(void);   /* deferred reception: unpack -> rxNotification */
void           Com_MainFunctionTx(void);   /* periodic/mixed transmission: PduR_ComTransmit */

/* callbacks invoked by PduR (names per R25-11 Com SWS; PduIdType = Com I-PDU handle) */
void           Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void           Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
Std_ReturnType Com_TriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr);

#endif /* COM_H */
