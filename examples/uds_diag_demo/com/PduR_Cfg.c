/*
 * PduR_Cfg.c
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: PduR_PBcfg.c (routing paths) - the place that
 * answers "why does a CanTp N-SDU end up in Dcm?".
 */
#include "PduR.h"
#include "CanTp.h"
#include "Dcm.h"

static const PduR_TpUpperLayerApiType PduR_DcmApi = {
    Dcm_StartOfReception, Dcm_CopyRxData, Dcm_TpRxIndication,
    Dcm_CopyTxData, Dcm_TpTxConfirmation, "Dcm"
};

static const PduR_RxRoutingPathType PduR_RxPaths[PDUR_NUM_RX_PATHS] = {
    { PduRConf_PduRSrcPdu_CanTp_DiagPhysReq, DcmConf_DcmDslProtocolRx_DiagPhys, &PduR_DcmApi, "CanTp(DiagPhys) -> Dcm" },
    { PduRConf_PduRSrcPdu_CanTp_DiagFuncReq, DcmConf_DcmDslProtocolRx_DiagFunc, &PduR_DcmApi, "CanTp(DiagFunc) -> Dcm" }
};

static const PduR_TxRoutingPathType PduR_TxPaths[PDUR_NUM_TX_PATHS] = {
    { PduRConf_PduRSrcPdu_Dcm_DiagResp, CanTpConf_TxNSdu_DiagPhys, CanTp_Transmit,
      PduRConf_PduRDestPdu_CanTp_DiagResp, DcmConf_DcmDslProtocolTx_DiagResp, &PduR_DcmApi, "Dcm -> CanTp(DiagPhys)" }
};

const PduR_PBConfigType PduR_Config = {
    PduR_RxPaths, PDUR_NUM_RX_PATHS,
    PduR_TxPaths, PDUR_NUM_TX_PATHS
};
