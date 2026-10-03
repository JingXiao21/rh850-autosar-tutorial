/* [Educational Implementation] Port_Cfg.h (REST node, hand written; the real counterpart is the generated Port configuration): pin ids = port*16+pin as in the generated LightEcu configuration. */
#ifndef PORT_CFG_H
#define PORT_CFG_H
#define PORT_NUM_PINS                      4u
#define PortConf_PortPin_Pin_Usart1Tx      9u     /* PA9  AF7 */
#define PortConf_PortPin_Pin_Usart1Rx      10u    /* PA10 AF7 */
#define PortConf_PortPin_Pin_CanTx         49u    /* PD1  AF9 */
#define PortConf_PortPin_Pin_CanRx         48u    /* PD0  AF9 */
#endif
