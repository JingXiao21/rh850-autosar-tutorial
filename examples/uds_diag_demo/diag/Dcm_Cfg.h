/*
 * Dcm_Cfg.h
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: Dcm_Cfg.h / Dcm_Lcfg.h generated from the Dcm
 * ECUC (DcmConfigSet: DcmDsl / DcmDsd / DcmDsp). Container names in the
 * comments are the R20-11 ECUC names (see research note 02, section 3.13).
 *
 * Real generators keep DID -> DidInfo -> DidRead/DidWrite -> DcmDspData as
 * separate containers referencing each other. This demo flattens them into
 * one Dcm_DspDidType row so the whole DID can be read in one place.
 */
#ifndef DCM_CFG_H
#define DCM_CFG_H

#include "Dcm_Types.h"

/* ---------------- general (DcmGeneral) ---------------- */
#define DCM_DEV_ERROR_DETECT            STD_ON
#define DCM_TASK_TIME_MS                10u    /* DcmTaskTime = 0.010 s                  */
#define DCM_RESPOND_ALL_REQUEST         STD_ON /* DcmRespondAllRequest                   */

/* ---------------- DSL (DcmDslBuffer / DcmDslProtocol) ---------------- */
#define DCM_DSL_BUFFER_SIZE             128u   /* DcmDslBufferSize (rx and tx each)      */
#define DCM_DSL_MAX_NUM_RESP_PEND       20u    /* DcmDslDiagRespMaxNumRespPend           */
#define DCM_TIM_P2_SERVER_ADJUST_MS     10u    /* DcmTimStrP2ServerAdjust                */
#define DCM_TIM_P2STAR_SERVER_ADJUST_MS 100u   /* DcmTimStrP2StarServerAdjust            */
#define DCM_S3_SERVER_MS                5000u  /* S3Server, fixed 5 s (SWS_Dcm_00143)    */

/* DcmDslProtocolRxPduId / DcmDslProtocolTxConfirmationPduId */
#define DcmConf_DcmDslProtocolRx_DiagPhys   0u
#define DcmConf_DcmDslProtocolRx_DiagFunc   1u
#define DcmConf_DcmDslProtocolTx_DiagResp   0u

/* ---------------- DSP limits ---------------- */
#define DCM_DSP_MAX_DID_TO_READ         4u     /* DcmDspMaxDidToRead                     */
#define DCM_DEM_CLIENT_ID               0u     /* DcmDemClientRef                        */

/* ---------------- access masks ----------------
 * Real config: lists of references to DcmDspSessionRow / DcmDspSecurityRow.
 * Demo: one bit per session / security level; access is granted if the bit
 * of the ACTIVE session / level is set. DCM_SEC_ANY = no security required
 * (same as an empty SecurityLevelRef list in the real configuration). */
#define DCM_SES_MASK(ses)    ((uint32)1u << ((ses) & 0x1Fu))
#define DCM_SEC_MASK(lev)    ((uint32)1u << ((lev) & 0x1Fu))
#define DCM_SES_DEFAULT      DCM_SES_MASK(DCM_DEFAULT_SESSION)
#define DCM_SES_EXTENDED     DCM_SES_MASK(DCM_EXTENDED_DIAGNOSTIC_SESSION)
#define DCM_SES_ALL          (DCM_SES_DEFAULT | DCM_SES_EXTENDED | DCM_SES_MASK(DCM_PROGRAMMING_SESSION))
#define DCM_SEC_ANY          0xFFFFFFFFu
#define DCM_SEC_LEVEL1       DCM_SEC_MASK(1u)

/* ---------------- DcmDspSessionRow ---------------- */
typedef struct {
    Dcm_SesCtrlType level;              /* DcmDspSessionLevel                        */
    uint16          p2ServerMaxMs;      /* DcmDspSessionP2ServerMax                  */
    uint16          p2StarServerMaxMs;  /* DcmDspSessionP2StarServerMax              */
    const char     *name;
} Dcm_DspSessionRowType;

/* ---------------- DcmDspSecurityRow (DcmDspSecurityUsePort = USE_ASYNCH_CLIENT_SERVER) */
typedef Std_ReturnType (*Dcm_GetSeedFncType)(Dcm_OpStatusType OpStatus, uint8 *Seed,
                                             Dcm_NegativeResponseCodeType *ErrorCode);
typedef Std_ReturnType (*Dcm_CompareKeyFncType)(const uint8 *Key, Dcm_OpStatusType OpStatus,
                                                Dcm_NegativeResponseCodeType *ErrorCode);
typedef struct {
    Dcm_SecLevelType      level;        /* DcmDspSecurityLevel (requestSeed = 2*level-1) */
    uint8                 seedSize;     /* DcmDspSecuritySeedSize                    */
    uint8                 keySize;      /* DcmDspSecurityKeySize                     */
    uint8                 numAttDelay;  /* DcmDspSecurityNumAttDelay                 */
    uint16                delayTimeMs;  /* DcmDspSecurityDelayTime                   */
    Dcm_GetSeedFncType    getSeed;      /* -> Rte_Call_SecurityAccess_<Level>_GetSeed */
    Dcm_CompareKeyFncType compareKey;   /* -> Rte_Call_SecurityAccess_<Level>_CompareKey */
    const char           *name;
} Dcm_DspSecurityRowType;

