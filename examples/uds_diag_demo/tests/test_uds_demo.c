/*
 * test_uds_demo.c
 *
 * [Educational Implementation] host tests for the educational diagnostic stack.
 * Every test powers on the simulated ECU, talks to it ONLY through the
 * virtual CAN bus (like a real tester would) and checks the UDS bytes.
 * A few white-box checks use public AUTOSAR APIs (Dcm_GetSesCtrlType,
 * Dcm_GetSecurityLevel) and Det counters.
 */
#include "SimHarness.h"
#include "UdsTester.h"
#include "UdsTrace.h"
#include "Dcm.h"
#include "Dem.h"
#include "Det.h"
#include "EcuM.h"
#include "Rte_VehicleInfoSWC.h"
#include <stdio.h>
#include <string.h>

static unsigned g_checks;
static unsigned g_failed_tests;
static boolean  g_current_failed;

#define CHECK(expr) do { g_checks++; if (!(expr)) { \
    printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); g_current_failed = TRUE; return; } } while (0)

/* Send a request and compare the final response byte by byte. */
#define EXPECT(functional, reqArr, expArr) do { \
    Sim_ResultType r_; \
    (void)Sim_Request((reqArr), (uint16)sizeof(reqArr), (functional), 300u, &r_); \
    if (!expect_bytes(&r_, (expArr), (uint16)sizeof(expArr), __LINE__)) { g_current_failed = TRUE; return; } \
} while (0)

#define EXPECT_NO_RESPONSE(functional, reqArr) do { \
    Sim_ResultType r_; \
    g_checks++; \
    if (Sim_Request((reqArr), (uint16)sizeof(reqArr), (functional), 200u, &r_)) { \
        printf("    FAILED line %d: expected no response, got %s\n", __LINE__, UdsTrace_Hex(r_.data, r_.length)); \
        g_current_failed = TRUE; return; } \
} while (0)

static boolean expect_bytes(const Sim_ResultType *r, const uint8 *exp, uint16 expLen, int line)
{
    g_checks++;
    if (!r->received) {
        printf("    FAILED line %d: no response, expected %s\n", line, UdsTrace_Hex(exp, expLen));
        return FALSE;
    }
    if ((r->length != expLen) || (memcmp(r->data, exp, expLen) != 0)) {
        printf("    FAILED line %d: got %s\n                 expected %s\n", line,
               UdsTrace_Hex(r->data, r->length), UdsTrace_Hex(exp, expLen));
        return FALSE;
    }
    return TRUE;
}

static void fresh_ecu(void)
{
    VehicleInfoSWC_SetVinPendingCycles(1u);
    Sim_PowerOn(1u, 2u);
}

static void enter_extended(void)
{
    static const uint8 req[] = { 0x10, 0x03 };
    static const uint8 exp[] = { 0x50, 0x03, 0x00, 0x32, 0x01, 0xF4 };
    EXPECT(FALSE, req, exp);
}

static boolean unlock_level1(void)
{
    static const uint8 seedReq[] = { 0x27, 0x01 };
    static const uint8 mask[4] = { 0x5A, 0x3C, 0x96, 0xE1 };
    uint8 keyReq[6] = { 0x27, 0x02, 0, 0, 0, 0 };
    Sim_ResultType r;
    uint8 i;

    if (!Sim_Request(seedReq, 2u, FALSE, 300u, &r) || (r.length != 6u) || (r.data[0] != 0x67u)) {
        return FALSE;
    }
    for (i = 0u; i < 4u; i++) {
        keyReq[2u + i] = (uint8)(r.data[2u + i] ^ mask[i]);
    }
    return (boolean)(Sim_Request(keyReq, 6u, FALSE, 300u, &r) && (r.length == 2u) &&
                     (r.data[0] == 0x67u) && (r.data[1] == 0x02u));
}

/* ------------------------------------------------------------------ */

