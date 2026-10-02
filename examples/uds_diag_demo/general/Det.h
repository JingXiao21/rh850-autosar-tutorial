/*
 * Det.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Det (Default Error Tracer, AUTOSAR_SWS_DefaultErrorTracer).
 * In a real ECU Det_ReportError usually ends in a breakpoint hook / debugger
 * halt or a log buffer read via a debugger. Here every report is recorded in
 * a small array (so host tests can assert "no DET error happened") and is
 * printed through the trace.
 *
 * Signature per R4.x convention (Det_ReportError returns Std_ReturnType since
 * R4.1), confirm against project release.
 */
#ifndef DET_H
#define DET_H

#include "Std_Types.h"

/* AUTOSAR module IDs (from the AUTOSAR BSW module list). */
#define DET_MODULE_ID_CAN   80u
#define DET_MODULE_ID_CANIF 60u
#define DET_MODULE_ID_CANTP 35u
#define DET_MODULE_ID_PDUR  51u
#define DET_MODULE_ID_DCM   53u
#define DET_MODULE_ID_DEM   54u
#define DET_MODULE_ID_NVM   20u

typedef struct {
    uint8 dummy; /* Det has no post-build configuration in this demo */
} Det_ConfigType;

typedef struct {
    uint16  moduleId;
    uint8   instanceId;
    uint8   apiId;
    uint8   errorId;
    boolean runtime;  /* TRUE: Det_ReportRuntimeError, FALSE: development error */
} Det_ErrorRecordType;

#define DET_MAX_RECORDS 32u

void Det_Init(const Det_ConfigType *ConfigPtr);
Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);
Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);

/* [Educational Implementation] helpers for host tests (not AUTOSAR APIs). */
uint16 Det_GetDevErrorCount(void);
uint16 Det_GetRuntimeErrorCount(void);
const Det_ErrorRecordType *Det_GetRecord(uint16 index);

#endif /* DET_H */
