/*
 * PduR.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: PDU Router (AUTOSAR_CP_SWS_PDURouter). Implemented: PduR_Init (SWS 8.3.1.1),
 * PduR_ComTransmit (upper layer -> lower layer, "Transmit" path of the IF routing), PduR_CanIfRxIndication
 * (lower -> upper, "RxIndication"), PduR_CanIfTxConfirmation ("TxConfirmation"). Names are the real R25-11
 * names. Routing is strictly 1:1 interface routing: the PduR handle is the index in the generated path table
 * (PduR_Config.txPaths / rxPaths). No gateway, no TP, no buffering, no routing path groups, no multicast.
 * The router is stateless apart from the ONLINE flag and therefore needs no exclusive area (SchM.h still
 * defines SchM_Enter_PduR_PDUR_EXCLUSIVE_AREA_0 for stateful extensions such as a gateway FIFO).
 * Spec: AUTOSAR_CP_SWS_PDURouter 8.3.1.1 PduR_Init; the Com - PduR - CanIf call graph of the IF routing chapter
 */
#include "PduR.h"
#include "Com.h"
#include "CanIf.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_Cfg.h"
#include "Mini_ModuleIds.h"

#define PDUR_API_INIT      0xF0u
#define PDUR_API_TRANSMIT  0x49u
#define PDUR_API_RX_IND    0x42u
#define PDUR_API_TX_CONF   0x40u

static const PduR_PBConfigType *pdur_cfg;
static PduR_StateType           pdur_state = PDUR_UNINIT;

void PduR_Init(const PduR_PBConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_INIT, MINI_E_PARAM_POINTER);
        return;
    }
    pdur_cfg = ConfigPtr;
    pdur_state = PDUR_ONLINE;                       /* routing is enabled immediately (no routing path groups) */
}

PduR_StateType PduR_GetState(void)
{
    return pdur_state;
}

void PduR_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo != NULL_PTR) {
        versioninfo->vendorID = MINI_VENDOR_ID;
        versioninfo->moduleID = MINI_MODULE_PDUR;
        versioninfo->sw_major_version = 1u;
        versioninfo->sw_minor_version = 0u;
        versioninfo->sw_patch_version = 0u;
    }
}

Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *PduInfoPtr)
{
    if (pdur_state != PDUR_ONLINE) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_TRANSMIT, MINI_E_UNINIT);
        return E_NOT_OK;
    }
    if (PduInfoPtr == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_TRANSMIT, MINI_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (id >= pdur_cfg->numTxPaths) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_TRANSMIT, MINI_E_PARAM_INVALID);
        return E_NOT_OK;
    }
    TRACE(TRACE_CAT_PDUR, "TX com=%u canif=%u", (unsigned)pdur_cfg->txPaths[id].comTxPduId,
          (unsigned)pdur_cfg->txPaths[id].canIfTxPduId);
    return CanIf_Transmit(pdur_cfg->txPaths[id].canIfTxPduId, PduInfoPtr);
}

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    if (pdur_state != PDUR_ONLINE) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_RX_IND, MINI_E_UNINIT);
        return;
    }
    if (PduInfoPtr == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_RX_IND, MINI_E_PARAM_POINTER);
        return;
    }
    if (RxPduId >= pdur_cfg->numRxPaths) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_RX_IND, MINI_E_PARAM_INVALID);
        return;
    }
    TRACE(TRACE_CAT_PDUR, "RX canif=%u com=%u", (unsigned)RxPduId, (unsigned)pdur_cfg->rxPaths[RxPduId].comRxPduId);
    Com_RxIndication(pdur_cfg->rxPaths[RxPduId].comRxPduId, PduInfoPtr);
}

void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    if (pdur_state != PDUR_ONLINE) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_TX_CONF, MINI_E_UNINIT);
        return;
    }
    if (TxPduId >= pdur_cfg->numTxPaths) {
        (void)Det_ReportError(MINI_MODULE_PDUR, 0u, PDUR_API_TX_CONF, MINI_E_PARAM_INVALID);
        return;
    }
    Com_TxConfirmation(pdur_cfg->txPaths[TxPduId].comTxPduId, result);
}
