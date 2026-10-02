/*
 * Dcm_Dsp.c - Diagnostic Service Processing
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the DSP part of Dcm (DCM SWS R20-11 §7.5/7.6,
 * service chapters p.112-196). Every handler has the external service handler
 * shape (OpStatus, pMsgContext, ErrorCode) and returns E_OK / E_NOT_OK /
 * DCM_E_PENDING. reqData/resData exclude the SID (DSD adds SID + 0x40).
 *
 * Services: 0x10 0x11 0x14 0x19(0x02) 0x22 0x27 0x2E 0x31 0x3E.
 * The DSP never calls a SW-C directly: DIDs, security and routines go through
 * the function pointers in Dcm_Cfg.c, which point at Rte_Call_* (Rte_Dcm.c).
 */
#include "Dcm_Internal.h"
#include "Dem.h"
#include "SchM_Dcm.h"
#include "UdsTrace.h"
#include <string.h>

#define DCM_DSP_MAX_SECURITY_ROWS 2u
#define DCM_DSP_MAX_ROUTINES      2u

/* ---- runtime state that must survive DCM_E_PENDING ---- */
typedef struct {
    uint8            didIdx[DCM_DSP_MAX_DID_TO_READ];
    uint8            numDids;
    uint8            current;
    Dcm_OpStatusType currentOpStatus;
    Dcm_MsgLenType   pos;
} Dcm_DspRdbiStateType;

typedef struct {
    uint8            seedLevel;                              /* level whose seed was sent; 0 = none */
    uint8            attemptCounter[DCM_DSP_MAX_SECURITY_ROWS];
    sint32           delayMs[DCM_DSP_MAX_SECURITY_ROWS];
} Dcm_DspSecurityStateType;

static Dcm_DspRdbiStateType     Dcm_DspRdbi;
static uint8                    Dcm_DspWdbiDid;
static Dcm_DspSecurityStateType Dcm_DspSec;
static boolean                  Dcm_DspRoutineStarted[DCM_DSP_MAX_ROUTINES];
static uint8                    Dcm_DspRoutineActive;

static boolean Dcm_DspSessionOk(uint32 mask)
{
    return (boolean)((mask & DCM_SES_MASK(Dcm_DslGetSesCtrlType())) != 0u);
}

static boolean Dcm_DspSecurityOk(uint32 mask)
{
    return (boolean)((mask & DCM_SEC_MASK(Dcm_DslGetSecurityLevel())) != 0u);
}

static const Dcm_DspDidType *Dcm_DspFindDid(uint16 did, uint8 *idx)
{
    uint8 i;
    for (i = 0u; i < Dcm_CfgPtr->numDids; i++) {
        if (Dcm_CfgPtr->dids[i].identifier == did) {
            *idx = i;
            return &Dcm_CfgPtr->dids[i];
        }
    }
    return NULL_PTR;
}

static const char *Dcm_DspOpName(Dcm_OpStatusType op)
{
    switch (op) {
    case DCM_INITIAL: return "DCM_INITIAL";
    case DCM_PENDING: return "DCM_PENDING";
    case DCM_CANCEL:  return "DCM_CANCEL";
    default:          return "DCM_FORCE_RCRRP_OK";
    }
}

void Dcm_DspInit(void)
{
    (void)memset(&Dcm_DspRdbi, 0, sizeof(Dcm_DspRdbi));
    (void)memset(&Dcm_DspSec, 0, sizeof(Dcm_DspSec));
    (void)memset(Dcm_DspRoutineStarted, 0, sizeof(Dcm_DspRoutineStarted));
    Dcm_DspWdbiDid = 0u;
    Dcm_DspRoutineActive = 0u;
}

/* Called from Dcm_MainFunction: security delay timers (DcmDspSecurityDelayTime). */
void Dcm_DspMainFunction(void)
{
    uint8 r;
    for (r = 0u; (r < Dcm_CfgPtr->numSecurityRows) && (r < DCM_DSP_MAX_SECURITY_ROWS); r++) {
        if (Dcm_DspSec.delayMs[r] > 0) {
            Dcm_DspSec.delayMs[r] -= (sint32)DCM_TASK_TIME_MS;
            if (Dcm_DspSec.delayMs[r] <= 0) {
                Dcm_DspSec.delayMs[r] = 0;
                Dcm_DspSec.attemptCounter[r] = 0u;     /* SWS_Dcm_01357 */
                UDS_TRACE("Dcm/DSP", "security delay of level %u expired -> attempt counter reset",
                          (unsigned)Dcm_CfgPtr->securityRows[r].level);
            }
        }
    }
}

