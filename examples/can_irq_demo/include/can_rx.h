#ifndef CAN_RX_H
#define CAN_RX_H

#include <stdbool.h>
#include <stdint.h>

/* 核心驱动只通过此接口访问寄存器：PC 接模型，目标机接 volatile MMIO。 */
typedef struct {
    void *context;
    uint32_t (*read32)(void *context, uint32_t address);
    void (*write32)(void *context, uint32_t address, uint32_t value);
    void (*syncp)(void *context);
} CanIo;

typedef struct {
    uint32_t id;
    uint16_t hrh;              /* 本例约定 AFL label 就是 HRH；不是硬件强制。 */
    uint16_t timestamp;
    uint8_t controller;
    uint8_t length;
    uint8_t data[8];
} CanRxFrame;

/* 回调必须同步使用或复制数据，禁止保留指向 ISR 栈内帧的指针。 */
typedef void (*CanRxIndication)(void *context, const CanRxFrame *frame);

typedef struct {
    CanIo io;
    CanRxIndication indication;
    void *upper_context;
    uint32_t isr_calls;
    uint32_t delivered;
    uint32_t rejected_format;
    bool loss_seen;            /* RFMLT 是锁存标志，不能拿它计算精确丢帧数。 */
} CanRxDriver;

void CanRx_Init(CanRxDriver *driver, CanIo io,
                CanRxIndication indication, void *upper_context);
/* 普通 C 函数：只能由正确的 ISR wrapper 调用，不是 CPU 裸中断入口。 */
void CanRx_Fifo0IsrBody(CanRxDriver *driver);

#endif
