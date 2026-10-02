/*
 * Dem.h
 *
 * [Educational Implementation] tiny Dem STUB.
 * Real AUTOSAR counterpart: Dem (Diagnostic Event Manager). A real Dem owns
 * event debouncing, the UDS status byte state machine, event memory in NvM,
 * freeze frames / extended data, operation cycles and aging. This stub only
 * keeps a fixed DTC table so that Dcm 0x14 / 0x19 0x02 have something to talk to.
 *
 * API shape follows the R4.3+ "client" interface that DCM R20-11 uses
 * (Dem_SelectDTC + Dem_ClearDTC(ClientId), Dem_SetDTCFilter + Dem_GetNextFilteredDTC,
 * see research note 02 §3.8.3/§3.8.4/§3.10). This repository has no Dem SWS:
 * signatures and return code values per R4.x convention, confirm against
 * project release.
 */
#ifndef DEM_H
#define DEM_H

#include "Std_Types.h"

typedef uint8  Dem_DTCFormatType;
typedef uint16 Dem_DTCOriginType;
typedef uint8  Dem_UdsStatusByteType;
typedef uint8  Dem_DTCSeverityType;

#define DEM_DTC_FORMAT_UDS              ((Dem_DTCFormatType)1u)
#define DEM_DTC_ORIGIN_PRIMARY_MEMORY   ((Dem_DTCOriginType)0x0001u)
#define DEM_DTC_GROUP_ALL_DTCS          0xFFFFFFu

/* Std_ReturnType extensions (values illustrative, confirm in Dem SWS) */
#define DEM_PENDING             ((Std_ReturnType)4u)
#define DEM_CLEAR_BUSY          ((Std_ReturnType)5u)
#define DEM_CLEAR_MEMORY_ERROR  ((Std_ReturnType)6u)
#define DEM_CLEAR_FAILED        ((Std_ReturnType)7u)
#define DEM_WRONG_DTC           ((Std_ReturnType)8u)
#define DEM_WRONG_DTCORIGIN     ((Std_ReturnType)9u)
#define DEM_NO_SUCH_ELEMENT     ((Std_ReturnType)48u)

/* UDS DTC status bits (ISO 14229-1) */
#define DEM_UDS_STATUS_TF       0x01u   /* testFailed                   */
#define DEM_UDS_STATUS_TFTOC    0x02u   /* testFailedThisOperationCycle */
#define DEM_UDS_STATUS_PDTC     0x04u   /* pendingDTC                   */
#define DEM_UDS_STATUS_CDTC     0x08u   /* confirmedDTC                 */

typedef struct {
    uint8 dummy;
} Dem_ConfigType;

void Dem_Init(const Dem_ConfigType *ConfigPtr);
Std_ReturnType Dem_SelectDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);
Std_ReturnType Dem_ClearDTC(uint8 ClientId);
Std_ReturnType Dem_GetDTCStatusAvailabilityMask(uint8 ClientId, Dem_UdsStatusByteType *DTCStatusMask);
Std_ReturnType Dem_SetDTCFilter(uint8 ClientId, uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat,
                                Dem_DTCOriginType DTCOrigin, boolean FilterWithSeverity,
                                Dem_DTCSeverityType DTCSeverityMask, boolean FilterForFaultDetectionCounter);
Std_ReturnType Dem_GetNextFilteredDTC(uint8 ClientId, uint32 *DTC, Dem_UdsStatusByteType *DTCStatus);

/* [Educational Implementation] stands in for a monitor calling
 * Dem_SetEventStatus(EventId, DEM_EVENT_STATUS_FAILED). Not an AUTOSAR API. */
void Dem_SimSetDtcStatus(uint32 DTC, Dem_UdsStatusByteType status);

#endif /* DEM_H */
