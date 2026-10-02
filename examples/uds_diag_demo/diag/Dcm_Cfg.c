/*
 * Dcm_Cfg.c
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: Dcm_Cfg.c / Dcm_Lcfg.c / Dcm_PBcfg.c produced by
 * the Dcm configurator (e.g. RTA-CAR / DaVinci / EB tresos) from the
 * diagnostic description (CDD/ODX -> ECUC). Nobody writes this by hand in a
 * real project - but reading it answers most "how does Dcm know ...?" questions:
 *
 *   "How does Dcm know 0x22 is ReadDataByIdentifier?"   -> Dcm_Services[] row 0x22
 *   "How does Dcm find F190?"                            -> Dcm_Dids[] row 0xF190
 *   "How does Dcm call the application?"                 -> function pointer to
 *                                                           Rte_Call_DataServices_DID_F190_ReadData
 */
#include "Dcm_Internal.h"
#include "Rte_Dcm.h"

/* ---------------- DcmDspSession ---------------- */
static const Dcm_DspSessionRowType Dcm_SessionRows[] = {
    /* level                             P2    P2*   name */
    { DCM_DEFAULT_SESSION,               50u, 5000u, "DefaultSession" },
    { DCM_EXTENDED_DIAGNOSTIC_SESSION,   50u, 5000u, "ExtendedDiagnosticSession" }
};

/* ---------------- DcmDspSecurity ---------------- */
static const Dcm_DspSecurityRowType Dcm_SecurityRows[] = {
    /* level seed key numAttDelay delay(ms) */
    { 1u, 4u, 4u, 3u, 3000u,
      Rte_Call_SecurityAccess_Level_01_GetSeed, Rte_Call_SecurityAccess_Level_01_CompareKey, "Level_01" }
};

/* ---------------- DcmDspDid / DcmDspData ---------------- */
static const Dcm_DspDidType Dcm_Dids[] = {
    {   /* VIN: asynchronous C/S interface -> exercises DCM_E_PENDING / OpStatus */
        0xF190u, 17u, DCM_USE_DATA_ASYNCH_CLIENT_SERVER,
        NULL_PTR, Rte_Call_DataServices_DID_F190_ReadData, NULL_PTR,
        DCM_SES_ALL, DCM_SEC_ANY, 0u, 0u, "VIN"
    },
    {   /* ECU software version: synchronous C/S interface */
        0xF187u, 8u, DCM_USE_DATA_SYNCH_CLIENT_SERVER,
        Rte_Call_DataServices_DID_F187_ReadData, NULL_PTR, NULL_PTR,
        DCM_SES_ALL, DCM_SEC_ANY, 0u, 0u, "SparePartNumber/SwVersion"
    },
    {   /* writable configuration stored in NvM: write needs extended session + level 1 */
        0xF1A0u, 10u, DCM_USE_DATA_SYNCH_CLIENT_SERVER,
        Rte_Call_DataServices_DID_F1A0_ReadData, NULL_PTR, Rte_Call_DataServices_DID_F1A0_WriteData,
        DCM_SES_ALL, DCM_SEC_ANY, DCM_SES_EXTENDED, DCM_SEC_LEVEL1, "DiagConfig(NvM)"
    }
};

/* ---------------- DcmDspRoutine ----------------
 * "Generated glue": adapts the uniform Dcm-internal routine shape to the
 * configured RoutineServices_Routine_FF00 operations (no in/out signals for
 * Start/Stop, one 1-byte out signal for RequestResults). */
static Std_ReturnType Dcm_Cfg_Routine_FF00_Start(const uint8 *InBuffer, uint16 InLength, Dcm_OpStatusType OpStatus,
                                                 uint8 *OutBuffer, uint16 *OutLength,
                                                 Dcm_NegativeResponseCodeType *ErrorCode)
{
    (void)InBuffer;
    (void)InLength;
    (void)OutBuffer;
    *OutLength = 0u;
    return Rte_Call_RoutineServices_Routine_FF00_Start(OpStatus, ErrorCode);
}

static Std_ReturnType Dcm_Cfg_Routine_FF00_Stop(const uint8 *InBuffer, uint16 InLength, Dcm_OpStatusType OpStatus,
                                                uint8 *OutBuffer, uint16 *OutLength,
                                                Dcm_NegativeResponseCodeType *ErrorCode)
{
    (void)InBuffer;
    (void)InLength;
    (void)OutBuffer;
    *OutLength = 0u;
    return Rte_Call_RoutineServices_Routine_FF00_Stop(OpStatus, ErrorCode);
}

