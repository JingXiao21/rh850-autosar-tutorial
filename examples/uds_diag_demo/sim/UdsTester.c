/*
 * UdsTester.c
 *
 * [Educational Implementation] PC-side tester with its own minimal ISO-TP.
 * See UdsTester.h. Physical requests may be segmented (FF + CF, honouring the
 * ECU's FC BS/STmin); responses are reassembled and the tester sends FC
 * frames with its own BS/STmin.
 */
#include "UdsTester.h"
#include "VirtualCanBus.h"
#include "UdsTrace.h"
#include <string.h>

typedef struct {
    boolean active;
    boolean waitFc;
    uint8   data[UDS_TESTER_MAX_MSG];
    uint16  length;
    uint16  sent;
    uint8   sn;
    uint8   bs;
    uint8   bsRemaining;
    uint8   stMin;
    uint8   stMinTimer;
} UdsTester_TxType;

typedef struct {
    boolean active;
    uint8   data[UDS_TESTER_MAX_MSG];
    uint16  total;
    uint16  received;
    uint8   nextSn;
    uint8   blockRemaining;
} UdsTester_RxType;

static UdsTester_ConfigType   UdsTester_Cfg;
static UdsTester_TxType       UdsTester_Tx;
static UdsTester_RxType       UdsTester_Rx;
static UdsTester_ResponseType UdsTester_Resp[UDS_TESTER_MAX_RESPONSES];
static uint8  UdsTester_RespHead;
static uint8  UdsTester_RespCount;
static uint32 UdsTester_FcSent;
static uint32 UdsTester_FcReceived;

static void UdsTester_SendFrame(uint32 id, uint8 *frame, uint8 used)
{
    uint8 i;
    for (i = used; i < 8u; i++) {
        frame[i] = UdsTester_Cfg.padding;
    }
    VirtualCanBus_TesterSend(id, frame, 8u);
}

static void UdsTester_PushResponse(const uint8 *data, uint16 length)
{
    UdsTester_ResponseType *r;
    if (UdsTester_RespCount >= UDS_TESTER_MAX_RESPONSES) {
        return;
    }
    r = &UdsTester_Resp[(UdsTester_RespHead + UdsTester_RespCount) % UDS_TESTER_MAX_RESPONSES];
    r->length = length;
    (void)memcpy(r->data, data, length);
    r->timeMs = 0u;
    UdsTester_RespCount++;
    UDS_TRACE("Tester", "<<< UDS response (%u bytes): %s", (unsigned)length, UdsTrace_Hex(data, length));
}

static void UdsTester_SendFc(void)
{
    uint8 frame[8];
    frame[0] = 0x30u;                       /* FC CTS */
    frame[1] = UdsTester_Cfg.bs;
    frame[2] = UdsTester_Cfg.stMin;
    UdsTester_FcSent++;
    UDS_TRACE("Tester", "send FC CTS BS=%u STmin=%u", (unsigned)UdsTester_Cfg.bs, (unsigned)UdsTester_Cfg.stMin);
    UdsTester_SendFrame(UdsTester_Cfg.physReqId, frame, 3u);
}

void UdsTester_Init(const UdsTester_ConfigType *cfg)
{
    UdsTester_Cfg = *cfg;
    (void)memset(&UdsTester_Tx, 0, sizeof(UdsTester_Tx));
    (void)memset(&UdsTester_Rx, 0, sizeof(UdsTester_Rx));
    UdsTester_RespHead = 0u;
    UdsTester_RespCount = 0u;
    UdsTester_FcSent = 0u;
    UdsTester_FcReceived = 0u;
}

Std_ReturnType UdsTester_SendRequest(const uint8 *data, uint16 length, boolean functional)
{
    uint8 frame[8];
    uint32 id = functional ? UdsTester_Cfg.funcReqId : UdsTester_Cfg.physReqId;

    if (UdsTester_Tx.active || (length == 0u) || (length > UDS_TESTER_MAX_MSG) || (functional && (length > 7u))) {
        return E_NOT_OK;
    }
    UDS_TRACE("Tester", ">>> UDS request %s (%u bytes): %s", functional ? "FUNCTIONAL 0x7DF" : "physical 0x7E0",
              (unsigned)length, UdsTrace_Hex(data, length));
    if (length <= 7u) {
        frame[0] = (uint8)length;
        (void)memcpy(&frame[1], data, length);
        UdsTester_SendFrame(id, frame, (uint8)(length + 1u));
        return E_OK;
    }
    (void)memcpy(UdsTester_Tx.data, data, length);
    UdsTester_Tx.length = length;
    UdsTester_Tx.sent = 6u;
    UdsTester_Tx.sn = 1u;
    UdsTester_Tx.active = TRUE;
    UdsTester_Tx.waitFc = TRUE;
    frame[0] = (uint8)(0x10u | ((length >> 8) & 0x0Fu));
    frame[1] = (uint8)(length & 0xFFu);
    (void)memcpy(&frame[2], data, 6u);
    UdsTester_SendFrame(id, frame, 8u);
    return E_OK;
}

