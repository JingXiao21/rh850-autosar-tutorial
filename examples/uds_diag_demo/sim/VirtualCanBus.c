/*
 * VirtualCanBus.c
 *
 * [Educational Implementation]
 * Real counterpart: CAN bus wire + RS-CANFD message RAM + PC CAN interface.
 * See VirtualCanBus.h for the split between the hardware view and the
 * tester view.
 */
#include "VirtualCanBus.h"
#include "UdsTrace.h"
#include <string.h>

typedef struct {
    boolean used;
    uint32  code;
    uint32  mask;
    uint16  label;
} VCan_RxRuleType;

typedef struct {
    VirtualCanBus_FrameType frame;
    uint16                  label;
} VCan_RxEntryType;

typedef struct {
    boolean                 requested;  /* TMCp.TMTR = 1 (transmission request) */
    boolean                 complete;   /* TMSTSp.TMTRF = "transmit complete"    */
    VirtualCanBus_FrameType frame;
} VCan_TxBufferType;

static VCan_RxRuleType   VCan_Rules[VCAN_NUM_RX_RULES];
static VCan_RxEntryType  VCan_RxFifo[VCAN_RX_FIFO_DEPTH];
static uint8             VCan_RxHead;
static uint8             VCan_RxCount;
static VCan_TxBufferType VCan_TxBuf[VCAN_NUM_TX_BUFFERS];
static boolean           VCan_Online;

static VirtualCanBus_FrameType VCan_TesterRx[VCAN_TESTER_RX_DEPTH];
static uint8  VCan_TesterHead;
static uint8  VCan_TesterCount;
static uint32 VCan_EcuTxCount;
static uint32 VCan_DroppedCount;

void VirtualCanBus_Reset(void)
{
    (void)memset(VCan_Rules, 0, sizeof(VCan_Rules));
    (void)memset(VCan_RxFifo, 0, sizeof(VCan_RxFifo));
    (void)memset(VCan_TxBuf, 0, sizeof(VCan_TxBuf));
    VCan_RxHead = 0u;
    VCan_RxCount = 0u;
    VCan_Online = FALSE;
    VCan_TesterHead = 0u;
    VCan_TesterCount = 0u;
    VCan_EcuTxCount = 0u;
    VCan_DroppedCount = 0u;
}

/* ---------------- hardware view ---------------- */

void VirtualCanBus_HwSetRxRule(uint8 rule, uint32 code, uint32 mask, uint16 label)
{
    if (rule < VCAN_NUM_RX_RULES) {
        VCan_Rules[rule].used = TRUE;
        VCan_Rules[rule].code = code;
        VCan_Rules[rule].mask = mask;
        VCan_Rules[rule].label = label;
    }
}

void VirtualCanBus_HwSetOnline(boolean online)
{
    VCan_Online = online;
    if (!online) {
        uint8 i;
        /* Channel reset/halt: pending transmissions are aborted. */
        for (i = 0u; i < VCAN_NUM_TX_BUFFERS; i++) {
            VCan_TxBuf[i].requested = FALSE;
        }
    }
}

boolean VirtualCanBus_HwRxPending(void)
{
    return (VCan_RxCount != 0u) ? TRUE : FALSE;
}

boolean VirtualCanBus_HwRxFifoRead(VirtualCanBus_FrameType *frame, uint16 *label)
{
    if (VCan_RxCount == 0u) {
        return FALSE;
    }
    *frame = VCan_RxFifo[VCan_RxHead].frame;
    *label = VCan_RxFifo[VCan_RxHead].label;
    /* RS-CANFD: reading the FIFO entry is completed by writing RFPCTRx = 0xFF
     * to advance the read pointer. */
    VCan_RxHead = (uint8)((VCan_RxHead + 1u) % VCAN_RX_FIFO_DEPTH);
    VCan_RxCount--;
    return TRUE;
}

Std_ReturnType VirtualCanBus_HwTxBufferWrite(uint8 txBuffer, const VirtualCanBus_FrameType *frame)
{
    if ((txBuffer >= VCAN_NUM_TX_BUFFERS) || VCan_TxBuf[txBuffer].requested) {
        return E_NOT_OK;
    }
    VCan_TxBuf[txBuffer].frame = *frame;
    VCan_TxBuf[txBuffer].complete = FALSE;
    VCan_TxBuf[txBuffer].requested = TRUE;
    return E_OK;
}

