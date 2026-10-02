/*
 * Can_Cfg.h
 *
 * [Educational Implementation] hand-written "as if generated".
 * Real AUTOSAR counterpart: Can_Cfg.h / Can_PBcfg.h produced by the MCAL
 * vendor's configurator (e.g. Renesas MCAL generator from the Can ECUC
 * container CanConfigSet/CanController/CanHardwareObject).
 *
 * HOH numbering follows ECUC_Can_00326 (CAN SWS R22-11 p.125): one
 * contiguous ID space, HRHs first, then HTHs.
 */
#ifndef CAN_CFG_H
#define CAN_CFG_H

#define CAN_DEV_ERROR_DETECT           STD_ON

/* CanController (ECUC_Can_00354) */
#define CanConf_CanController_CAN0      0u   /* conceptually RS-CANFD channel 0 */
#define CAN_NUM_CONTROLLERS             1u

/* CanHardwareObject (ECUC_Can_00324) */
#define CanConf_HRH_DiagPhysReq_7E0     0u   /* RECEIVE, FULL, filter 0x7E0     */
#define CanConf_HRH_DiagFuncReq_7DF     1u   /* RECEIVE, FULL, filter 0x7DF     */
#define CanConf_HTH_DiagResp            2u   /* TRANSMIT -> RS-CANFD TX buffer 0 */
#define CAN_NUM_HOH                     3u

#endif /* CAN_CFG_H */
