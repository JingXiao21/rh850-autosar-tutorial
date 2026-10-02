#include "sim.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void Sim_Log(const SimHw *hw, const char *format, ...)
{
    if (hw->trace) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        putchar('\n');
    }
}

void SimHw_Init(SimHw *hw, bool trace)
{
    memset(hw, 0, sizeof(*hw));
    hw->trace = trace;
}

static uint32_t encoded_id(const BusFrame *frame)
{
    return frame->id | (frame->extended ? RFID_IDE : 0u) |
           (frame->remote ? RFID_RTR : 0u);
}

bool SimHw_Receive(SimHw *hw, const BusFrame *frame)
{
    SimSlot *slot;
    Sim_Log(hw, "[BUS] decoded frame: id=0x%03lX dlc=%u FDF=%u",
            (unsigned long)frame->id, (unsigned int)frame->dlc, (unsigned int)frame->fd);
    /* 本函数代替 CAN 协议引擎，不是接收驱动调用的入队 API。
     * 大于 8 字节的 FD 不在此模型范围内，不模拟 CMPOC/CMPOF。 */
    if (!frame->protocol_valid || frame->dlc > 15u ||
        frame->id > (frame->extended ? UINT32_C(0x1FFFFFFF) : UINT32_C(0x7FF)) ||
        (frame->fd && (frame->remote || frame->dlc > 8u))) {
        hw->invalid++;
        Sim_Log(hw, "[HW] invalid / outside this model's input domain; not stored");
        return false;
    }
    if (!hw->operating || hw->grmcfg != 1u ||
        hw->gaflcfg0 != UINT32_C(0x01000000) ||
        (((encoded_id(frame) ^ hw->afl_id) & hw->afl_mask) != 0u) ||
        (hw->afl_p1 & 1u) == 0u || (hw->rfcc & RFCC_RFE) == 0u) {
        hw->filtered++;
        Sim_Log(hw, "[AFL] no enabled route / rule match; no FIFO write");
        return false;
    }
    if (hw->count == SIM_DEPTH) {
        hw->flags |= RFSTS_RFMLT;
        hw->lost++;
        Sim_Log(hw, "[FIFO] full: discard NEW frame, latch RFMLT=1");
        return false;
    }
    slot = &hw->slots[(hw->head + hw->count) % SIM_DEPTH];
    slot->frame = *frame;                   /* 硬件向消息 RAM 写入，而非 CPU。 */
    slot->label = (uint16_t)((hw->afl_p0 >> 16) & UINT32_C(0xFFF));
    slot->timestamp = ++hw->time;           /* 仅模型序号，不是实际计时单位。 */
    hw->count++;
    hw->stored++;
    assert((hw->rfcc & RFCC_RFIM) != 0u);  /* 本模型只实现每帧触发模式。 */
    hw->flags |= RFSTS_RFIF;                /* RFIF 是锁存位，不是帧计数器。 */
    Sim_Log(hw, "[AFL -> FIFO0] hardware stores frame+label=%u; unread=%u, RFIF=1",
            (unsigned int)slot->label, hw->count);
    Sim_Log(hw, "[IRQ] RFIF & RFIE -> INTRCANGRECC -> EI190, level=%u",
            (unsigned int)SimHw_Irq(hw));
    return true;
}

bool SimHw_Irq(const SimHw *hw)
{
    return ((hw->flags & RFSTS_RFIF) != 0u) && ((hw->rfcc & RFCC_RFIE) != 0u);
}

