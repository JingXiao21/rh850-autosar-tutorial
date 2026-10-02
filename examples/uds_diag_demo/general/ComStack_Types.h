/*
 * ComStack_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: ComStack_Types.h (AUTOSAR_SWS_CommunicationStackTypes),
 * shared by CanIf / CanTp / PduR / Dcm / Com.
 *
 * Only the types used by the diagnostic path are provided. Type widths
 * (PduIdType, PduLengthType = uint16) are configuration dependent in a real
 * stack. Signature per R4.x convention, confirm against project release
 * (this repository has no ComStack_Types SWS).
 */
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

/* [AUTOSAR API] Handle of an I-PDU / N-SDU / L-PDU. The numeric value is
 * only meaningful to the module that owns the handle: CanIf's L-PDU 0,
 * CanTp's N-PDU 0 and Dcm's DcmRxPduId 0 are unrelated handles. Routing
 * tables (CanIf_Cfg.c, CanTp_Cfg.c, PduR_Cfg.c) connect them. */
typedef uint16 PduIdType;
typedef uint16 PduLengthType;

/* [AUTOSAR API] Data + length container passed across every layer.
 * R4.x (>= 4.2) added MetaDataPtr; unused here (no meta data configured). */
typedef struct {
    uint8         *SduDataPtr;
    uint8         *MetaDataPtr;
    PduLengthType  SduLength;
} PduInfoType;

/* [AUTOSAR API] Result of the TP buffer handshake
 * (StartOfReception / CopyRxData / CopyTxData). */
typedef enum {
    BUFREQ_OK = 0,       /* buffer request accomplished                       */
    BUFREQ_E_NOT_OK,     /* request failed, abort the transfer                */
    BUFREQ_E_BUSY,       /* temporarily no buffer/data, lower layer may retry */
    BUFREQ_E_OVFL        /* requested length exceeds the buffer               */
} BufReq_ReturnType;

/* [AUTOSAR API] Retry handling for CopyTxData (CanTp may ask the upper layer
 * to re-provide data if a frame must be repeated). */
typedef enum {
    TP_DATACONF = 0,     /* all previously copied data is confirmed           */
    TP_DATARETRY,        /* re-copy TxTpDataCnt bytes back from current pos.  */
    TP_CONFPENDING       /* previously copied data not yet confirmed          */
} TpDataStateType;

typedef struct {
    TpDataStateType TpDataState;
    PduLengthType   TxTpDataCnt;
} RetryInfoType;

/* [AUTOSAR API] Parameters changeable via <Up>_ChangeParameter (unused). */
typedef enum {
    TP_STMIN = 0,
    TP_BS,
    TP_BC
} TPParameterType;

typedef uint8 NetworkHandleType;

#endif /* COMSTACK_TYPES_H */
