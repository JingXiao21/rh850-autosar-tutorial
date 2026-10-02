/*
 * VehicleInfoSWC.c
 *
 * [Educational Implementation] demo application SW-C.
 * Real AUTOSAR counterpart: an application (or ECU-specific "DiagApp") SW-C
 * that provides DataServices_* / RoutineServices_* server ports to the Dcm.
 * It knows nothing about CAN, ISO-TP, SIDs or NRC 0x78: it only implements
 * runnables with the port-interface signatures and uses its own Rte_ header.
 *
 * Server runnables:
 *   VehicleInfoSWC_ReadVin            DID F190, async: returns DCM_E_PENDING
 *                                     a configurable number of times first
 *   VehicleInfoSWC_ReadSwVersion      DID F187, sync
 *   VehicleInfoSWC_ReadDiagConfig     DID F1A0, sync (RAM mirror of NvM block)
 *   VehicleInfoSWC_WriteDiagConfig    DID F1A0, async: waits for NvM job
 *   VehicleInfoSWC_SelfTest*          Routine FF00 Start/Stop/RequestResults
 * Cyclic runnable VehicleInfoSWC_Run10ms advances the self test.
 *
 * The UDS_TRACE calls are part of the teaching harness, not something a
 * production SW-C would contain.
 */
#include "Rte_VehicleInfoSWC.h"
#include "UdsTrace.h"
#include <string.h>

#define VEHINFO_VIN_LENGTH         17u
#define VEHINFO_SWVER_LENGTH        8u
#define VEHINFO_SELFTEST_DURATION 100u   /* ms */

#define SELFTEST_STATUS_RUNNING    0x01u
#define SELFTEST_STATUS_COMPLETED  0x02u
#define SELFTEST_STATUS_ABORTED    0x03u

static const uint8 VehicleInfoSWC_Vin[VEHINFO_VIN_LENGTH] = {
    'L', 'R', 'H', '8', '5', '0', 'D', 'E', 'M', 'O', '0', '0', '0', '0', '0', '0', '1'
};
static const uint8 VehicleInfoSWC_SwVersion[VEHINFO_SWVER_LENGTH] = { 'S', 'W', '0', '1', '0', '2', '0', '3' };

static uint8 VehicleInfoSWC_ConfigMirror[NVM_DIAGCONFIG_BLOCK_SIZE];   /* NvM RAM block */
static uint8 VehicleInfoSWC_ConfigStaging[NVM_DIAGCONFIG_BLOCK_SIZE];  /* must stay stable while NvM writes */

static uint8 VehicleInfoSWC_VinPendingCycles = 1u;   /* how often ReadVin answers DCM_E_PENDING */
static uint8 VehicleInfoSWC_VinPendingLeft;

static uint8  VehicleInfoSWC_SelfTestStatus;
static uint16 VehicleInfoSWC_SelfTestElapsed;
static boolean VehicleInfoSWC_SelfTestActive;

void VehicleInfoSWC_SetVinPendingCycles(uint8 cycles)
{
    VehicleInfoSWC_VinPendingCycles = cycles;
}

void VehicleInfoSWC_Init(void)
{
    (void)Rte_Call_NvM_DiagConfig_ReadBlock(VehicleInfoSWC_ConfigMirror);
    VehicleInfoSWC_SelfTestActive = FALSE;
    VehicleInfoSWC_SelfTestStatus = 0u;
    VehicleInfoSWC_VinPendingLeft = 0u;
}

void VehicleInfoSWC_Run10ms(void)
{
    if (VehicleInfoSWC_SelfTestActive) {
        VehicleInfoSWC_SelfTestElapsed = (uint16)(VehicleInfoSWC_SelfTestElapsed + 10u);
        if (VehicleInfoSWC_SelfTestElapsed >= VEHINFO_SELFTEST_DURATION) {
            VehicleInfoSWC_SelfTestActive = FALSE;
            VehicleInfoSWC_SelfTestStatus = SELFTEST_STATUS_COMPLETED;
            UDS_TRACE("SWC", "VehicleInfoSWC_Run10ms: self test finished after %u ms", (unsigned)VehicleInfoSWC_SelfTestElapsed);
        }
    }
}

/* DataServices_DID_F190.ReadData (asynchronous):
 * Simulates data that is not immediately available (e.g. read from another
 * ECU or from slow memory). OUT data is only valid when E_OK is returned. */
Std_ReturnType VehicleInfoSWC_ReadVin(Dcm_OpStatusType OpStatus, uint8 *Data)
{
    if (OpStatus == DCM_CANCEL) {
        VehicleInfoSWC_VinPendingLeft = 0u;
        UDS_TRACE("SWC", "VehicleInfoSWC_ReadVin(DCM_CANCEL): abort");
        return E_OK;
    }
    if (OpStatus == DCM_INITIAL) {
        VehicleInfoSWC_VinPendingLeft = VehicleInfoSWC_VinPendingCycles;
    }
    if (VehicleInfoSWC_VinPendingLeft != 0u) {
        VehicleInfoSWC_VinPendingLeft--;
        UDS_TRACE("SWC", "VehicleInfoSWC_ReadVin: VIN not ready yet -> DCM_E_PENDING (%u more)",
                  (unsigned)VehicleInfoSWC_VinPendingLeft);
        return DCM_E_PENDING;
    }
    (void)memcpy(Data, VehicleInfoSWC_Vin, VEHINFO_VIN_LENGTH);
    UDS_TRACE("SWC", "VehicleInfoSWC_ReadVin: VIN \"%.17s\" copied -> E_OK", (const char *)VehicleInfoSWC_Vin);
    return E_OK;
}