static void test_rdbi_vin_multiframe_with_fc(void)
{
    static const uint8 req[] = { 0x22, 0xF1, 0x90 };
    static const uint8 exp[] = { 0x62, 0xF1, 0x90, 'L', 'R', 'H', '8', '5', '0', 'D', 'E', 'M', 'O',
                                 '0', '0', '0', '0', '0', '0', '1' };
    Sim_ResultType r;
    fresh_ecu();
    CHECK(Sim_Request(req, 3u, FALSE, 300u, &r));
    CHECK(expect_bytes(&r, exp, (uint16)sizeof(exp), __LINE__));
    /* 20 bytes = FF(6) + CF(7) + CF(7); tester BS=1 -> FC after FF and after CF1 */
    CHECK(UdsTester_GetFcSentCount() == 2u);
    /* F190 returned DCM_E_PENDING once: finished in the next 10 ms cycle, well inside P2 */
    CHECK(r.numResponsePending == 0u);
    CHECK(Det_GetDevErrorCount() == 0u);
}

static void test_rdbi_sw_version_and_multi_did(void)
{
    static const uint8 req1[] = { 0x22, 0xF1, 0x87 };
    static const uint8 exp1[] = { 0x62, 0xF1, 0x87, 'S', 'W', '0', '1', '0', '2', '0', '3' };
    static const uint8 req2[] = { 0x22, 0xF1, 0x87, 0x12, 0x34, 0xF1, 0x87 };  /* 0x1234 unsupported -> skipped */
    static const uint8 exp2[] = { 0x62, 0xF1, 0x87, 'S', 'W', '0', '1', '0', '2', '0', '3',
                                  0xF1, 0x87, 'S', 'W', '0', '1', '0', '2', '0', '3' };
    static const uint8 req3[] = { 0x22, 0x12, 0x34 };
    static const uint8 exp3[] = { 0x7F, 0x22, 0x31 };
    fresh_ecu();
    EXPECT(FALSE, req1, exp1);
    EXPECT(FALSE, req2, exp2);
    EXPECT(FALSE, req3, exp3);
}

static void test_session_control_p2_values(void)
{
    static const uint8 req1[] = { 0x10, 0x01 };
    static const uint8 exp1[] = { 0x50, 0x01, 0x00, 0x32, 0x01, 0xF4 };   /* P2 = 50 ms, P2* = 500 x 10 ms */
    static const uint8 req2[] = { 0x10, 0x02 };
    static const uint8 exp2[] = { 0x7F, 0x10, 0x12 };                     /* programming session not configured */
    Dcm_SesCtrlType ses = 0u;
    fresh_ecu();
    EXPECT(FALSE, req1, exp1);
    enter_extended();
    CHECK(Dcm_GetSesCtrlType(&ses) == E_OK);
    CHECK(ses == DCM_EXTENDED_DIAGNOSTIC_SESSION);
    EXPECT(FALSE, req2, exp2);
}