static Std_ReturnType Dcm_Cfg_Routine_FF00_RequestResults(const uint8 *InBuffer, uint16 InLength,
                                                          Dcm_OpStatusType OpStatus, uint8 *OutBuffer,
                                                          uint16 *OutLength, Dcm_NegativeResponseCodeType *ErrorCode)
{
    (void)InBuffer;
    (void)InLength;
    *OutLength = 1u;
    return Rte_Call_RoutineServices_Routine_FF00_RequestResults(OpStatus, &OutBuffer[0], ErrorCode);
}

static const Dcm_DspRoutineType Dcm_Routines[] = {
    { 0xFF00u, 0u, Dcm_Cfg_Routine_FF00_Start, Dcm_Cfg_Routine_FF00_Stop, Dcm_Cfg_Routine_FF00_RequestResults,
      DCM_SES_EXTENDED, DCM_SEC_ANY, "Routine_FF00_SelfTest" }
};

/* ---------------- DcmDsdServiceTable ---------------- */
static const Dcm_DsdSubServiceType Dcm_Sub10[] = {
    { 0x01u, DCM_SES_ALL, DCM_SEC_ANY },
    { 0x03u, DCM_SES_ALL, DCM_SEC_ANY }
};
static const Dcm_DsdSubServiceType Dcm_Sub11[] = {
    { 0x01u, DCM_SES_ALL, DCM_SEC_ANY },
    { 0x03u, DCM_SES_ALL, DCM_SEC_ANY }
};
static const Dcm_DsdSubServiceType Dcm_Sub19[] = {
    { 0x02u, DCM_SES_DEFAULT | DCM_SES_EXTENDED, DCM_SEC_ANY }
};
static const Dcm_DsdSubServiceType Dcm_Sub27[] = {
    { 0x01u, DCM_SES_EXTENDED, DCM_SEC_ANY },
    { 0x02u, DCM_SES_EXTENDED, DCM_SEC_ANY }
};
static const Dcm_DsdSubServiceType Dcm_Sub3E[] = {
    { 0x00u, DCM_SES_ALL, DCM_SEC_ANY }
};

#define DCM_N(a) ((uint8)(sizeof(a) / sizeof((a)[0])))

static const Dcm_DsdServiceType Dcm_Services[] = {
    /* SID   subFn  minLen sessions                         security     sub-table          handler */
    { 0x10u, TRUE,  2u, DCM_SES_ALL,                        DCM_SEC_ANY, Dcm_Sub10, DCM_N(Dcm_Sub10), Dcm_DspDiagnosticSessionControl,   "DiagnosticSessionControl" },
    { 0x11u, TRUE,  2u, DCM_SES_ALL,                        DCM_SEC_ANY, Dcm_Sub11, DCM_N(Dcm_Sub11), Dcm_DspEcuReset,                   "ECUReset" },
    { 0x14u, FALSE, 4u, DCM_SES_DEFAULT | DCM_SES_EXTENDED, DCM_SEC_ANY, NULL_PTR,  0u,               Dcm_DspClearDiagnosticInformation, "ClearDiagnosticInformation" },
    { 0x19u, TRUE,  3u, DCM_SES_DEFAULT | DCM_SES_EXTENDED, DCM_SEC_ANY, Dcm_Sub19, DCM_N(Dcm_Sub19), Dcm_DspReadDTCInformation,         "ReadDTCInformation" },
    { 0x22u, FALSE, 3u, DCM_SES_ALL,                        DCM_SEC_ANY, NULL_PTR,  0u,               Dcm_DspReadDataByIdentifier,       "ReadDataByIdentifier" },
    { 0x27u, TRUE,  2u, DCM_SES_EXTENDED,                   DCM_SEC_ANY, Dcm_Sub27, DCM_N(Dcm_Sub27), Dcm_DspSecurityAccess,             "SecurityAccess" },
    { 0x2Eu, FALSE, 4u, DCM_SES_EXTENDED,                   DCM_SEC_ANY, NULL_PTR,  0u,               Dcm_DspWriteDataByIdentifier,      "WriteDataByIdentifier" },
    { 0x31u, TRUE,  4u, DCM_SES_EXTENDED,                   DCM_SEC_ANY, NULL_PTR,  0u,               Dcm_DspRoutineControl,             "RoutineControl" },
    { 0x3Eu, TRUE,  2u, DCM_SES_ALL,                        DCM_SEC_ANY, Dcm_Sub3E, DCM_N(Dcm_Sub3E), Dcm_DspTesterPresent,              "TesterPresent" }
};

const Dcm_ConfigType Dcm_Config = {
    Dcm_Services,     DCM_N(Dcm_Services),
    Dcm_SessionRows,  DCM_N(Dcm_SessionRows),
    Dcm_SecurityRows, DCM_N(Dcm_SecurityRows),
    Dcm_Dids,         DCM_N(Dcm_Dids),
    Dcm_Routines,     DCM_N(Dcm_Routines)
};
