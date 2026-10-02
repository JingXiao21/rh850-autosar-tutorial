/*
 * NvM.c
 *
 * [Educational Implementation] NvM STUB. See NvM.h.
 */
#include "NvM.h"
#include "UdsTrace.h"
#include <string.h>

#define NVM_WRITE_DURATION_CYCLES 2u   /* "flash programming" takes 2 NvM_MainFunction calls */

static uint8   NvM_EmulatedFlash[NVM_DIAGCONFIG_BLOCK_SIZE];   /* survives simulated reset */
static boolean NvM_FlashValid;
static NvM_RequestResultType NvM_BlockResult;
static const uint8 *NvM_JobSrc;
static uint8   NvM_JobCycles;
static boolean NvM_JobActive;

static const uint8 NvM_RomDefault[NVM_DIAGCONFIG_BLOCK_SIZE] = { 0u };

void NvM_Init(const NvM_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    NvM_JobActive = FALSE;
    NvM_BlockResult = NVM_REQ_OK;
}

void NvM_ReadAll(void)
{
    if (!NvM_FlashValid) {
        (void)memcpy(NvM_EmulatedFlash, NvM_RomDefault, sizeof(NvM_EmulatedFlash));  /* first power-up */
        NvM_FlashValid = TRUE;
    }
    UDS_TRACE("NvM", "ReadAll: DiagConfig block = [%s]", UdsTrace_Hex(NvM_EmulatedFlash, NVM_DIAGCONFIG_BLOCK_SIZE));
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr)
{
    if ((BlockId != NvMConf_NvMBlockDescriptor_DiagConfig) || (NvM_DstPtr == NULL_PTR)) {
        return E_NOT_OK;
    }
    (void)memcpy(NvM_DstPtr, NvM_EmulatedFlash, NVM_DIAGCONFIG_BLOCK_SIZE);
    return E_OK;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr)
{
    if ((BlockId != NvMConf_NvMBlockDescriptor_DiagConfig) || (NvM_SrcPtr == NULL_PTR) || NvM_JobActive) {
        return E_NOT_OK;
    }
    NvM_JobSrc = (const uint8 *)NvM_SrcPtr;
    NvM_JobCycles = NVM_WRITE_DURATION_CYCLES;
    NvM_JobActive = TRUE;
    NvM_BlockResult = NVM_REQ_PENDING;
    UDS_TRACE("NvM", "WriteBlock(DiagConfig) queued -> NVM_REQ_PENDING");
    return E_OK;
}

Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr)
{
    if ((BlockId != NvMConf_NvMBlockDescriptor_DiagConfig) || (RequestResultPtr == NULL_PTR)) {
        return E_NOT_OK;
    }
    *RequestResultPtr = NvM_BlockResult;
    return E_OK;
}

void NvM_MainFunction(void)
{
    if (!NvM_JobActive) {
        return;
    }
    if (NvM_JobCycles > 1u) {
        NvM_JobCycles--;
        return;
    }
    (void)memcpy(NvM_EmulatedFlash, NvM_JobSrc, NVM_DIAGCONFIG_BLOCK_SIZE);   /* RAM block -> "flash" */
    NvM_JobActive = FALSE;
    NvM_BlockResult = NVM_REQ_OK;
    UDS_TRACE("NvM", "MainFunction: DiagConfig written to emulated data flash -> NVM_REQ_OK");
}