void Dcm_DspSessionChanged(void)
{
    Dcm_DspSec.seedLevel = 0u;   /* a seed is only valid in the session it was requested in */
}

/* ================================================================== */
/* 0x10 DiagnosticSessionControl (SWS_Dcm_00250, 00307, 00311)         */
/* ================================================================== */
Std_ReturnType Dcm_DspDiagnosticSessionControl(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                               Dcm_NegativeResponseCodeType *ErrorCode)
{
    uint8 row;
    const Dcm_DspSessionRowType *r;
    uint16 p2Star10ms;

    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (pMsgContext->reqDataLen != 1u) {
        *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
        return E_NOT_OK;
    }
    row = Dcm_DslFindSessionRow(pMsgContext->reqData[0]);
    if (row == DCM_SESSION_ROW_INVALID) {
        *ErrorCode = DCM_E_SUBFUNCTIONNOTSUPPORTED;
        return E_NOT_OK;
    }
    r = &Dcm_CfgPtr->sessionRows[row];
    p2Star10ms = (uint16)(r->p2StarServerMaxMs / 10u);   /* ISO 14229-2: P2* in 10 ms units */
    pMsgContext->resData[0] = r->level;
    pMsgContext->resData[1] = (uint8)(r->p2ServerMaxMs >> 8);
    pMsgContext->resData[2] = (uint8)(r->p2ServerMaxMs & 0xFFu);
    pMsgContext->resData[3] = (uint8)(p2Star10ms >> 8);
    pMsgContext->resData[4] = (uint8)(p2Star10ms & 0xFFu);
    pMsgContext->resDataLen = 5u;
    /* The switch itself happens after the response is confirmed (SWS_Dcm_00311). */
    Dcm_DslRequestSessionChange(row);
    UDS_TRACE("Dcm/DSP", "0x10: %s accepted, P2=%u ms P2*=%u ms; switch deferred until TX confirmation",
              r->name, (unsigned)r->p2ServerMaxMs, (unsigned)r->p2StarServerMaxMs);
    return E_OK;
}

/* ================================================================== */
/* 0x11 ECUReset (SWS_Dcm_00260, 00373, 00594)                         */
/* ================================================================== */
Std_ReturnType Dcm_DspEcuReset(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                               Dcm_NegativeResponseCodeType *ErrorCode)
{
    uint8 sub;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (pMsgContext->reqDataLen != 1u) {
        *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
        return E_NOT_OK;
    }
    sub = pMsgContext->reqData[0];
    /* SWS_Dcm_00373: announce the reset type first (BswM may prepare), then respond. */
    SchM_Switch_Dcm_DcmEcuReset((sub == 0x01u) ? RTE_MODE_DcmEcuReset_HARD : RTE_MODE_DcmEcuReset_SOFT);
    Dcm_DslRequestEcuReset(sub);
    pMsgContext->resData[0] = sub;
    pMsgContext->resDataLen = 1u;
    return E_OK;
}

/* ================================================================== */
/* 0x14 ClearDiagnosticInformation (SWS_Dcm_01263, 00005, 00705..)     */
/* ================================================================== */
Std_ReturnType Dcm_DspClearDiagnosticInformation(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                                 Dcm_NegativeResponseCodeType *ErrorCode)
{
    Std_ReturnType r;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (OpStatus == DCM_INITIAL) {
        uint32 group;
        if (pMsgContext->reqDataLen != 3u) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
            return E_NOT_OK;
        }
        group = ((uint32)pMsgContext->reqData[0] << 16) | ((uint32)pMsgContext->reqData[1] << 8) |
                pMsgContext->reqData[2];
        UDS_TRACE("Dcm/DSP", "0x14: Dem_SelectDTC(client %u, 0x%06lX) + Dem_ClearDTC",
                  (unsigned)DCM_DEM_CLIENT_ID, (unsigned long)group);
        if (Dem_SelectDTC(DCM_DEM_CLIENT_ID, group, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY) != E_OK) {
            *ErrorCode = DCM_E_CONDITIONSNOTCORRECT;
            return E_NOT_OK;
        }
    }
    r = Dem_ClearDTC(DCM_DEM_CLIENT_ID);
    switch (r) {
    case E_OK:
        pMsgContext->resDataLen = 0u;
        return E_OK;
    case DEM_PENDING:
        return DCM_E_PENDING;                                       /* SWS_Dcm_01412 */
    case DEM_WRONG_DTC:
    case DEM_WRONG_DTCORIGIN:
        *ErrorCode = DCM_E_REQUESTOUTOFRANGE;                       /* SWS_Dcm_00708/01408 */
        return E_NOT_OK;
    case DEM_CLEAR_MEMORY_ERROR:
        *ErrorCode = DCM_E_GENERALPROGRAMMINGFAILURE;               /* SWS_Dcm_01060 */
        return E_NOT_OK;
    default:
        *ErrorCode = DCM_E_CONDITIONSNOTCORRECT;                    /* SWS_Dcm_00707/00966 */
        return E_NOT_OK;
    }
}

