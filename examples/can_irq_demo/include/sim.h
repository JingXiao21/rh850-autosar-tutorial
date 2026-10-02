#ifndef CAN_IRQ_SIM_H
#define CAN_IRQ_SIM_H

#include "can_rx.h"
#include "rh850_can_regs.h"

#define SIM_DEPTH 8u
#define SIM_CAPTURE 32u

/* 已解码的总线帧。模型不实现位时序、CRC 算法、ACK 或物理收发器。 */
typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
    bool extended;
    bool remote;
    bool fd;
    bool protocol_valid;
} BusFrame;

typedef struct {
    BusFrame frame;
    uint16_t label;
    uint16_t timestamp;
} SimSlot;

typedef struct {
    uint32_t grmcfg, gaflectr, gaflcfg0, afl_id, afl_mask, afl_p0, afl_p1, rfcc;
    uint32_t flags;
    SimSlot slots[SIM_DEPTH];
    unsigned int head, count;
    uint16_t time;
    bool operating;
    bool trace;
    bool inject_before_clear;
    BusFrame injected_frame;
    uint32_t stored, filtered, lost, invalid, popped, syncs, last_status_write;
} SimHw;

typedef void (*SimVector)(void *context);
typedef struct {
    SimVector entry;
    void *context;
} SimVectorEntry;

typedef struct {
    SimHw *hw;
    CanRxDriver *driver;
    /* PC 函数指针的大小由主机决定！这不是可烧录的 4 字节 RH850 INTBP 表。 */
    SimVectorEntry vectors[EI_CHANNELS];
    uint16_t eic190, pmr;
    uint32_t eiic, pc, eipc;
    bool psw_id, in_isr;
    uint32_t entries, default_entries;
} SimCpu;

typedef struct {
    SimHw hw;
    CanRxDriver driver;
    SimCpu cpu;
    CanRxFrame received[SIM_CAPTURE];
    unsigned int received_count;
} Demo;

void Sim_Log(const SimHw *hw, const char *format, ...);
void SimHw_Init(SimHw *hw, bool trace);
bool SimHw_Receive(SimHw *hw, const BusFrame *frame);
bool SimHw_Irq(const SimHw *hw);
uint32_t SimHw_Read32(void *context, uint32_t address);
void SimHw_Write32(void *context, uint32_t address, uint32_t value);
void SimHw_Syncp(void *context);
void SimCpu_Init(SimCpu *cpu, SimHw *hw, CanRxDriver *driver);
bool SimCpu_RunOne(SimCpu *cpu);
void SimCpu_BindCan(SimCpu *cpu, unsigned int channel);
uint16_t SimCpu_ReadEic190(const SimCpu *cpu);
void Demo_Init(Demo *demo, bool trace);
BusFrame Demo_Frame(uint8_t first_byte);

#endif
