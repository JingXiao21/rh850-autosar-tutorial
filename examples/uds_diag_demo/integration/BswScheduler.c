/*
 * BswScheduler.c
 *
 * [Educational Implementation] See BswScheduler.h.
 *
 * One call = 1 ms:
 *   1. "hardware": VirtualCanBus moves requested TX buffers onto the wire
 *   2. "interrupt controller": if the RX FIFO has frames and the interrupt is
 *      enabled, run the Can RX ISR (EI190 on RH850/P1M-E)
 *   3. 1 ms task : Can_MainFunction_Mode/Write/Read, CanTp_MainFunction
 *   4. 5 ms task : NvM_MainFunction
 *   5. 10 ms task: Dcm_MainFunction, Rte_Task_10ms (SW-C runnables)
 *   6. simulated reset requested by Dcm (0x11) -> EcuM_SimPerformReset
 */
#include "BswScheduler.h"
#include "SimClock.h"
#include "VirtualCanBus.h"
#include "Can.h"
#include "CanTp.h"
#include "NvM.h"
#include "Dcm.h"
#include "Rte.h"
#include "EcuM.h"

void BswScheduler_Tick1ms(void)
{
    uint32 now;

    SimClock_Advance(1u);
    now = SimClock_NowMs();

    VirtualCanBus_Tick();
    if (VirtualCanBus_HwRxPending() && Can_IsRxInterruptEnabled()) {
        Can_Isr_GlobalRxFifo();
    }

    /* Task_1ms */
    Can_MainFunction_Mode();
    Can_MainFunction_Write();
    Can_MainFunction_Read();
    CanTp_MainFunction();

    /* Task_5ms */
    if ((now % 5u) == 0u) {
        NvM_MainFunction();
    }

    /* Task_10ms */
    if ((now % 10u) == 0u) {
        Dcm_MainFunction();
        Rte_Task_10ms();
    }

    if (EcuM_SimIsResetRequested()) {
        EcuM_SimPerformReset();
    }
}
