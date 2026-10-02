/*
 * Dcm_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Dcm_Types.h (DCM SWS R20-11).
 * Dcm_MsgContextType follows SWS_Dcm_00994 (p.234-235): it is the object a
 * service handler receives - request data WITHOUT the SID, a response
 * buffer WITHOUT the response SID, and addressing info.
 */
#ifndef DCM_TYPES_H
#define DCM_TYPES_H

#include "ComStack_Types.h"
#include "Rte_Dcm_Type.h"

typedef uint8 *Dcm_MsgType;
typedef uint32 Dcm_MsgLenType;
typedef uint8  Dcm_IdContextType;

#define DCM_PHYSICAL_REQUEST    0u
#define DCM_FUNCTIONAL_REQUEST  1u

/* SWS uses bit fields; plain uint8 members keep the demo -pedantic clean. */
typedef struct {
    uint8 reqType;              /* DCM_PHYSICAL_REQUEST / DCM_FUNCTIONAL_REQUEST */
    uint8 suppressPosResponse;  /* SPRMIB (bit 7 of the sub-function) was set    */
} Dcm_MsgAddInfoType;

typedef struct {
    Dcm_MsgType        reqData;       /* request bytes after the SID            */
    Dcm_MsgLenType     reqDataLen;
    Dcm_MsgType        resData;       /* response bytes after the response SID  */
    Dcm_MsgLenType     resDataLen;
    Dcm_MsgAddInfoType msgAddInfo;
    Dcm_MsgLenType     resMaxDataLen;
    Dcm_IdContextType  idContext;
    PduIdType          dcmRxPduId;
} Dcm_MsgContextType;

#endif /* DCM_TYPES_H */