/* ---------------- DcmDspDid + DcmDspData ---------------- */
typedef enum {
    DCM_USE_DATA_SYNCH_CLIENT_SERVER = 0,  /* ReadData(Data)            - no OpStatus  */
    DCM_USE_DATA_ASYNCH_CLIENT_SERVER      /* ReadData(OpStatus, Data)  - may PENDING  */
} Dcm_DspDataUsePortType;

typedef Std_ReturnType (*Dcm_ReadDataSyncFncType)(uint8 *Data);
typedef Std_ReturnType (*Dcm_ReadDataAsyncFncType)(Dcm_OpStatusType OpStatus, uint8 *Data);
typedef Std_ReturnType (*Dcm_WriteDataAsyncFncType)(const uint8 *Data, Dcm_OpStatusType OpStatus,
                                                    Dcm_NegativeResponseCodeType *ErrorCode);
typedef struct {
    uint16                    identifier;        /* DcmDspDidIdentifier          */
    uint16                    size;              /* DcmDspDataByteSize (fixed)   */
    Dcm_DspDataUsePortType    usePort;           /* DcmDspDataUsePort            */
    Dcm_ReadDataSyncFncType   readSync;          /* used if SYNCH                */
    Dcm_ReadDataAsyncFncType  readAsync;         /* used if ASYNCH               */
    Dcm_WriteDataAsyncFncType writeAsync;        /* NULL_PTR: no DcmDspDidWrite  */
    uint32                    readSessionMask;   /* DcmDspDidReadSessionRef      */
    uint32                    readSecurityMask;  /* DcmDspDidReadSecurityLevelRef */
    uint32                    writeSessionMask;  /* DcmDspDidWriteSessionRef     */
    uint32                    writeSecurityMask; /* DcmDspDidWriteSecurityLevelRef */
    const char               *name;
} Dcm_DspDidType;

/* ---------------- DcmDspRoutine (DcmDspRoutineUsePort = TRUE) -------- */
/* Uniform internal shape; Dcm_Cfg.c contains generated "glue" that adapts
 * it to the configured RoutineServices_<Name> operation signatures. */
typedef Std_ReturnType (*Dcm_RoutineFncType)(const uint8 *InBuffer, uint16 InLength, Dcm_OpStatusType OpStatus,
                                             uint8 *OutBuffer, uint16 *OutLength,
                                             Dcm_NegativeResponseCodeType *ErrorCode);
typedef struct {
    uint16             identifier;     /* DcmDspRoutineIdentifier                       */
    uint16             startInLen;     /* fixed length of routineControlOptionRecord    */
    Dcm_RoutineFncType start;          /* DcmDspStartRoutine                            */
    Dcm_RoutineFncType stop;           /* DcmDspStopRoutine (NULL: not supported)       */
    Dcm_RoutineFncType requestResults; /* DcmDspRequestRoutineResults (NULL: n/a)       */
    uint32             sessionMask;    /* DcmDspCommonAuthorization session refs        */
    uint32             securityMask;   /* DcmDspCommonAuthorization security refs       */
    const char        *name;
} Dcm_DspRoutineType;

/* ---------------- DcmDsdServiceTable ---------------- */
typedef Std_ReturnType (*Dcm_DsdServiceFncType)(Dcm_OpStatusType OpStatus, Dcm_MsgContextType *pMsgContext,
                                                Dcm_NegativeResponseCodeType *ErrorCode);
typedef struct {
    uint8  subFunctionId;               /* DcmDsdSubServiceId                         */
    uint32 sessionMask;                 /* DcmDsdSubServiceSessionLevelRef            */
    uint32 securityMask;                /* DcmDsdSubServiceSecurityLevelRef           */
} Dcm_DsdSubServiceType;

typedef struct {
    uint8                        sid;            /* DcmDsdSidTabServiceId                */
    boolean                      subFuncAvail;   /* DcmDsdSidTabSubfuncAvail             */
    uint8                        minReqLen;      /* incl. SID; shorter -> NRC 0x13       */
    uint32                       sessionMask;    /* DcmDsdSidTabSessionLevelRef          */
    uint32                       securityMask;   /* DcmDsdSidTabSecurityLevelRef         */
    const Dcm_DsdSubServiceType *subServices;    /* NULL: sub-function checked by DSP   */
    uint8                        numSubServices;
    Dcm_DsdServiceFncType        fnc;            /* DcmDsdSidTabFnc / internal DSP       */
    const char                  *name;
} Dcm_DsdServiceType;

/* ---------------- DcmConfigSet ---------------- */
typedef struct {
    const Dcm_DsdServiceType     *services;
    uint8                         numServices;
    const Dcm_DspSessionRowType  *sessionRows;
    uint8                         numSessionRows;
    const Dcm_DspSecurityRowType *securityRows;
    uint8                         numSecurityRows;
    const Dcm_DspDidType         *dids;
    uint8                         numDids;
    const Dcm_DspRoutineType     *routines;
    uint8                         numRoutines;
} Dcm_ConfigType;

extern const Dcm_ConfigType Dcm_Config;

#endif /* DCM_CFG_H */
