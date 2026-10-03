/*
 * ComStack_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: ComStack_Types.h (AUTOSAR_SWS_CommunicationStackTypes), shared by
 * CanIf / PduR / Com. Same content as examples/uds_diag_demo (TP types kept for compatibility).
 */
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

/* Handle of an I-PDU / L-PDU. Its value is only meaningful to the module that owns it:
 * Com's tx PDU 0, PduR's tx PDU 0 and CanIf's tx PDU 0 are three different handles
 * connected by the routing tables in Com_Cfg.c / PduR_Cfg.c / CanIf_Cfg.c. */
typedef uint16 PduIdType;
typedef uint16 PduLengthType;

typedef struct {
    uint8         *SduDataPtr;
    uint8         *MetaDataPtr;     /* unused, always NULL_PTR */
    PduLengthType  SduLength;
} PduInfoType;

typedef enum { BUFREQ_OK = 0, BUFREQ_E_NOT_OK, BUFREQ_E_BUSY, BUFREQ_E_OVFL } BufReq_ReturnType;
typedef enum { TP_DATACONF = 0, TP_DATARETRY, TP_CONFPENDING } TpDataStateType;
typedef struct { TpDataStateType TpDataState; PduLengthType TxTpDataCnt; } RetryInfoType;
typedef enum { TP_STMIN = 0, TP_BS, TP_BC } TPParameterType;

typedef uint8 NetworkHandleType;

#endif /* COMSTACK_TYPES_H */
