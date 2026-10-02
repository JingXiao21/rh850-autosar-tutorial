/*
 * CanTp_Cfg.h
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: CanTp_Cfg.h generated from CanTpConfig /
 * CanTpChannel / CanTpRxNSdu / CanTpTxNSdu.
 *
 * Two handle spaces exist and are easy to confuse:
 *   N-PDU ids  : CanIf <-> CanTp   (one CAN frame)
 *   N-SDU ids  : CanTp <-> PduR    (one complete UDS message, up to 4095 bytes)
 */
#ifndef CANTP_CFG_H
#define CANTP_CFG_H

#define CANTP_DEV_ERROR_DETECT            STD_ON
#define CANTP_MAIN_FUNCTION_PERIOD_MS     1u     /* CanTpMainFunctionPeriod = 0.001 s */
#define CANTP_PADDING_BYTE                0xCCu  /* CanTpPaddingByte, CanTpPaddingActivation = ON */

/* N-PDUs received from CanIf (CanTp_RxIndication RxPduId) */
#define CanTpConf_RxNPdu_DiagPhysReq_7E0  0u   /* SF/FF/CF of requests + FC for our responses */
#define CanTpConf_RxNPdu_DiagFuncReq_7DF  1u   /* SF only (functional) */

/* N-PDUs transmitted via CanIf (CanTp_TxConfirmation TxPduId) */
#define CanTpConf_TxNPdu_DiagResp_7E8     0u   /* SF/FF/CF of responses */
#define CanTpConf_TxFcNPdu_DiagResp_7E8   1u   /* FC frames sent while receiving */

/* N-SDUs */
#define CanTpConf_RxNSdu_DiagPhys         0u
#define CanTpConf_RxNSdu_DiagFunc         1u
#define CANTP_NUM_RX_NSDU                 2u
#define CanTpConf_TxNSdu_DiagPhys         0u
#define CANTP_NUM_TX_NSDU                 1u

#endif /* CANTP_CFG_H */
