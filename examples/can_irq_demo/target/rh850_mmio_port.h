#ifndef CAN_IRQ_RH850_MMIO_PORT_H
#define CAN_IRQ_RH850_MMIO_PORT_H

#include "can_rx.h"

/* 只创建访问层；不初始化 CAN，也不配置中断。调用约束见 README 第 12 节。 */
CanIo CanRx_CreateTargetIo(void);

#endif
