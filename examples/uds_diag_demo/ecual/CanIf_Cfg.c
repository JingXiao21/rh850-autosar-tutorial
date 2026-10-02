/*
 * CanIf_Cfg.c
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: CanIf_PBcfg.c / CanIf_Lcfg.c.
 *
 * This table is THE answer to "how does CanIf know that 0x7E0 belongs to
 * CanTp?": nothing in CanIf code knows about diagnostics; the routing
 * (HRH, CAN ID) -> (CanTp_RxIndication, CanTp N-PDU id) is pure configuration.
 */
#include "CanIf.h"
#include "Can.h"
#include "CanTp.h"

static const CanIf_RxPduConfigType CanIf_RxPdus[CANIF_NUM_RX_PDUS] = {
    { 0x7E0u, CanConf_HRH_DiagPhysReq_7E0, 1u, CanTpConf_RxNPdu_DiagPhysReq_7E0, CanTp_RxIndication, "DiagPhysReq_7E0" },
    { 0x7DFu, CanConf_HRH_DiagFuncReq_7DF, 1u, CanTpConf_RxNPdu_DiagFuncReq_7DF, CanTp_RxIndication, "DiagFuncReq_7DF" }
};

static const CanIf_TxPduConfigType CanIf_TxPdus[CANIF_NUM_TX_PDUS] = {
    { 0x7E8u, CanConf_HTH_DiagResp, CanTpConf_TxNPdu_DiagResp_7E8,   CanTp_TxConfirmation, "DiagResp_7E8" },
    { 0x7E8u, CanConf_HTH_DiagResp, CanTpConf_TxFcNPdu_DiagResp_7E8, CanTp_TxConfirmation, "DiagRespFC_7E8" }
};

const CanIf_ConfigType CanIf_Config = {
    CanIf_RxPdus, CANIF_NUM_RX_PDUS,
    CanIf_TxPdus, CANIF_NUM_TX_PDUS
};