/* ================================================================== */
/* 0x19 ReadDTCInformation, sub-function 0x02 reportDTCByStatusMask    */
/* (SWS_Dcm_00377/00378, 00008)                                        */
/* ================================================================== */
Std_ReturnType Dcm_DspReadDTCInformation(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                         Dcm_NegativeResponseCodeType *ErrorCode)
{
    Dem_UdsStatusByteType avail = 0u;
    Dem_UdsStatusByteType status = 0u;
    uint32 dtc = 0u;
    uint8 mask;
    Dcm_MsgLenType pos;

    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (pMsgContext->reqDataLen != 2u) {
        *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
        return E_NOT_OK;
    }
    mask = pMsgContext->reqData[1];
    (void)Dem_GetDTCStatusAvailabilityMask(DCM_DEM_CLIENT_ID, &avail);
    pMsgContext->resData[0] = 0x02u;
    pMsgContext->resData[1] = avail;
    pos = 2u;
    if ((mask & avail) != 0u) {                                     /* SWS_Dcm_00008 */
        if (Dem_SetDTCFilter(DCM_DEM_CLIENT_ID, mask, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY,
                             FALSE, 0u, FALSE) != E_OK) {
            *ErrorCode = DCM_E_REQUESTOUTOFRANGE;
            return E_NOT_OK;
        }
        while (Dem_GetNextFilteredDTC(DCM_DEM_CLIENT_ID, &dtc, &status) == E_OK) {
            if ((pos + 4u) > pMsgContext->resMaxDataLen) {
                *ErrorCode = DCM_E_RESPONSETOOLONG;
                return E_NOT_OK;
            }
            pMsgContext->resData[pos++] = (uint8)(dtc >> 16);
            pMsgContext->resData[pos++] = (uint8)(dtc >> 8);
            pMsgContext->resData[pos++] = (uint8)dtc;
            pMsgContext->resData[pos++] = status;
        }
    }
    UDS_TRACE("Dcm/DSP", "0x19 02: mask 0x%02X & availability 0x%02X -> %u DTC(s) from Dem",
              (unsigned)mask, (unsigned)avail, (unsigned)((pos - 2u) / 4u));
    pMsgContext->resDataLen = pos;
    return E_OK;
}


