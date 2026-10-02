/*
 * Det.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Det module. See Det.h.
 */
#include "Det.h"
#include "UdsTrace.h"

static Det_ErrorRecordType Det_Records[DET_MAX_RECORDS];
static uint16 Det_RecordCount;
static uint16 Det_DevCount;
static uint16 Det_RuntimeCount;

static void Det_Store(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId, boolean runtime)
{
    if (Det_RecordCount < DET_MAX_RECORDS) {
        Det_Records[Det_RecordCount].moduleId = ModuleId;
        Det_Records[Det_RecordCount].instanceId = InstanceId;
        Det_Records[Det_RecordCount].apiId = ApiId;
        Det_Records[Det_RecordCount].errorId = ErrorId;
        Det_Records[Det_RecordCount].runtime = runtime;
        Det_RecordCount++;
    }
}

void Det_Init(const Det_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    Det_RecordCount = 0u;
    Det_DevCount = 0u;
    Det_RuntimeCount = 0u;
}

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    Det_Store(ModuleId, InstanceId, ApiId, ErrorId, FALSE);
    Det_DevCount++;
    UDS_TRACE("Det", "DEVELOPMENT ERROR module=%u instance=%u api=0x%02X error=0x%02X",
              (unsigned)ModuleId, (unsigned)InstanceId, (unsigned)ApiId, (unsigned)ErrorId);
    return E_OK;
}

Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    Det_Store(ModuleId, InstanceId, ApiId, ErrorId, TRUE);
    Det_RuntimeCount++;
    UDS_TRACE("Det", "RUNTIME ERROR module=%u instance=%u api=0x%02X error=0x%02X",
              (unsigned)ModuleId, (unsigned)InstanceId, (unsigned)ApiId, (unsigned)ErrorId);
    return E_OK;
}

uint16 Det_GetDevErrorCount(void) { return Det_DevCount; }
uint16 Det_GetRuntimeErrorCount(void) { return Det_RuntimeCount; }

const Det_ErrorRecordType *Det_GetRecord(uint16 index)
{
    return (index < Det_RecordCount) ? &Det_Records[index] : (const Det_ErrorRecordType *)NULL_PTR;
}