boolean VirtualCanBus_HwTxBufferBusy(uint8 txBuffer)
{
    return (txBuffer < VCAN_NUM_TX_BUFFERS) ? VCan_TxBuf[txBuffer].requested : TRUE;
}

boolean VirtualCanBus_HwTxCompleteFlag(uint8 txBuffer)
{
    boolean flag = FALSE;
    if ((txBuffer < VCAN_NUM_TX_BUFFERS) && VCan_TxBuf[txBuffer].complete) {
        VCan_TxBuf[txBuffer].complete = FALSE;  /* write-to-clear in real HW */
        flag = TRUE;
    }
    return flag;
}

/* ---------------- bus timing ---------------- */

void VirtualCanBus_Tick(void)
{
    uint8 i;
    for (i = 0u; i < VCAN_NUM_TX_BUFFERS; i++) {
        if (VCan_TxBuf[i].requested && VCan_Online) {
            const VirtualCanBus_FrameType *f = &VCan_TxBuf[i].frame;
            UDS_TRACE("Bus", "ECU    -> wire  ID=0x%03lX DLC=%u  %s",
                      (unsigned long)f->id, (unsigned)f->dlc, UdsTrace_Hex(f->data, f->dlc));
            if (VCan_TesterCount < VCAN_TESTER_RX_DEPTH) {
                uint8 slot = (uint8)((VCan_TesterHead + VCan_TesterCount) % VCAN_TESTER_RX_DEPTH);
                VCan_TesterRx[slot] = *f;
                VCan_TesterCount++;
            }
            VCan_TxBuf[i].requested = FALSE;
            VCan_TxBuf[i].complete = TRUE;   /* frame was ACKed by the tester */
            VCan_EcuTxCount++;
        }
    }
}

/* ---------------- tester view ---------------- */

void VirtualCanBus_TesterSend(uint32 id, const uint8 *data, uint8 dlc)
{
    uint8 r;
    VCan_RxEntryType entry;

    (void)memset(&entry, 0, sizeof(entry));
    entry.frame.id = id;
    entry.frame.dlc = (dlc > 8u) ? 8u : dlc;
    (void)memcpy(entry.frame.data, data, entry.frame.dlc);
    UDS_TRACE("Bus", "Tester -> wire  ID=0x%03lX DLC=%u  %s",
              (unsigned long)id, (unsigned)entry.frame.dlc, UdsTrace_Hex(entry.frame.data, entry.frame.dlc));

    if (!VCan_Online) {
        VCan_DroppedCount++;
        UDS_TRACE("Bus", "  ECU controller not STARTED: frame not received");
        return;
    }
    /* Acceptance filtering happens in hardware (RS-CANFD receive rules):
     * a frame that matches no rule never reaches software. */
    for (r = 0u; r < VCAN_NUM_RX_RULES; r++) {
        if (VCan_Rules[r].used && ((id & VCan_Rules[r].mask) == (VCan_Rules[r].code & VCan_Rules[r].mask))) {
            break;
        }
    }
    if (r == VCAN_NUM_RX_RULES) {
        VCan_DroppedCount++;
        UDS_TRACE("Bus", "  no receive rule matches ID=0x%03lX: hardware discards it", (unsigned long)id);
        return;
    }
    if (VCan_RxCount >= VCAN_RX_FIFO_DEPTH) {
        VCan_DroppedCount++;  /* RX FIFO overflow (RFSTSx.RFMLT in real HW) */
        UDS_TRACE("Bus", "  RX FIFO full: message lost");
        return;
    }
    entry.label = VCan_Rules[r].label;
    VCan_RxFifo[(VCan_RxHead + VCan_RxCount) % VCAN_RX_FIFO_DEPTH] = entry;
    VCan_RxCount++;
}

boolean VirtualCanBus_TesterReceive(VirtualCanBus_FrameType *frame)
{
    if (VCan_TesterCount == 0u) {
        return FALSE;
    }
    *frame = VCan_TesterRx[VCan_TesterHead];
    VCan_TesterHead = (uint8)((VCan_TesterHead + 1u) % VCAN_TESTER_RX_DEPTH);
    VCan_TesterCount--;
    return TRUE;
}

uint32 VirtualCanBus_GetEcuTxCount(void) { return VCan_EcuTxCount; }
uint32 VirtualCanBus_GetDroppedCount(void) { return VCan_DroppedCount; }