uint32_t SimHw_Read32(void *context, uint32_t address)
{
    SimHw *hw = context;
    const SimSlot *slot;
    uint32_t value;
    unsigned int i, offset;
    switch (address) {
    case REG_GRMCFG: return hw->grmcfg;
    case REG_GAFLECTR: return hw->gaflectr;
    case REG_GAFLCFG0: return hw->gaflcfg0;
    case REG_GAFLID0: return hw->afl_id;
    case REG_GAFLM0: return hw->afl_mask;
    case REG_GAFLP0_0: return hw->afl_p0;
    case REG_GAFLP1_0: return hw->afl_p1;
    case REG_RFCC0: return hw->rfcc;
    case REG_RFSTS0:
        return hw->flags | ((uint32_t)hw->count << 8) |
               (hw->count == 0u ? RFSTS_RFEMP : 0u) |
               (hw->count == SIM_DEPTH ? RFSTS_RFFLL : 0u);
    default: break;
    }
    /* 访问窗口不会弹出；连续读 RFID0 两次，看到的是同一队首。 */
    assert(hw->count != 0u);
    slot = &hw->slots[hw->head];
    switch (address) {
    case REG_RFID0: value = encoded_id(&slot->frame); break;
    case REG_RFPTR0:
        value = ((uint32_t)slot->frame.dlc << 28) |
                ((uint32_t)slot->label << 16) | slot->timestamp;
        break;
    case REG_RFFDSTS0: value = slot->frame.fd ? RFFDSTS_FDF : 0u; break;
    case REG_RFDF0_0:
    case REG_RFDF1_0:
        value = 0u;
        offset = (address == REG_RFDF0_0) ? 0u : 4u;
        for (i = 0u; i < 4u; ++i) {
            if (offset + i < slot->frame.dlc) {
                value |= (uint32_t)slot->frame.data[offset + i] << (8u * i);
            }
        }
        break;
    default: assert(!"unimplemented MMIO read"); return 0u;
    }
    Sim_Log(hw, "[MMIO read32] 0x%08lX -> 0x%08lX (FIFO head, no pop)",
            (unsigned long)address, (unsigned long)value);
    return value;
}

void SimHw_Write32(void *context, uint32_t address, uint32_t value)
{
    SimHw *hw = context;
    if (address >= REG_GAFLID0 && address <= REG_GAFLP1_0) {
        assert(!hw->operating && (hw->gaflectr & AFL_WRITE_ENABLE) != 0u);
        assert((hw->gaflectr & UINT32_C(0x1F)) == 0u);
    }
    switch (address) {
    case REG_GRMCFG: assert(!hw->operating && value == 1u); hw->grmcfg = value; break;
    case REG_GAFLECTR: assert(!hw->operating); hw->gaflectr = value; break;
    case REG_GAFLCFG0: assert(!hw->operating); hw->gaflcfg0 = value; break;
    case REG_GAFLID0: hw->afl_id = value; break;
    case REG_GAFLM0: hw->afl_mask = value; break;
    case REG_GAFLP0_0: hw->afl_p0 = value; break;
    case REG_GAFLP1_0: hw->afl_p1 = value; break;
    case REG_RFCC0:
        /* 初始化配置与 RFE 开启分两次写；不模拟运行时重新配置 FIFO。 */
        if (!hw->operating) {
            assert((value & RFCC_RFE) == 0u);
        } else {
            assert((value ^ hw->rfcc) == RFCC_RFE && (value & RFCC_RFE) != 0u);
        }
        hw->rfcc = value;
        break;
    case REG_RFPCTR0:
        assert(value == UINT32_C(0xFF) && hw->count != 0u &&
               (hw->rfcc & RFCC_RFE) != 0u);
        hw->head = (hw->head + 1u) % SIM_DEPTH;
        hw->count--;
        hw->popped++;
        /* 不清 RFIF！弹出数据和撤销中断请求是两个独立动作。 */
        Sim_Log(hw, "[MMIO write32] RFPCTR0=0xFF -> pop; unread=%u, RFIF=%u",
                hw->count, (unsigned int)((hw->flags & RFSTS_RFIF) != 0u));
        break;
    case REG_RFSTS0:
        assert((value & ~(RFSTS_RFIF | RFSTS_RFMLT)) == 0u);
        /* 可重复实验：恰好在 CPU 判断空之后、清 RFIF 之前收到新帧。 */
        if (hw->inject_before_clear) {
            bool stored;
            hw->inject_before_clear = false;
            Sim_Log(hw, "[RACE] another frame arrives immediately BEFORE clearing RFIF");
            stored = SimHw_Receive(hw, &hw->injected_frame);
            assert(stored);
            (void)stored;
        }
        hw->flags &= value;   /* 只有 RFIF/RFMLT 存在 flags 中；模拟 W0C。 */
        hw->last_status_write = value;
        Sim_Log(hw, "[MMIO write32] RFSTS0=0x%lX -> RFIF=%u, RFMLT=%u",
                (unsigned long)value, (unsigned int)((hw->flags & RFSTS_RFIF) != 0u),
                (unsigned int)((hw->flags & RFSTS_RFMLT) != 0u));
        break;
    default: assert(!"unimplemented MMIO write"); break;
    }
}

void SimHw_Syncp(void *context)
{
    SimHw *hw = context;
    hw->syncs++;
    Sim_Log(hw, "[SYNC model] dummy read + SYNCP point (PC does not execute RH850 SYNCP)");
}
