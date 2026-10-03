/*
 * Det.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Default Error Tracer (AUTOSAR_CP_SWS_DefaultErrorTracer; Det_Init,
 * Det_Start, Det_ReportError, Det_ReportRuntimeError, Det_ReportTransientFault). Implemented: each
 * report emits one DET trace line and increments counters; development errors are stored in a small
 * ring buffer readable by tests; no callbacks, no DLT. Owner: agent C.
 */
#ifndef DET_H
#define DET_H

#include "Std_Types.h"
#include "Mini_ModuleIds.h"

typedef struct { uint8 reserved; } Det_ConfigType;
extern const Det_ConfigType Det_Config;          /* generated Det_Cfg.c (EcuM_Cfg.c) */

typedef struct {
    uint16 moduleId;
    uint8  instanceId;
    uint8  apiId;
    uint8  errorId;
    uint8  kind;               /* 0 = development error, 1 = runtime error, 2 = transient fault */
} Det_EntryType;

void           Det_Init(const Det_ConfigType *ConfigPtr);
void           Det_Start(void);
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);   /* returns E_OK */
Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);
Std_ReturnType Det_ReportTransientFault(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 FaultId);
void           Det_GetVersionInfo(Std_VersionInfoType *versioninfo);

/* educational test access */
uint16         Det_GetErrorCount(void);                         /* all kinds since Det_Init */
boolean        Det_GetEntry(uint16 index, Det_EntryType *entry);/* index 0 = oldest of the last 16 */

#endif /* DET_H */
