/*
 * main_demo.c
 *
 * [Educational Implementation] scripted UDS session with a full layer trace.
 * Real counterpart: a tester script (CANoe CAPL / python-udsoncan) talking to a
 * real ECU, with a debugger trace on the ECU side. Every line prefixed with a
 * module name shows one hop:
 *
 *   Tester -> Bus -> Can(ISR) -> CanIf -> CanTp -> PduR -> Dcm/DSL -> Dcm/DSD
 *          -> Dcm/DSP -> Rte -> SWC   ... and back down to the Bus.
 *
 * Build/run: python tools/run_uds_demo.py  (output: artifacts/uds-demo/trace.txt)
 */
#include "SimHarness.h"
#include "UdsTrace.h"
#include "Rte_VehicleInfoSWC.h"
#include <stdio.h>
#include <string.h>

static void Demo_Banner(const char *title)
{
    printf("\n==============================================================================\n");
    printf("== %s\n", title);
    printf("==============================================================================\n");
}

static void Demo_Step(const char *title, const uint8 *req, uint16 len, boolean functional, Sim_ResultType *res)
{
    Demo_Banner(title);
    if (Sim_Request(req, len, functional, 300u, res)) {
        printf("-- RESULT: %s  (after %lu ms, %u x NRC 0x78 before)\n", UdsTrace_Hex(res->data, res->length),
               (unsigned long)res->elapsedMs, (unsigned)res->numResponsePending);
    } else {
        printf("-- RESULT: no response (expected for suppressed responses)\n");
    }
}

int main(void)
{
    Sim_ResultType res;
    static const uint8 kMask[4] = { 0x5Au, 0x3Cu, 0x96u, 0xE1u };   /* insecure teaching algorithm */
    uint8 req[16];
    uint8 i;

    UdsTrace_SetEnabled(TRUE);
    Demo_Banner("Power on: EcuM_Init brings up Can -> CanIf -> CanTp -> PduR -> NvM -> Dem -> Dcm -> Rte");
    Sim_PowerOn(1u, 2u);   /* tester FC: BS=1, STmin=2 ms -> one FC per CF, easy to see */

    req[0] = 0x22u; req[1] = 0xF1u; req[2] = 0x90u;
    Demo_Step("22 F1 90  ReadDataByIdentifier VIN (async DID, 1x DCM_E_PENDING, multi-frame response)",
              req, 3u, FALSE, &res);

    req[0] = 0x22u; req[1] = 0xF1u; req[2] = 0x87u;
    Demo_Step("22 F1 87  ReadDataByIdentifier SW version (sync DID, single frame)", req, 3u, FALSE, &res);

    req[0] = 0x10u; req[1] = 0x03u;
    Demo_Step("10 03  DiagnosticSessionControl -> extended session (switch after TX confirmation)",
              req, 2u, FALSE, &res);

    req[0] = 0x27u; req[1] = 0x01u;
    Demo_Step("27 01  SecurityAccess requestSeed", req, 2u, FALSE, &res);
    req[0] = 0x27u; req[1] = 0x02u;
    for (i = 0u; i < 4u; i++) {
        req[2u + i] = (uint8)(res.data[2u + i] ^ kMask[i]);
    }
    Demo_Step("27 02 <key>  SecurityAccess sendKey (key = seed XOR mask)", req, 6u, FALSE, &res);

    req[0] = 0x2Eu; req[1] = 0xF1u; req[2] = 0xA0u;
    for (i = 0u; i < 10u; i++) {
        req[3u + i] = (uint8)(0xA0u + i);
    }
    Demo_Step("2E F1 A0 <10 bytes>  WriteDataByIdentifier (13 byte request = ECU receives FF/CF, sends FC; NvM async)",
              req, 13u, FALSE, &res);

    req[0] = 0x22u; req[1] = 0xF1u; req[2] = 0xA0u; req[3] = 0xF1u; req[4] = 0x87u;
    Demo_Step("22 F1 A0 F1 87  ReadDataByIdentifier with two DIDs in one request", req, 5u, FALSE, &res);

    req[0] = 0x31u; req[1] = 0x01u; req[2] = 0xFFu; req[3] = 0x00u;
    Demo_Step("31 01 FF 00  RoutineControl startRoutine", req, 4u, FALSE, &res);
    Sim_RunMs(150u);
    req[1] = 0x03u;
    Demo_Step("31 03 FF 00  RoutineControl requestRoutineResults (02 = completed)", req, 4u, FALSE, &res);

    req[0] = 0x3Eu; req[1] = 0x00u;
    Demo_Step("3E 00  TesterPresent (physical, response expected)", req, 2u, FALSE, &res);
    req[0] = 0x3Eu; req[1] = 0x80u;
    Demo_Step("3E 80  TesterPresent functional with SPRMIB (handled in DSL, no response)", req, 2u, TRUE, &res);

    req[0] = 0x19u; req[1] = 0x02u; req[2] = 0xFFu;
    Demo_Step("19 02 FF  ReadDTCInformation reportDTCByStatusMask (via Dem stub)", req, 3u, FALSE, &res);
    req[0] = 0x14u; req[1] = 0xFFu; req[2] = 0xFFu; req[3] = 0xFFu;
    Demo_Step("14 FF FF FF  ClearDiagnosticInformation all groups (Dem returns DEM_PENDING once)", req, 4u, FALSE, &res);
    req[0] = 0x19u; req[1] = 0x02u; req[2] = 0xFFu;
    Demo_Step("19 02 FF  again: no DTC left", req, 3u, FALSE, &res);

    req[0] = 0x85u; req[1] = 0x02u;
    Demo_Step("85 02  unsupported SID -> NRC 0x11", req, 2u, FALSE, &res);

    VehicleInfoSWC_SetVinPendingCycles(8u);
    req[0] = 0x22u; req[1] = 0xF1u; req[2] = 0x90u;
    Demo_Step("22 F1 90 with a slow SW-C (8 x DCM_E_PENDING = 80 ms > P2 50 ms) -> NRC 0x78 first",
              req, 3u, FALSE, &res);
    VehicleInfoSWC_SetVinPendingCycles(1u);

    Demo_Banner("Idle 5 s without TesterPresent -> S3 timeout -> default session");
    Sim_RunMs(5100u);

    req[0] = 0x11u; req[1] = 0x01u;
    Demo_Step("11 01  ECUReset hard (reset executed after the positive response is on the bus)", req, 2u, FALSE, &res);
    Sim_RunMs(20u);
    req[0] = 0x22u; req[1] = 0xF1u; req[2] = 0xA0u;
    Demo_Step("22 F1 A0 after reset: value written before survived in (emulated) NvM", req, 3u, FALSE, &res);

    Demo_Banner("Demo finished");
    return 0;
}