static void test_security_access(void)
{
    static const uint8 seedReq[] = { 0x27, 0x01 };
    static const uint8 badKey[] = { 0x27, 0x02, 0x00, 0x00, 0x00, 0x00 };
    static const uint8 nrc24[] = { 0x7F, 0x27, 0x24 };
    static const uint8 nrc35[] = { 0x7F, 0x27, 0x35 };
    static const uint8 nrc36[] = { 0x7F, 0x27, 0x36 };
    static const uint8 nrc37[] = { 0x7F, 0x27, 0x37 };
    static const uint8 nrc7F[] = { 0x7F, 0x27, 0x7F };
    Sim_ResultType r;
    Dcm_SecLevelType lev = 0xFFu;
    fresh_ecu();

    EXPECT(FALSE, seedReq, nrc7F);                     /* 0x27 not allowed in default session */
    enter_extended();
    EXPECT(FALSE, badKey, nrc24);                      /* sendKey without seed */

    /* three wrong keys: 0x35, 0x35, then 0x36 and the delay starts */
    CHECK(Sim_Request(seedReq, 2u, FALSE, 300u, &r) && (r.length == 6u) && (r.data[0] == 0x67u));
    EXPECT(FALSE, badKey, nrc35);
    CHECK(Sim_Request(seedReq, 2u, FALSE, 300u, &r) && (r.data[0] == 0x67u));
    EXPECT(FALSE, badKey, nrc35);
    CHECK(Sim_Request(seedReq, 2u, FALSE, 300u, &r) && (r.data[0] == 0x67u));
    EXPECT(FALSE, badKey, nrc36);
    EXPECT(FALSE, seedReq, nrc37);                     /* delay (3000 ms) not expired */

    Sim_RunMs(3100u);                                  /* < S3 (5000 ms): session stays extended */
    CHECK(unlock_level1());
    CHECK(Dcm_GetSecurityLevel(&lev) == E_OK);
    CHECK(lev == 1u);

    /* already unlocked: requestSeed returns an all-zero seed */
    {
        static const uint8 zeroSeed[] = { 0x67, 0x01, 0x00, 0x00, 0x00, 0x00 };
        EXPECT(FALSE, seedReq, zeroSeed);
    }
}

