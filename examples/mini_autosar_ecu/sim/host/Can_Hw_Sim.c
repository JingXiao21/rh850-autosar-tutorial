/*
 * Can_Hw_Sim.c   (HOST BUILD ONLY: file name contains _Sim)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the controller specific part of a CAN MCAL driver (here: a behavioural model instead of FDCAN registers).
 * Host backend behind mcal/can/Can_Hw.h; the hardware independent Can.c above it is IDENTICAL to the target build. Models the
 * parts of the STM32 FDCAN that are visible to the driver:
 *   - STOPPED/STARTED (init mode): frames are only received / sent while started (CanHw_Start/Stop),
 *   - 3 TX buffers: CanHw_TxRequest hands the frame to the virtual bus (SimCan_OnTx, "sent" at the current virtual time) and the
 *     buffer reports "done" to the next Can_MainFunction_Write, exactly like the polled TXBTO flag on the target,
 *   - RX acceptance filter: classic id/mask filters from the receive HOH of Can_Config (frames that match no filter are ignored by
 *     the "hardware" - the same as RXGFC.ANFS = reject),
 *   - RX FIFO0 of depth 3 (RXF0S): a frame arriving while it is full is lost (RF0L, counted),
 *   - interrupt: a new frame sets the "RF0N" flag; if RX interrupts are enabled the Cat2 ISR for IRQ 39 (FDCAN1_IT0) is raised via
 *     SimTime_RaiseIsr; re-enabling the interrupt with the flag still set raises it again (level behaviour).
 * Bus side: SimCan.c (TX log / RX script files) calls CanHwSim_PushRx(). Owner: agent B.
 */
#include "Can_Hw.h"
#include "SimCan.h"
#include "SimTime.h"

typedef struct {
    boolean used;
    boolean done;
} CanHwSim_TxBuf;

static const Can_ConfigType *s_cfg;
static boolean               s_started;
static boolean               s_irqEnabled;
static boolean               s_irqFlag;                 /* IR.RF0N */
static CanHwSim_TxBuf        s_tx[CAN_HW_TX_BUFFERS];
static CanHw_FrameType       s_fifo[CAN_HW_RX_FIFO_SIZE];
static uint8                 s_fifoHead;                /* get index */
static uint8                 s_fifoCount;               /* fill level */
static uint32                s_lost;

Std_ReturnType CanHw_Init(const Can_ConfigType *cfg)
{
    uint8 i;
    s_cfg        = cfg;
    s_started    = FALSE;
    s_irqEnabled = cfg->rxInterrupt;
    s_irqFlag    = FALSE;
    s_fifoHead   = 0u;
    s_fifoCount  = 0u;
    s_lost       = 0u;
    for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {
        s_tx[i].used = FALSE;
        s_tx[i].done = FALSE;
    }
    return E_OK;
}

Std_ReturnType CanHw_Start(void)
{
    s_started = TRUE;
    return E_OK;
}

Std_ReturnType CanHw_Stop(void)
{
    uint8 i;
    s_started = FALSE;
    for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {             /* TXBCR: pending requests are cancelled */
        s_tx[i].used = FALSE;
        s_tx[i].done = FALSE;
    }
    return E_OK;
}

Std_ReturnType CanHw_TxRequest(Can_IdType id, uint8 dlc, const uint8 *data, uint8 *bufIdx)
{
    uint8 i;
    for (i = 0u; i < CAN_HW_TX_BUFFERS; i++) {
        if (!s_tx[i].used) {
            s_tx[i].used = TRUE;
            s_tx[i].done = TRUE;                           /* the virtual bus is infinitely fast: sent at once, reported by polling */
            SimCan_OnTx(SimTime_GetUs(), id, dlc, data);
            *bufIdx = i;
            return E_OK;
        }
    }
    return E_NOT_OK;                                       /* all buffers occupied */
}

boolean CanHw_TxIsDone(uint8 bufIdx)
{
    if ((bufIdx < CAN_HW_TX_BUFFERS) && s_tx[bufIdx].used && s_tx[bufIdx].done) {
        s_tx[bufIdx].used = FALSE;
        s_tx[bufIdx].done = FALSE;
        return TRUE;
    }
    return FALSE;
}

boolean CanHwSim_PushRx(uint32 id, uint8 dlc, const uint8 *data)
{
    boolean match = FALSE;
    uint8   i;
    uint8   put;
    if (!s_started || (s_cfg == NULL_PTR)) {
        return FALSE;                                      /* controller in init mode: not on the bus, frame lost for it */
    }
    if (dlc > 8u) {
        dlc = 8u;
    }
    for (i = 0u; i < s_cfg->numHoh; i++) {                 /* acceptance filtering (classic id + mask) */
        if ((s_cfg->hoh[i].type == CAN_HOH_RECEIVE) && (((id ^ s_cfg->hoh[i].canId) & s_cfg->hoh[i].filterMask) == 0u)) {
            match = TRUE;
            break;
        }
    }
    if (!match) {
        return TRUE;                                       /* rejected by the filter: nothing stored, nothing lost */
    }
    if (s_fifoCount >= CAN_HW_RX_FIFO_SIZE) {
        s_lost++;                                          /* RF0L */
        return FALSE;
    }
    put = (uint8)((s_fifoHead + s_fifoCount) % CAN_HW_RX_FIFO_SIZE);
    s_fifo[put].id  = id & CAN_ID_VALUE_MASK_STD;
    s_fifo[put].dlc = dlc;
    for (i = 0u; i < 8u; i++) {
        s_fifo[put].data[i] = (i < dlc) ? data[i] : 0u;
    }
    s_fifoCount++;
    s_irqFlag = TRUE;
    if (s_irqEnabled) {
        SimTime_RaiseIsr(SIMCAN_IRQ_FDCAN1_IT0);
    }
    return TRUE;
}

boolean CanHw_RxFetch(CanHw_FrameType *frame)
{
    if (s_fifoCount == 0u) {
        return FALSE;
    }
    *frame      = s_fifo[s_fifoHead];
    s_fifoHead  = (uint8)((s_fifoHead + 1u) % CAN_HW_RX_FIFO_SIZE);
    s_fifoCount--;
    return TRUE;
}

void CanHw_RxIrqAck(void)
{
    s_irqFlag = FALSE;
}

void CanHw_RxIrqEnable(boolean enable)
{
    s_irqEnabled = enable;
    if (enable && s_irqFlag) {                             /* flag stayed set while masked: the interrupt fires now */
        SimTime_RaiseIsr(SIMCAN_IRQ_FDCAN1_IT0);
    }
}

boolean CanHw_IsBusOff(void)
{
    return FALSE;
}

uint32 CanHw_GetRxLostCount(void)
{
    return s_lost;
}