Std_ReturnType VehicleInfoSWC_ReadSwVersion(uint8 *Data)
{
    (void)memcpy(Data, VehicleInfoSWC_SwVersion, VEHINFO_SWVER_LENGTH);
    UDS_TRACE("SWC", "VehicleInfoSWC_ReadSwVersion: \"%.8s\"", (const char *)VehicleInfoSWC_SwVersion);
    return E_OK;
}

Std_ReturnType VehicleInfoSWC_ReadDiagConfig(uint8 *Data)
{
    (void)memcpy(Data, VehicleInfoSWC_ConfigMirror, NVM_DIAGCONFIG_BLOCK_SIZE);
    return E_OK;
}

/* DataServices_DID_F1A0.WriteData (asynchronous, fixed length):
 * INITIAL -> request NvM write and return DCM_E_PENDING;
 * PENDING -> poll NvM until the job is finished. */
Std_ReturnType VehicleInfoSWC_WriteDiagConfig(const uint8 *Data, Dcm_OpStatusType OpStatus,
                                              Dcm_NegativeResponseCodeType *ErrorCode)
{
    NvM_RequestResultType res = NVM_REQ_NOT_OK;

    if (OpStatus == DCM_CANCEL) {
        return E_OK;   /* real SW-C: NvM_CancelJobs may be needed (SWS_Dcm_01048) */
    }
    if (OpStatus == DCM_INITIAL) {
        (void)memcpy(VehicleInfoSWC_ConfigStaging, Data, NVM_DIAGCONFIG_BLOCK_SIZE);
        if (Rte_Call_NvM_DiagConfig_WriteBlock(VehicleInfoSWC_ConfigStaging) != E_OK) {
            *ErrorCode = DCM_E_CONDITIONSNOTCORRECT;
            return E_NOT_OK;
        }
        UDS_TRACE("SWC", "VehicleInfoSWC_WriteDiagConfig: NvM write requested -> DCM_E_PENDING");
        return DCM_E_PENDING;
    }
    (void)Rte_Call_NvM_DiagConfig_GetErrorStatus(&res);
    if (res == NVM_REQ_PENDING) {
        return DCM_E_PENDING;
    }
    if (res != NVM_REQ_OK) {
        *ErrorCode = DCM_E_GENERALPROGRAMMINGFAILURE;
        return E_NOT_OK;
    }
    (void)memcpy(VehicleInfoSWC_ConfigMirror, VehicleInfoSWC_ConfigStaging, NVM_DIAGCONFIG_BLOCK_SIZE);
    UDS_TRACE("SWC", "VehicleInfoSWC_WriteDiagConfig: NvM job OK -> E_OK");
    return E_OK;
}

/* RoutineServices_Routine_FF00 */
Std_ReturnType VehicleInfoSWC_SelfTestStart(Dcm_OpStatusType OpStatus, Dcm_NegativeResponseCodeType *ErrorCode)
{
    (void)ErrorCode;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    VehicleInfoSWC_SelfTestActive = TRUE;
    VehicleInfoSWC_SelfTestElapsed = 0u;
    VehicleInfoSWC_SelfTestStatus = SELFTEST_STATUS_RUNNING;
    UDS_TRACE("SWC", "VehicleInfoSWC_SelfTestStart: started (session seen via Rte_Mode = 0x%02X)",
              (unsigned)Rte_Mode_DcmDiagnosticSessionControl_DcmDiagnosticSessionControl());
    return E_OK;
}

Std_ReturnType VehicleInfoSWC_SelfTestStop(Dcm_OpStatusType OpStatus, Dcm_NegativeResponseCodeType *ErrorCode)
{
    (void)ErrorCode;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    if (VehicleInfoSWC_SelfTestActive) {
        VehicleInfoSWC_SelfTestStatus = SELFTEST_STATUS_ABORTED;
    }
    VehicleInfoSWC_SelfTestActive = FALSE;
    UDS_TRACE("SWC", "VehicleInfoSWC_SelfTestStop");
    return E_OK;
}

Std_ReturnType VehicleInfoSWC_SelfTestRequestResults(Dcm_OpStatusType OpStatus, uint8 *Out_RoutineStatus,
                                                     Dcm_NegativeResponseCodeType *ErrorCode)
{
    (void)ErrorCode;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    *Out_RoutineStatus = VehicleInfoSWC_SelfTestStatus;
    UDS_TRACE("SWC", "VehicleInfoSWC_SelfTestRequestResults: status 0x%02X", (unsigned)VehicleInfoSWC_SelfTestStatus);
    return E_OK;
}