/* ================================================================== */
/* 0x22 ReadDataByIdentifier (SWS_Dcm_01335, 00438, 00433-00437)       */
/* ================================================================== */
Std_ReturnType Dcm_DspReadDataByIdentifier(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                           Dcm_NegativeResponseCodeType *ErrorCode)
{
    Dcm_DspRdbiStateType *st = &Dcm_DspRdbi;

    if (OpStatus == DCM_CANCEL) {
        if (st->current < st->numDids) {
            const Dcm_DspDidType *d = &Dcm_CfgPtr->dids[st->didIdx[st->current]];
            if (d->usePort == DCM_USE_DATA_ASYNCH_CLIENT_SERVER) {
                (void)d->readAsync(DCM_CANCEL, &pMsgContext->resData[st->pos + 2u]);
            }
        }
        st->numDids = 0u;
        return E_OK;
    }

    if (OpStatus == DCM_INITIAL) {
        uint8 n;
        uint8 i;
        boolean securityFailed = FALSE;
        Dcm_MsgLenType total = 0u;

        if ((pMsgContext->reqDataLen < 2u) || ((pMsgContext->reqDataLen % 2u) != 0u)) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
            return E_NOT_OK;
        }
        n = (uint8)(pMsgContext->reqDataLen / 2u);
        if (n > DCM_DSP_MAX_DID_TO_READ) {                          /* SWS_Dcm_01335 */
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
            return E_NOT_OK;
        }
        st->numDids = 0u;
        for (i = 0u; i < n; i++) {
            uint16 didId = (uint16)(((uint16)pMsgContext->reqData[2u * i] << 8) |
                                    pMsgContext->reqData[(2u * i) + 1u]);
            uint8 idx = 0u;
            const Dcm_DspDidType *d = Dcm_DspFindDid(didId, &idx);

            /* SWS_Dcm_00438/00433/00434: unsupported, not readable or not allowed
             * in this session -> skipped; only if ALL are skipped -> NRC 0x31. */
            if ((d == NULL_PTR) || !Dcm_DspSessionOk(d->readSessionMask)) {
                UDS_TRACE("Dcm/DSP", "0x22: DID 0x%04X not supported / not in this session -> skipped",
                          (unsigned)didId);
                continue;
            }
            if (!Dcm_DspSecurityOk(d->readSecurityMask)) {
                securityFailed = TRUE;                               /* SWS_Dcm_00435 */
                continue;
            }
            st->didIdx[st->numDids++] = idx;
            total += (Dcm_MsgLenType)(2u + d->size);
        }
        if (securityFailed) {
            *ErrorCode = DCM_E_SECURITYACCESSDENIED;
            return E_NOT_OK;
        }
        if (st->numDids == 0u) {
            *ErrorCode = DCM_E_REQUESTOUTOFRANGE;
            return E_NOT_OK;
        }
        if (total > pMsgContext->resMaxDataLen) {
            *ErrorCode = DCM_E_RESPONSETOOLONG;
            return E_NOT_OK;
        }
        st->current = 0u;
        st->pos = 0u;
        st->currentOpStatus = DCM_INITIAL;
    }

    /* Read the DIDs one after the other; an asynchronous one may return
     * DCM_E_PENDING, then we continue exactly here in the next cycle. */
    while (st->current < st->numDids) {
        const Dcm_DspDidType *d = &Dcm_CfgPtr->dids[st->didIdx[st->current]];
        uint8 *out = &pMsgContext->resData[st->pos];
        Std_ReturnType r;

        out[0] = (uint8)(d->identifier >> 8);
        out[1] = (uint8)(d->identifier & 0xFFu);
        if (d->usePort == DCM_USE_DATA_SYNCH_CLIENT_SERVER) {
            UDS_TRACE("Dcm/DSP", "0x22: DID 0x%04X (%s) USE_DATA_SYNCH_CLIENT_SERVER -> ReadData(Data)",
                      (unsigned)d->identifier, d->name);
            r = d->readSync(&out[2]);
        } else {
            UDS_TRACE("Dcm/DSP", "0x22: DID 0x%04X (%s) USE_DATA_ASYNCH_CLIENT_SERVER -> ReadData(%s, Data)",
                      (unsigned)d->identifier, d->name, Dcm_DspOpName(st->currentOpStatus));
            r = d->readAsync(st->currentOpStatus, &out[2]);
        }
        if (r == DCM_E_PENDING) {
            st->currentOpStatus = DCM_PENDING;                       /* SWS_Dcm_00530 */
            return DCM_E_PENDING;
        }
        if (r != E_OK) {
            st->numDids = 0u;
            *ErrorCode = DCM_E_CONDITIONSNOTCORRECT;
            return E_NOT_OK;
        }
        st->pos += (Dcm_MsgLenType)(2u + d->size);
        st->current++;
        st->currentOpStatus = DCM_INITIAL;
    }
    pMsgContext->resDataLen = st->pos;
    st->numDids = 0u;
    return E_OK;
}

/* ================================================================== */
/* 0x27 SecurityAccess (SWS_Dcm_00321-00325, 00660, 01349, 01350)      */
/* ================================================================== */
static uint8 Dcm_DspFindSecurityRow(uint8 level)
{
    uint8 r;
    for (r = 0u; r < Dcm_CfgPtr->numSecurityRows; r++) {
        if (Dcm_CfgPtr->securityRows[r].level == level) {
            return r;
        }
    }
    return 0xFFu;
}

