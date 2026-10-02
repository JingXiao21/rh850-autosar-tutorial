/*
 * CanIf_Cfg.h
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: CanIf_Cfg.h generated from the CanIf ECUC
 * (CanIfInitCfg/CanIfRxPduCfg/CanIfTxPduCfg/CanIfHrhCfg/CanIfHthCfg).
 * Symbolic name style "CanIfConf_<container>_<shortName>" mirrors what
 * real generators emit; numbers are L-PDU handles owned by CanIf.
 */
#ifndef CANIF_CFG_H
#define CANIF_CFG_H

/* Rx L-PDUs (CanIf_RxIndication matches HRH + CAN ID against this list) */
#define CanIfConf_CanIfRxPduCfg_DiagPhysReq_7E0   0u
#define CanIfConf_CanIfRxPduCfg_DiagFuncReq_7DF   1u
#define CANIF_NUM_RX_PDUS                         2u

/* Tx L-PDUs: both use CAN ID 0x7E8, one for CanTp data frames (SF/FF/CF),
 * one for flow control frames sent while the ECU *receives* a long request.
 * Separate handles let CanTp distinguish the two TX confirmations. */
#define CanIfConf_CanIfTxPduCfg_DiagResp_7E8      0u
#define CanIfConf_CanIfTxPduCfg_DiagRespFc_7E8    1u
#define CANIF_NUM_TX_PDUS                         2u

#define CANIF_TX_BUFFER_DEPTH                     4u   /* CanIfBufferSize */

#endif /* CANIF_CFG_H */
