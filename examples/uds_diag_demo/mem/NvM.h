/*
 * NvM.h
 *
 * [Educational Implementation] NvM STUB.
 * Real AUTOSAR counterpart: NvM (NVRAM Manager) on top of MemIf -> Fee -> Fls
 * (on RH850: data flash via the Renesas FCL/FDL or the MCAL Fls driver).
 * Kept from the real contract: write requests are ASYNCHRONOUS - NvM_WriteBlock
 * only queues the job, the copy happens later in NvM_MainFunction, the caller
 * polls NvM_GetErrorStatus and must keep its RAM buffer stable meanwhile.
 * Simplified: one job at a time, "flash" is a RAM array that survives the
 * simulated ECU reset, NvM_ReadBlock completes synchronously, no CRC,
 * no redundant blocks, no NvM_WriteAll at shutdown.
 *
 * No NvM SWS in this repository: signature per R4.x convention, confirm
 * against project release.
 */
#ifndef NVM_H
#define NVM_H

#include "Std_Types.h"

typedef uint16 NvM_BlockIdType;
typedef uint8  NvM_RequestResultType;

#define NVM_REQ_OK        ((NvM_RequestResultType)0u)
#define NVM_REQ_NOT_OK    ((NvM_RequestResultType)1u)
#define NVM_REQ_PENDING   ((NvM_RequestResultType)2u)

/* NvMBlockDescriptor ids (0 and 1 are reserved by AUTOSAR) */
#define NvMConf_NvMBlockDescriptor_DiagConfig   2u
#define NVM_DIAGCONFIG_BLOCK_SIZE               10u

typedef struct {
    uint8 dummy;
} NvM_ConfigType;

void NvM_Init(const NvM_ConfigType *ConfigPtr);
void NvM_ReadAll(void);
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr);
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr);
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr);
void NvM_MainFunction(void);

#endif /* NVM_H */
