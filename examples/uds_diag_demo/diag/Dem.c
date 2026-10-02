/*
 * Dem.c
 *
 * [Educational Implementation] tiny Dem STUB. See Dem.h.
 * The DTC table survives a simulated ECU reset (as if stored in NvM):
 * it is only pre-filled on the very first Dem_Init.
 */
#include "Dem.h"
#include "UdsTrace.h"

#define DEM_NUM_DTCS            3u
#define DEM_STATUS_AVAILABILITY 0x7Fu   /* bit 7 (warningIndicatorRequested) not supported */

typedef struct {
    uint32                dtc;
    Dem_UdsStatusByteType status;
} Dem_DtcEntryType;

static Dem_DtcEntryType Dem_Dtcs[DEM_NUM_DTCS];
static boolean Dem_MemoryValid;     /* "event memory read from NvM" */
static uint32  Dem_SelectedDtc;
static boolean Dem_Selected;
static uint8   Dem_ClearPendingCycles;
static uint8   Dem_FilterMask;
static uint8   Dem_FilterIndex;
static boolean Dem_FilterActive;

void Dem_Init(const Dem_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    if (!Dem_MemoryValid) {
        /* Example content: U0100-00 (lost communication with ECM) currently failing
         * and confirmed; P0562-00 (system voltage low) confirmed but not failing now;
         * C0035-00 never failed. 3-byte UDS DTC values. */
        Dem_Dtcs[0].dtc = 0xC10000u; Dem_Dtcs[0].status = (Dem_UdsStatusByteType)(DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TFTOC | DEM_UDS_STATUS_CDTC);
        Dem_Dtcs[1].dtc = 0x056200u; Dem_Dtcs[1].status = (Dem_UdsStatusByteType)DEM_UDS_STATUS_CDTC;
        Dem_Dtcs[2].dtc = 0x403500u; Dem_Dtcs[2].status = 0x00u;
        Dem_MemoryValid = TRUE;
    }
    Dem_Selected = FALSE;
    Dem_FilterActive = FALSE;
    Dem_ClearPendingCycles = 0u;
}

Std_ReturnType Dem_SelectDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin)
{
    (void)ClientId;
    (void)DTCFormat;
    (void)DTCOrigin;
    Dem_SelectedDtc = DTC & 0xFFFFFFu;
    Dem_Selected = TRUE;
    Dem_ClearPendingCycles = 1u;   /* clearing "needs NvM": one DEM_PENDING round */
    return E_OK;
}

Std_ReturnType Dem_ClearDTC(uint8 ClientId)
{
    uint8 i;
    boolean found = FALSE;
    (void)ClientId;

    if (!Dem_Selected) {
        return DEM_WRONG_DTC;
    }
    if (Dem_ClearPendingCycles != 0u) {
        Dem_ClearPendingCycles--;
        UDS_TRACE("Dem", "ClearDTC(0x%06lX): event memory update in progress -> DEM_PENDING",
                  (unsigned long)Dem_SelectedDtc);
        return DEM_PENDING;
    }
    for (i = 0u; i < DEM_NUM_DTCS; i++) {
        if ((Dem_SelectedDtc == DEM_DTC_GROUP_ALL_DTCS) || (Dem_Dtcs[i].dtc == Dem_SelectedDtc)) {
            Dem_Dtcs[i].status = 0x00u;
            found = TRUE;
        }
    }
    Dem_Selected = FALSE;
    if (!found) {
        return DEM_WRONG_DTC;
    }
    UDS_TRACE("Dem", "ClearDTC(0x%06lX) done", (unsigned long)Dem_SelectedDtc);
    return E_OK;
}

Std_ReturnType Dem_GetDTCStatusAvailabilityMask(uint8 ClientId, Dem_UdsStatusByteType *DTCStatusMask)
{
    (void)ClientId;
    *DTCStatusMask = DEM_STATUS_AVAILABILITY;
    return E_OK;
}

Std_ReturnType Dem_SetDTCFilter(uint8 ClientId, uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat,
                                Dem_DTCOriginType DTCOrigin, boolean FilterWithSeverity,
                                Dem_DTCSeverityType DTCSeverityMask, boolean FilterForFaultDetectionCounter)
{
    (void)ClientId;
    (void)DTCFormat;
    (void)FilterWithSeverity;
    (void)DTCSeverityMask;
    (void)FilterForFaultDetectionCounter;
    if (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY) {
        return E_NOT_OK;
    }
    Dem_FilterMask = (uint8)(DTCStatusMask & DEM_STATUS_AVAILABILITY);
    Dem_FilterIndex = 0u;
    Dem_FilterActive = TRUE;
    return E_OK;
}

Std_ReturnType Dem_GetNextFilteredDTC(uint8 ClientId, uint32 *DTC, Dem_UdsStatusByteType *DTCStatus)
{
    (void)ClientId;
    if (!Dem_FilterActive) {
        return E_NOT_OK;
    }
    while (Dem_FilterIndex < DEM_NUM_DTCS) {
        const Dem_DtcEntryType *e = &Dem_Dtcs[Dem_FilterIndex];
        Dem_FilterIndex++;
        if ((e->status & Dem_FilterMask) != 0u) {
            *DTC = e->dtc;
            *DTCStatus = (Dem_UdsStatusByteType)(e->status & DEM_STATUS_AVAILABILITY);
            return E_OK;
        }
    }
    Dem_FilterActive = FALSE;
    return DEM_NO_SUCH_ELEMENT;
}

void Dem_SimSetDtcStatus(uint32 DTC, Dem_UdsStatusByteType status)
{
    uint8 i;
    for (i = 0u; i < DEM_NUM_DTCS; i++) {
        if (Dem_Dtcs[i].dtc == DTC) {
            Dem_Dtcs[i].status = status;
        }
    }
}