Std_ReturnType Dcm_DspSecurityAccess(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                     Dcm_NegativeResponseCodeType *ErrorCode)
{
    uint8 sub = pMsgContext->reqData[0];
    uint8 level = (uint8)((sub + 1u) / 2u);                  /* SecurityLevel = (AccessType+1)/2 */
    uint8 rowIdx = Dcm_DspFindSecurityRow(level);
    const Dcm_DspSecurityRowType *row;
    Std_ReturnType r;

    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if ((rowIdx == 0xFFu) || (rowIdx >= DCM_DSP_MAX_SECURITY_ROWS)) {
        *ErrorCode = DCM_E_SUBFUNCTIONNOTSUPPORTED;                  /* SWS_Dcm_00321 */
        return E_NOT_OK;
    }
    row = &Dcm_CfgPtr->securityRows[rowIdx];

    if ((sub & 0x01u) != 0u) {
        /* ---------------- requestSeed ---------------- */
        if (OpStatus == DCM_INITIAL) {
            if (pMsgContext->reqDataLen != 1u) {
                *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
                return E_NOT_OK;
            }
            if (Dcm_DspSec.delayMs[rowIdx] > 0) {                    /* SWS_Dcm_01350 */
                UDS_TRACE("Dcm/DSP", "0x27: requestSeed during security delay (%ld ms left) -> NRC 0x37",
                          (long)Dcm_DspSec.delayMs[rowIdx]);
                *ErrorCode = DCM_E_REQUIREDTIMEDELAYNOTEXPIRED;
                return E_NOT_OK;
            }
            if (Dcm_DslGetSecurityLevel() == level) {                 /* SWS_Dcm_00323 */
                pMsgContext->resData[0] = sub;
                (void)memset(&pMsgContext->resData[1], 0, row->seedSize);
                pMsgContext->resDataLen = (Dcm_MsgLenType)(1u + row->seedSize);
                UDS_TRACE("Dcm/DSP", "0x27: level %u already unlocked -> zero seed", (unsigned)level);
                return E_OK;
            }
        }
        UDS_TRACE("Dcm/DSP", "0x27 %02X: requestSeed level %u -> GetSeed(%s)", (unsigned)sub,
                  (unsigned)level, Dcm_DspOpName(OpStatus));
        r = row->getSeed(OpStatus, &pMsgContext->resData[1], ErrorCode);    /* SWS_Dcm_00324 */
        if (r == DCM_E_PENDING) {
            return DCM_E_PENDING;
        }
        if (r != E_OK) {
            return E_NOT_OK;                                         /* SWS_Dcm_00659 */
        }
        Dcm_DspSec.seedLevel = level;
        pMsgContext->resData[0] = sub;
        pMsgContext->resDataLen = (Dcm_MsgLenType)(1u + row->seedSize);
        return E_OK;
    }

    /* ---------------- sendKey ---------------- */
    if (OpStatus == DCM_INITIAL) {
        if (pMsgContext->reqDataLen != (Dcm_MsgLenType)(1u + row->keySize)) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
            return E_NOT_OK;
        }
        if (Dcm_DspSec.delayMs[rowIdx] > 0) {
            *ErrorCode = DCM_E_REQUIREDTIMEDELAYNOTEXPIRED;
            return E_NOT_OK;
        }
        if (Dcm_DspSec.seedLevel != level) {                         /* sendKey without seed */
            *ErrorCode = DCM_E_REQUESTSEQUENCEERROR;
            return E_NOT_OK;
        }
    }
    UDS_TRACE("Dcm/DSP", "0x27 %02X: sendKey level %u -> CompareKey(%s)", (unsigned)sub, (unsigned)level,
              Dcm_DspOpName(OpStatus));
    r = row->compareKey(&pMsgContext->reqData[1], OpStatus, ErrorCode);    /* SWS_Dcm_00863 */
    if (r == DCM_E_PENDING) {
        return DCM_E_PENDING;
    }
    Dcm_DspSec.seedLevel = 0u;           /* a seed can be used for exactly one key attempt */
    if (r == E_OK) {
        Dcm_DspSec.attemptCounter[rowIdx] = 0u;
        Dcm_DslSetSecurityLevel(level);                              /* SWS_Dcm_00325 */
        pMsgContext->resData[0] = sub;
        pMsgContext->resDataLen = 1u;
        return E_OK;
    }
    if (r == DCM_E_COMPARE_KEY_FAILED) {
        Dcm_DspSec.attemptCounter[rowIdx]++;                         /* SWS_Dcm_01397 */
        if (Dcm_DspSec.attemptCounter[rowIdx] >= row->numAttDelay) {
            Dcm_DspSec.delayMs[rowIdx] = (sint32)row->delayTimeMs;   /* SWS_Dcm_01349 */
            UDS_TRACE("Dcm/DSP", "0x27: invalid key, attempt %u/%u -> delay %u ms started, NRC 0x36",
                      (unsigned)Dcm_DspSec.attemptCounter[rowIdx], (unsigned)row->numAttDelay,
                      (unsigned)row->delayTimeMs);
            *ErrorCode = DCM_E_EXCEEDNUMBEROFATTEMPTS;
        } else {
            UDS_TRACE("Dcm/DSP", "0x27: invalid key, attempt %u/%u -> NRC 0x35",
                      (unsigned)Dcm_DspSec.attemptCounter[rowIdx], (unsigned)row->numAttDelay);
            *ErrorCode = DCM_E_INVALIDKEY;                           /* SWS_Dcm_00660 */
        }
        return E_NOT_OK;
    }
    return E_NOT_OK;                     /* E_NOT_OK: ErrorCode from the SW-C (SWS_Dcm_01150) */
}