static void UdsTester_HandleFrame(const VirtualCanBus_FrameType *f)
{
    uint8 pci = (uint8)(f->data[0] >> 4);
    uint16 n;

    switch (pci) {
    case 0x0u:   /* SF */
        n = (uint16)(f->data[0] & 0x0Fu);
        if ((n >= 1u) && (n <= 7u)) {
            UdsTester_PushResponse(&f->data[1], n);
        }
        break;
    case 0x1u:   /* FF */
        UdsTester_Rx.total = (uint16)(((uint16)(f->data[0] & 0x0Fu) << 8) | f->data[1]);
        if (UdsTester_Rx.total > UDS_TESTER_MAX_MSG) {
            return;
        }
        (void)memcpy(UdsTester_Rx.data, &f->data[2], 6u);
        UdsTester_Rx.received = 6u;
        UdsTester_Rx.nextSn = 1u;
        UdsTester_Rx.active = TRUE;
        UdsTester_Rx.blockRemaining = UdsTester_Cfg.bs;
        UdsTester_SendFc();
        break;
    case 0x2u:   /* CF */
        if (!UdsTester_Rx.active || ((f->data[0] & 0x0Fu) != UdsTester_Rx.nextSn)) {
            UDS_TRACE("Tester", "unexpected CF / wrong SN -> reception aborted");
            UdsTester_Rx.active = FALSE;
            return;
        }
        n = (uint16)(UdsTester_Rx.total - UdsTester_Rx.received);
        if (n > 7u) {
            n = 7u;
        }
        (void)memcpy(&UdsTester_Rx.data[UdsTester_Rx.received], &f->data[1], n);
        UdsTester_Rx.received = (uint16)(UdsTester_Rx.received + n);
        UdsTester_Rx.nextSn = (uint8)((UdsTester_Rx.nextSn + 1u) & 0x0Fu);
        if (UdsTester_Rx.received >= UdsTester_Rx.total) {
            UdsTester_Rx.active = FALSE;
            UdsTester_PushResponse(UdsTester_Rx.data, UdsTester_Rx.total);
        } else if (UdsTester_Cfg.bs != 0u) {
            UdsTester_Rx.blockRemaining--;
            if (UdsTester_Rx.blockRemaining == 0u) {
                UdsTester_Rx.blockRemaining = UdsTester_Cfg.bs;
                UdsTester_SendFc();
            }
        } else {
            /* BS = 0: no further FC */
        }
        break;
    case 0x3u:   /* FC from the ECU for our segmented request */
        UdsTester_FcReceived++;
        if (!UdsTester_Tx.active || !UdsTester_Tx.waitFc) {
            return;
        }
        if ((f->data[0] & 0x0Fu) == 0x0u) {
            UdsTester_Tx.waitFc = FALSE;
            UdsTester_Tx.bs = f->data[1];
            UdsTester_Tx.bsRemaining = f->data[1];
            UdsTester_Tx.stMin = (f->data[2] <= 0x7Fu) ? f->data[2] : 1u;
            UdsTester_Tx.stMinTimer = 0u;
            UDS_TRACE("Tester", "got FC CTS from ECU: BS=%u STmin=%u", (unsigned)UdsTester_Tx.bs,
                      (unsigned)UdsTester_Tx.stMin);
        } else if ((f->data[0] & 0x0Fu) == 0x2u) {
            UDS_TRACE("Tester", "got FC OVFLW from ECU -> request aborted");
            UdsTester_Tx.active = FALSE;
        } else {
            UDS_TRACE("Tester", "got FC WAIT from ECU");
        }
        break;
    default:
        break;
    }
}

void UdsTester_MainFunction(void)
{
    VirtualCanBus_FrameType f;

    while (VirtualCanBus_TesterReceive(&f)) {
        if (f.id == UdsTester_Cfg.respId) {
            UdsTester_HandleFrame(&f);
        }
    }
    if (UdsTester_Tx.active && !UdsTester_Tx.waitFc) {
        if (UdsTester_Tx.stMinTimer > 0u) {
            UdsTester_Tx.stMinTimer--;
        }
        if (UdsTester_Tx.stMinTimer == 0u) {
            uint8 frame[8];
            uint16 n = (uint16)(UdsTester_Tx.length - UdsTester_Tx.sent);
            if (n > 7u) {
                n = 7u;
            }
            frame[0] = (uint8)(0x20u | UdsTester_Tx.sn);
            (void)memcpy(&frame[1], &UdsTester_Tx.data[UdsTester_Tx.sent], n);
            UdsTester_SendFrame(UdsTester_Cfg.physReqId, frame, (uint8)(n + 1u));
            UdsTester_Tx.sent = (uint16)(UdsTester_Tx.sent + n);
            UdsTester_Tx.sn = (uint8)((UdsTester_Tx.sn + 1u) & 0x0Fu);
            UdsTester_Tx.stMinTimer = UdsTester_Tx.stMin;
            if (UdsTester_Tx.sent >= UdsTester_Tx.length) {
                UdsTester_Tx.active = FALSE;
            } else if (UdsTester_Tx.bs != 0u) {
                UdsTester_Tx.bsRemaining--;
                if (UdsTester_Tx.bsRemaining == 0u) {
                    UdsTester_Tx.waitFc = TRUE;
                }
            } else {
                /* BS = 0: send everything */
            }
        }
    }
}

boolean UdsTester_GetResponse(UdsTester_ResponseType *resp)
{
    if (UdsTester_RespCount == 0u) {
        return FALSE;
    }
    *resp = UdsTester_Resp[UdsTester_RespHead];
    UdsTester_RespHead = (uint8)((UdsTester_RespHead + 1u) % UDS_TESTER_MAX_RESPONSES);
    UdsTester_RespCount--;
    return TRUE;
}

boolean UdsTester_IsTxBusy(void) { return UdsTester_Tx.active; }
uint32 UdsTester_GetFcSentCount(void) { return UdsTester_FcSent; }
uint32 UdsTester_GetFcReceivedCount(void) { return UdsTester_FcReceived; }
