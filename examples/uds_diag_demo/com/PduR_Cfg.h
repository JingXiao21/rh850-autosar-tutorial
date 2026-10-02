/*
 * PduR_Cfg.h
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: PduR_Cfg.h / PduR_PBcfg.h generated from
 * PduRRoutingTables / PduRRoutingPath / PduRSrcPdu / PduRDestPdu.
 * Handles below are owned by PduR: CanTp passes them in PduR_CanTp* calls,
 * Dcm passes them in PduR_DcmTransmit.
 */
#ifndef PDUR_CFG_H
#define PDUR_CFG_H

/* Source PDUs for RX routing paths (used by CanTp in StartOfReception/CopyRxData/RxIndication) */
#define PduRConf_PduRSrcPdu_CanTp_DiagPhysReq   0u
#define PduRConf_PduRSrcPdu_CanTp_DiagFuncReq   1u
#define PDUR_NUM_RX_PATHS                       2u

/* Source PDU of the TX routing path (used by Dcm in PduR_DcmTransmit) */
#define PduRConf_PduRSrcPdu_Dcm_DiagResp        0u
/* Destination PDU of the TX routing path (used by CanTp in CopyTxData/TxConfirmation) */
#define PduRConf_PduRDestPdu_CanTp_DiagResp     0u
#define PDUR_NUM_TX_PATHS                       1u

#endif /* PDUR_CFG_H */