/* ================================================================== */
/* 0x2E WriteDataByIdentifier (SWS_Dcm_00467-00473, 00395)             */
/* ================================================================== */
Std_ReturnType Dcm_DspWriteDataByIdentifier(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                            Dcm_NegativeResponseCodeType *ErrorCode)
{
    const Dcm_DspDidType *d;
    Std_ReturnType r;

    if (OpStatus == DCM_INITIAL) {
        uint16 didId;
        uint8 idx = 0u;
        if (pMsgContext->reqDataLen < 3u) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
            return E_NOT_OK;
        }
        didId = (uint16)(((uint16)pMsgContext->reqData[0] << 8) | pMsgContext->reqData[1]);
        d = Dcm_DspFindDid(didId, &idx);
        if ((d == NULL_PTR) || (d->writeAsync == NULL_PTR) || !Dcm_DspSessionOk(d->writeSessionMask)) {
            *ErrorCode = DCM_E_REQUESTOUTOFRANGE;                    /* SWS_Dcm_00467/00468/00469 */
            return E_NOT_OK;
        }
        if (!Dcm_DspSecurityOk(d->writeSecurityMask)) {
            UDS_TRACE("Dcm/DSP", "0x2E: DID 0x%04X needs security level, current is LOCKED -> NRC 0x33",
                      (unsigned)didId);
            *ErrorCode = DCM_E_SECURITYACCESSDENIED;                 /* SWS_Dcm_00470 */
            return E_NOT_OK;
        }
        if ((pMsgContext->reqDataLen - 2u) != d->size) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT; /* SWS_Dcm_00473 */
            return E_NOT_OK;
        }
        Dcm_DspWdbiDid = idx;
    }
    d = &Dcm_CfgPtr->dids[Dcm_DspWdbiDid];
    UDS_TRACE("Dcm/DSP", "0x2E: DID 0x%04X (%s) -> WriteData(Data, %s)", (unsigned)d->identifier, d->name,
              Dcm_DspOpName(OpStatus));
    r = d->writeAsync(&pMsgContext->reqData[2], OpStatus, ErrorCode);
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (r == DCM_E_PENDING) {
        return DCM_E_PENDING;
    }
    if (r != E_OK) {
        return E_NOT_OK;
    }
    pMsgContext->resData[0] = (uint8)(d->identifier >> 8);
    pMsgContext->resData[1] = (uint8)(d->identifier & 0xFFu);
    pMsgContext->resDataLen = 2u;
    return E_OK;
}

