/*
 * CanTp_Cfg.c
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: CanTp_PBcfg.c.
 * Timing values are typical OEM values for diagnostics (ISO 15765-2 allows
 * N_As/N_Ar <= 1000 ms, N_Bs/N_Cr <= 1000 ms); confirm against the OEM spec.
 */
#include "CanTp.h"
#include "CanIf.h"
#include "PduR_Cfg.h"

static const CanTp_RxNSduConfigType CanTp_RxNSdus[CANTP_NUM_RX_NSDU] = {
    {   /* physical requests 0x7E0, may be multi-frame */
        CanTpConf_RxNPdu_DiagPhysReq_7E0, CanTpConf_TxFcNPdu_DiagResp_7E8, CanIfConf_CanIfTxPduCfg_DiagRespFc_7E8,
        PduRConf_PduRSrcPdu_CanTp_DiagPhysReq, CANTP_PHYSICAL,
        2u,  /* BS: tester must wait for a new FC after 2 CFs (shows block handling) */
        5u,  /* STmin = 5 ms requested from the tester */
        3u, 70u, 70u, 150u, "RxNSdu_DiagPhys"
    },
    {   /* functional requests 0x7DF, single frame only */
        CanTpConf_RxNPdu_DiagFuncReq_7DF, CanTpConf_TxFcNPdu_DiagResp_7E8, CanIfConf_CanIfTxPduCfg_DiagRespFc_7E8,
        PduRConf_PduRSrcPdu_CanTp_DiagFuncReq, CANTP_FUNCTIONAL,
        0u, 0u, 0u, 70u, 70u, 150u, "RxNSdu_DiagFunc"
    }
};

static const CanTp_TxNSduConfigType CanTp_TxNSdus[CANTP_NUM_TX_NSDU] = {
    {
        CanTpConf_TxNPdu_DiagResp_7E8, CanIfConf_CanIfTxPduCfg_DiagResp_7E8, CanTpConf_RxNPdu_DiagPhysReq_7E0,
        PduRConf_PduRDestPdu_CanTp_DiagResp, CANTP_PHYSICAL,
        70u, 150u, 70u, "TxNSdu_DiagPhys"
    }
};

const CanTp_ConfigType CanTp_Config = {
    CanTp_RxNSdus, CANTP_NUM_RX_NSDU,
    CanTp_TxNSdus, CANTP_NUM_TX_NSDU
};
