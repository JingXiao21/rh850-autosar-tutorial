#include "sim.h"
#include <assert.h>
#include <string.h>

static void Os_CanRx_Category2_Wrapper_Model(void *context)
{
    SimCpu *cpu = context;
    Sim_Log(cpu->hw, "[OS wrapper model] enter Category 2 -> call ordinary C ISR body");
    /* 真机 wrapper 负责软件上下文、栈、OS 状态及退出调度。本模型仅显示边界。 */
    CanRx_Fifo0IsrBody(cpu->driver);
    Sim_Log(cpu->hw, "[OS wrapper model] leave Category 2 -> restore context / EIRET boundary");
}

void SimCpu_BindCan(SimCpu *cpu, unsigned int channel)
{
    assert(channel < EI_CHANNELS);
    memset(cpu->vectors, 0, sizeof(cpu->vectors));
    cpu->vectors[channel].entry = Os_CanRx_Category2_Wrapper_Model;
    cpu->vectors[channel].context = cpu;
}

void SimCpu_Init(SimCpu *cpu, SimHw *hw, CanRxDriver *driver)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->hw = hw;
    cpu->driver = driver;
    cpu->eic190 = EIC_EICT | EIC_EITB | 5u; /* 仅示例 EIP=5；不是 OS 优先级映射。 */
    cpu->pc = UINT32_C(0x1000);            /* 模型中被打断的代码地址。 */
    SimCpu_BindCan(cpu, CAN_RX_EI);
}

uint16_t SimCpu_ReadEic190(const SimCpu *cpu)
{
    /* 电平型 EIRF 跟随外设请求，即使 EIMK=1 也能看到请求。 */
    return cpu->eic190 | (SimHw_Irq(cpu->hw) ? EIC_EIRF : 0u);
}

bool SimCpu_RunOne(SimCpu *cpu)
{
    unsigned int priority = cpu->eic190 & 15u;
    unsigned int channel;
    bool old_id = cpu->psw_id;
    SimVectorEntry vector;
    if (!SimHw_Irq(cpu->hw)) {
        return false;
    }
    if (cpu->psw_id || cpu->in_isr || (cpu->eic190 & EIC_EIMK) != 0u ||
        (cpu->pmr & (UINT16_C(1) << priority)) != 0u) {
        Sim_Log(cpu->hw, "[CPU] request pending but masked; frame stays in hardware FIFO");
        return false;
    }

    /* 只服务一个固定硬件源。不模拟完整优先级仲裁、EIBD、ISPR 和嵌套。 */
    cpu->eiic = UINT32_C(0x1000) + CAN_RX_EI;
    cpu->eipc = cpu->pc;
    cpu->psw_id = true;
    cpu->in_isr = true;
    cpu->entries++;
    Sim_Log(cpu->hw, "[CPU] accept EI190; EIIC=0x10BE; save interrupted PC -> EIPC");
    if ((cpu->eic190 & EIC_EITB) != 0u) {
        channel = CAN_RX_EI; /* 模拟硬件用正在受理的通道号查 INTBP 表。 */
        Sim_Log(cpu->hw, "[VECTOR] EITB=1: read handler ADDRESS at INTBP + 0x2F8");
    } else {
        channel = (unsigned int)(cpu->eiic - UINT32_C(0x1000));
        Sim_Log(cpu->hw, "[VECTOR] EITB=0, RINT=0: base+0x%X (EIP=%u); dispatch EIIC-0x1000",
                0x100u + priority * 0x10u, priority);
    }
    /* 两种方式在主机上共用软件表；真机表引用的查表由 CPU 完成。
     * 直接分支演示则代表公共入口读 EIIC 后用软件查表。 */
    assert(channel < EI_CHANNELS);
    vector = cpu->vectors[channel];
    if (vector.entry != NULL) {
        vector.entry(vector.context);
    } else {
        cpu->default_entries++;
        Sim_Log(cpu->hw, "[DEFAULT] no EI190 handler! Binding EI90 does NOT reroute CAN hardware");
    }
    cpu->pc = cpu->eipc;
    cpu->psw_id = old_id;
    cpu->in_isr = false;
    Sim_Log(cpu->hw, "[CPU return model] resume interrupted PC; request level=%u",
            (unsigned int)SimHw_Irq(cpu->hw));
    return true;
}