/* ================================================================== */
/* 0x31 RoutineControl (SWS_Dcm_00257, 00568-00571, 00869, 01140)      */
/* ================================================================== */
Std_ReturnType Dcm_DspRoutineControl(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                     Dcm_NegativeResponseCodeType *ErrorCode)
{
    uint8 sub = pMsgContext->reqData[0];
    const Dcm_DspRoutineType *rt;
    Dcm_RoutineFncType fnc;
    uint16 outLen = 0u;
    Std_ReturnType r;

    if (OpStatus == DCM_INITIAL) {
        uint16 rid;
        uint8 i;
        if (pMsgContext->reqDataLen < 3u) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
            return E_NOT_OK;
        }
        rid = (uint16)(((uint16)pMsgContext->reqData[1] << 8) | pMsgContext->reqData[2]);
        rt = NULL_PTR;
        for (i = 0u; (i < Dcm_CfgPtr->numRoutines) && (i < DCM_DSP_MAX_ROUTINES); i++) {
            if (Dcm_CfgPtr->routines[i].identifier == rid) {
                rt = &Dcm_CfgPtr->routines[i];
                Dcm_DspRoutineActive = i;
                break;
            }
        }
        if ((rt == NULL_PTR) || !Dcm_DspSessionOk(rt->sessionMask)) {
            *ErrorCode = DCM_E_REQUESTOUTOFRANGE;                    /* SWS_Dcm_00568/00570 */
            return E_NOT_OK;
        }
        if (!Dcm_DspSecurityOk(rt->securityMask)) {
            *ErrorCode = DCM_E_SECURITYACCESSDENIED;                 /* SWS_Dcm_00571 */
            return E_NOT_OK;
        }
        fnc = (sub == 0x01u) ? rt->start : ((sub == 0x02u) ? rt->stop : ((sub == 0x03u) ? rt->requestResults : NULL_PTR));
        if (fnc == NULL_PTR) {
            *ErrorCode = DCM_E_SUBFUNCTIONNOTSUPPORTED;              /* SWS_Dcm_00869 */
            return E_NOT_OK;
        }
        if (((sub == 0x01u) && ((pMsgContext->reqDataLen - 3u) != rt->startInLen)) ||
            ((sub != 0x01u) && (pMsgContext->reqDataLen != 3u))) {
            *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT; /* SWS_Dcm_01140 */
            return E_NOT_OK;
        }
        if ((sub != 0x01u) && !Dcm_DspRoutineStarted[Dcm_DspRoutineActive]) {
            UDS_TRACE("Dcm/DSP", "0x31 %02X: routine 0x%04X was never started -> NRC 0x24",
                      (unsigned)sub, (unsigned)rid);
            *ErrorCode = DCM_E_REQUESTSEQUENCEERROR;                 /* ISO 14229-1 */
            return E_NOT_OK;
        }
    }
    rt = &Dcm_CfgPtr->routines[Dcm_DspRoutineActive];
    fnc = (sub == 0x01u) ? rt->start : ((sub == 0x02u) ? rt->stop : rt->requestResults);
    UDS_TRACE("Dcm/DSP", "0x31 %02X: routine 0x%04X (%s) -> RoutineServices %s(%s)", (unsigned)sub,
              (unsigned)rt->identifier, rt->name,
              (sub == 0x01u) ? "Start" : ((sub == 0x02u) ? "Stop" : "RequestResults"), Dcm_DspOpName(OpStatus));
    r = fnc(&pMsgContext->reqData[3], (uint16)(pMsgContext->reqDataLen - 3u), OpStatus,
            &pMsgContext->resData[3], &outLen, ErrorCode);
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (r == DCM_E_PENDING) {
        return DCM_E_PENDING;
    }
    if (r != E_OK) {
        return E_NOT_OK;
    }
    if (sub == 0x01u) {
        Dcm_DspRoutineStarted[Dcm_DspRoutineActive] = TRUE;
    } else if (sub == 0x02u) {
        Dcm_DspRoutineStarted[Dcm_DspRoutineActive] = FALSE;
    } else {
        /* requestResults keeps the state */
    }
    pMsgContext->resData[0] = sub;
    pMsgContext->resData[1] = (uint8)(rt->identifier >> 8);
    pMsgContext->resData[2] = (uint8)(rt->identifier & 0xFFu);
    pMsgContext->resDataLen = (Dcm_MsgLenType)(3u + outLen);
    return E_OK;
}

/* ================================================================== */
/* 0x3E TesterPresent (SWS_Dcm_00251)                                  */
/* ================================================================== */
Std_ReturnType Dcm_DspTesterPresent(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                    Dcm_NegativeResponseCodeType *ErrorCode)
{
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (pMsgContext->reqDataLen != 1u) {
        *ErrorCode = DCM_E_INCORRECTMESSAGELENGTHORINVALIDFORMAT;
        return E_NOT_OK;
    }
    /* Nothing to do: the effect (S3 restart) happens in the DSL when the
     * request is finished. */
    pMsgContext->resData[0] = 0x00u;
    pMsgContext->resDataLen = 1u;
    return E_OK;
}
