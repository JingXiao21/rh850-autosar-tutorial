/*
 * SimHarness.c
 *
 * [Educational Implementation] See SimHarness.h.
 */
#include "SimHarness.h"
#include "SimClock.h"
#include "VirtualCanBus.h"
#include "BswScheduler.h"
#include "EcuM.h"
#include "Can.h"
#include "UdsTrace.h"
#include <string.h>

void Sim_PowerOn(uint8 testerBs, uint8 testerStMin)
{
    UdsTester_ConfigType cfg;

    SimClock_Reset();
    VirtualCanBus_Reset();
    Can_DeInit();      /* power cycle: the driver starts from UNINIT again */
    EcuM_Init();
    cfg.physReqId = 0x7E0u;
    cfg.funcReqId = 0x7DFu;
    cfg.respId = 0x7E8u;
    cfg.bs = testerBs;
    cfg.stMin = testerStMin;
    cfg.padding = 0x55u;
    UdsTester_Init(&cfg);
    Sim_RunMs(10u);   /* let Can_MainFunction_Mode confirm STARTED */
}

void Sim_RunMs(uint32 ms)
{
    uint32 i;
    for (i = 0u; i < ms; i++) {
        BswScheduler_Tick1ms();
        UdsTester_MainFunction();
    }
}

boolean Sim_Request(const uint8 *req, uint16 length, boolean functional, uint32 timeoutMs, Sim_ResultType *out)
{
    uint32 start = SimClock_NowMs();
    UdsTester_ResponseType r;

    (void)memset(out, 0, sizeof(*out));
    while (UdsTester_GetResponse(&r)) {
        /* drop stale responses */
    }
    if (UdsTester_SendRequest(req, length, functional) != E_OK) {
        return FALSE;
    }
    while ((SimClock_NowMs() - start) < timeoutMs) {
        Sim_RunMs(1u);
        while (UdsTester_GetResponse(&r)) {
            if ((r.length == 3u) && (r.data[0] == 0x7Fu) && (r.data[2] == 0x78u)) {
                out->numResponsePending++;
                continue;
            }
            out->received = TRUE;
            out->length = r.length;
            (void)memcpy(out->data, r.data, r.length);
            out->elapsedMs = SimClock_NowMs() - start;
            return TRUE;
        }
    }
    out->elapsedMs = SimClock_NowMs() - start;
    UDS_TRACE("Tester", "no final response within %lu ms", (unsigned long)timeoutMs);
    return FALSE;
}