static void test_wdbi_needs_security(void)
{
    static const uint8 wr[] = { 0x2E, 0xF1, 0xA0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    static const uint8 wrShort[] = { 0x2E, 0xF1, 0xA0, 1, 2 };
    static const uint8 nrc7F[] = { 0x7F, 0x2E, 0x7F };
    static const uint8 nrc33[] = { 0x7F, 0x2E, 0x33 };
    static const uint8 nrc13[] = { 0x7F, 0x2E, 0x13 };
    static const uint8 ok[] = { 0x6E, 0xF1, 0xA0 };
    static const uint8 rd[] = { 0x22, 0xF1, 0xA0 };
    static const uint8 rdExp[] = { 0x62, 0xF1, 0xA0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    uint32 fcBefore;
    fresh_ecu();

    EXPECT(FALSE, wr, nrc7F);                          /* default session: service not allowed */
    enter_extended();
    fcBefore = UdsTester_GetFcReceivedCount();
    EXPECT(FALSE, wr, nrc33);                          /* extended but LOCKED */
    CHECK(UdsTester_GetFcReceivedCount() > fcBefore);  /* 13-byte request: ECU CanTp sent FC */
    CHECK(unlock_level1());
    EXPECT(FALSE, wrShort, nrc13);                     /* wrong data length for the DID */
    EXPECT(FALSE, wr, ok);                             /* NvM write -> DCM_E_PENDING -> 6E */
    EXPECT(FALSE, rd, rdExp);
}

static void test_routine_control(void)
{
    static const uint8 start[] = { 0x31, 0x01, 0xFF, 0x00 };
    static const uint8 startOk[] = { 0x71, 0x01, 0xFF, 0x00 };
    static const uint8 results[] = { 0x31, 0x03, 0xFF, 0x00 };
    static const uint8 running[] = { 0x71, 0x03, 0xFF, 0x00, 0x01 };
    static const uint8 done[] = { 0x71, 0x03, 0xFF, 0x00, 0x02 };
    static const uint8 nrc24[] = { 0x7F, 0x31, 0x24 };
    static const uint8 unknownRid[] = { 0x31, 0x01, 0x12, 0x34 };
    static const uint8 nrc31[] = { 0x7F, 0x31, 0x31 };
    fresh_ecu();
    enter_extended();
    EXPECT(FALSE, results, nrc24);                     /* results before start */
    EXPECT(FALSE, unknownRid, nrc31);
    EXPECT(FALSE, start, startOk);
    EXPECT(FALSE, results, running);
    Sim_RunMs(150u);
    EXPECT(FALSE, results, done);
}

static void test_tester_present(void)
{
    static const uint8 tp[] = { 0x3E, 0x00 };
    static const uint8 tpOk[] = { 0x7E, 0x00 };
    static const uint8 tpSup[] = { 0x3E, 0x80 };
    Dcm_SesCtrlType ses = 0u;
    uint8 i;
    fresh_ecu();
    EXPECT(FALSE, tp, tpOk);
    EXPECT_NO_RESPONSE(FALSE, tpSup);                  /* physical, SPRMIB: DSD suppresses */
    EXPECT_NO_RESPONSE(TRUE, tpSup);                   /* functional: DSL handles it alone */

    enter_extended();
    for (i = 0u; i < 4u; i++) {                        /* 4 x 3 s > S3, kept alive by 3E 80 */
        Sim_RunMs(3000u);
        EXPECT_NO_RESPONSE(TRUE, tpSup);
    }
    CHECK(Dcm_GetSesCtrlType(&ses) == E_OK);
    CHECK(ses == DCM_EXTENDED_DIAGNOSTIC_SESSION);
}

static void test_unknown_sid_and_lengths(void)
{
    static const uint8 unk[] = { 0x85, 0x02 };
    static const uint8 unkNrc[] = { 0x7F, 0x85, 0x11 };
    static const uint8 shortRdbi[] = { 0x22, 0xF1 };
    static const uint8 nrc22_13[] = { 0x7F, 0x22, 0x13 };
    static const uint8 longDsc[] = { 0x10, 0x03, 0x00 };
    static const uint8 nrc10_13[] = { 0x7F, 0x10, 0x13 };
    static const uint8 longTp[] = { 0x3E, 0x00, 0x00 };
    static const uint8 nrc3E_13[] = { 0x7F, 0x3E, 0x13 };
    static const uint8 badSub[] = { 0x3E, 0x05 };
    static const uint8 nrc3E_12[] = { 0x7F, 0x3E, 0x12 };
    fresh_ecu();
    EXPECT(FALSE, unk, unkNrc);
    EXPECT_NO_RESPONSE(TRUE, unk);                     /* functional: NRC 0x11 suppressed */
    EXPECT(FALSE, shortRdbi, nrc22_13);                /* DSD minimum length */
    EXPECT(FALSE, longDsc, nrc10_13);                  /* DSP exact length */
    EXPECT(FALSE, longTp, nrc3E_13);
    EXPECT(FALSE, badSub, nrc3E_12);
}

static void test_s3_timeout(void)
{
    Dcm_SesCtrlType ses = 0u;
    Dcm_SecLevelType lev = 0u;
    fresh_ecu();
    enter_extended();
    CHECK(unlock_level1());
    Sim_RunMs(4900u);
    CHECK(Dcm_GetSesCtrlType(&ses) == E_OK);
    CHECK(ses == DCM_EXTENDED_DIAGNOSTIC_SESSION);
    Sim_RunMs(200u);
    CHECK(Dcm_GetSesCtrlType(&ses) == E_OK);
    CHECK(ses == DCM_DEFAULT_SESSION);
    CHECK(Dcm_GetSecurityLevel(&lev) == E_OK);
    CHECK(lev == DCM_SEC_LEV_LOCKED);
}

static void test_response_pending_0x78(void)
{
    static const uint8 req[] = { 0x22, 0xF1, 0x90 };
    Sim_ResultType r;
    fresh_ecu();
    VehicleInfoSWC_SetVinPendingCycles(8u);            /* 80 ms > P2 (50 ms - 10 ms adjust) */
    CHECK(Sim_Request(req, 3u, FALSE, 500u, &r));
    CHECK(r.numResponsePending == 1u);                 /* exactly one 7F 22 78 */
    CHECK(r.length == 20u);
    CHECK((r.data[0] == 0x62u) && (r.data[1] == 0xF1u) && (r.data[2] == 0x90u));
    CHECK(r.elapsedMs >= 80u);
    VehicleInfoSWC_SetVinPendingCycles(1u);
}

static void test_dtc_clear_and_read(void)
{
    static const uint8 rd[] = { 0x19, 0x02, 0xFF };
    static const uint8 rdExp[] = { 0x59, 0x02, 0x7F, 0xC1, 0x00, 0x00, 0x0B, 0x05, 0x62, 0x00, 0x08 };
    static const uint8 clr[] = { 0x14, 0xFF, 0xFF, 0xFF };
    static const uint8 clrExp[] = { 0x54 };
    static const uint8 rdEmpty[] = { 0x59, 0x02, 0x7F };
    static const uint8 rdBadSub[] = { 0x19, 0x0A, 0xFF };
    static const uint8 nrc12[] = { 0x7F, 0x19, 0x12 };
    fresh_ecu();
    Dem_SimSetDtcStatus(0xC10000u, 0x0Bu);
    Dem_SimSetDtcStatus(0x056200u, 0x08u);
    EXPECT(FALSE, rd, rdExp);
    EXPECT(FALSE, rdBadSub, nrc12);
    EXPECT(FALSE, clr, clrExp);
    EXPECT(FALSE, rd, rdEmpty);
}

static void test_ecu_reset(void)
{
    static const uint8 rst[] = { 0x11, 0x01 };
    static const uint8 rstOk[] = { 0x51, 0x01 };
    uint32 resets;
    Dcm_SesCtrlType ses = 0u;
    fresh_ecu();
    enter_extended();
    resets = EcuM_SimGetResetCount();
    EXPECT(FALSE, rst, rstOk);                         /* response first ... */
    Sim_RunMs(20u);
    CHECK(EcuM_SimGetResetCount() == resets + 1u);     /* ... then the reset */
    CHECK(Dcm_GetSesCtrlType(&ses) == E_OK);
    CHECK(ses == DCM_DEFAULT_SESSION);
}

/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

int main(void)
{
    static const TestCase tests[] = {
        { "22 F1 90 multi-frame VIN with FC",             test_rdbi_vin_multiframe_with_fc },
        { "22 F1 87 + multi-DID + unsupported DID",       test_rdbi_sw_version_and_multi_did },
        { "10 01/03 -> 50 xx + P2/P2*, 10 02 -> 0x12",    test_session_control_p2_values },
        { "27 01/02 good+bad key, 0x24/0x35/0x36/0x37",   test_security_access },
        { "2E without security 0x33, with security 6E",   test_wdbi_needs_security },
        { "31 01/03 FF00 routine + 0x24/0x31",            test_routine_control },
        { "3E 00 -> 7E 00, 3E 80 -> no response, S3",     test_tester_present },
        { "unknown SID 0x11, wrong length 0x13, 0x12",    test_unknown_sid_and_lengths },
        { "S3 timeout -> default session + locked",       test_s3_timeout },
        { "NRC 0x78 response pending sequence",           test_response_pending_0x78 },
        { "19 02 FF / 14 FF FF FF via Dem stub",          test_dtc_clear_and_read },
        { "11 01 -> 51 01, reset after response",         test_ecu_reset }
    };
    unsigned i;
    unsigned n = (unsigned)(sizeof(tests) / sizeof(tests[0]));

    UdsTrace_SetEnabled(FALSE);
    printf("UDS educational stack host tests (%u test cases)\n", n);
    for (i = 0u; i < n; i++) {
        g_current_failed = FALSE;
        tests[i].fn();
        if (!g_current_failed && (Det_GetDevErrorCount() != 0u)) {
            printf("    FAILED: %u DET development error(s) reported\n", (unsigned)Det_GetDevErrorCount());
            g_current_failed = TRUE;
        }
        printf("[%s] %s\n", g_current_failed ? "FAIL" : "PASS", tests[i].name);
        if (g_current_failed) {
            g_failed_tests++;
        }
    }
    printf("\n%u/%u test cases passed, %u checks executed\n", n - g_failed_tests, n, g_checks);
    return (g_failed_tests == 0u) ? 0 : 1;
}
