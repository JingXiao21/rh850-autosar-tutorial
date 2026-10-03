// SOURCES: bsw/com/Com.c
// [Educational Implementation] Host unit test of Com: signal pack/unpack (LE/BE/signed/unaligned), I-PDU groups,
// IMMEDIATE vs DEFERRED reception, DIRECT/PERIODIC transmission, Det checks. PduR and Det are mocked.
#define TEST_MOCK_DET
#include "test_support.h"
#include "Com.h"
#include "PduR.h"

#if COM_NUM_IPDUS < 4
#error "this test needs COM_NUM_IPDUS >= 4 (LightEcu-style configuration)"
#endif

/* ---- mock of the lower layer ---- */
static int      m_txCalls;
static PduIdType m_lastPduR;
static uint8    m_lastData[8];
static PduLengthType m_lastLen;
static Std_ReturnType m_txResult = E_OK;
Std_ReturnType PduR_ComTransmit(PduIdType id, const PduInfoType *p)
{
    uint8 i;
    m_txCalls++;
    m_lastPduR = id;
    m_lastLen = p->SduLength;
    for (i = 0u; i < p->SduLength && i < 8u; i++) { m_lastData[i] = p->SduDataPtr[i]; }
    return m_txResult;
}

/* ---- notifications ---- */
static int n_deferred, n_immediate;
static void cb_deferred(void)  { n_deferred++; }
static void cb_immediate(void) { n_immediate++; }

/* ---- configuration: ipdu0 RX deferred(8), ipdu1 RX immediate(1), ipdu2 TX direct(2), ipdu3 TX periodic(8) ---- */
static uint8 buf0[8], buf1[1], buf2[2], buf3[8];
enum { S_LE16, S_BE16, S_S8, S_BOOL, S_IMM, S_TRIG, S_PEND, S_U32, S_U16TX, S_S16BE };
static const Com_SignalConfigType signals[] = {
    /* bitPos size type          endian             ipdu init  transfer     notification */
    {  0u, 16u, COM_UINT16, COM_LITTLE_ENDIAN, 0u, 0x0000u, COM_PENDING,   cb_deferred  },  /* bytes 0..1 */
    { 23u, 16u, COM_UINT16, COM_BIG_ENDIAN,    0u, 0x0000u, COM_PENDING,   NULL_PTR     },  /* bytes 2..3, MSB first */
    { 32u,  5u, COM_SINT8,  COM_LITTLE_ENDIAN, 0u, 0x0000u, COM_PENDING,   NULL_PTR     },  /* byte4 bits 0..4 */
    { 37u,  1u, COM_BOOLEAN,COM_LITTLE_ENDIAN, 0u, 0x0000u, COM_PENDING,   NULL_PTR     },  /* byte4 bit 5 */
    {  0u,  8u, COM_UINT8,  COM_LITTLE_ENDIAN, 1u, 0x00FFu, COM_PENDING,   cb_immediate },  /* init 255 */
    {  0u,  8u, COM_UINT8,  COM_LITTLE_ENDIAN, 2u, 0x0000u, COM_TRIGGERED, NULL_PTR     },
    {  8u,  8u, COM_UINT8,  COM_LITTLE_ENDIAN, 2u, 0x0000u, COM_PENDING,   NULL_PTR     },
    {  4u, 32u, COM_UINT32, COM_LITTLE_ENDIAN, 3u, 0x0000u, COM_PENDING,   NULL_PTR     },  /* bits 4..35 unaligned */
    { 40u, 16u, COM_UINT16, COM_LITTLE_ENDIAN, 3u, 0x1234u, COM_PENDING,   NULL_PTR     },
    { 55u, 16u, COM_SINT16, COM_BIG_ENDIAN,    0u, 0x0000u, COM_PENDING,   NULL_PTR     }   /* overlaps (test only, unused) */
};
static const Com_IpduConfigType ipdus[] = {
    { COM_PDU_RX, 8u, buf0, 0u, 0u, COM_TX_MODE_NONE,     0u,  0u, COM_RX_DEFERRED  },
    { COM_PDU_RX, 1u, buf1, 0u, 0u, COM_TX_MODE_NONE,     0u,  0u, COM_RX_IMMEDIATE },
    { COM_PDU_TX, 2u, buf2, 5u, 0u, COM_TX_MODE_DIRECT,   0u,  0u, COM_RX_IMMEDIATE },
    { COM_PDU_TX, 8u, buf3, 6u, 0u, COM_TX_MODE_PERIODIC, 20u, 0u, COM_RX_IMMEDIATE }
};
const Com_ConfigType Com_Config = { 9u, signals, 4u, ipdus, 10u };    /* last signal (S_S16BE) is excluded: numSignals = 9 */

static void rx(PduIdType id, const uint8 *d, PduLengthType len)
{
    PduInfoType pi;
    pi.SduDataPtr = (uint8 *)d; pi.MetaDataPtr = NULL_PTR; pi.SduLength = len;
    Com_RxIndication(id, &pi);
}

int main(void)
{
    uint8 u8 = 0u; uint16 u16 = 0u; uint32 u32 = 0u; sint8 s8 = 0;
    (void)S_S16BE;

    /* ---- before init: Det error, no crash ---- */
    CHECK_EQ(Com_SendSignal(S_TRIG, &u8), COM_SERVICE_NOT_AVAILABLE);
    CHECK(t_detErrors == 1);
    CHECK(Com_GetStatus() == COM_UNINIT);

    Com_Init(&Com_Config);
    CHECK(Com_GetStatus() == COM_INIT);
    /* all groups stopped after Com_Init */
    CHECK_EQ(Com_ReceiveSignal(S_IMM, &u8), COM_SERVICE_NOT_AVAILABLE);
    CHECK_EQ(Com_SendSignal(S_TRIG, &u8), COM_SERVICE_NOT_AVAILABLE);
    CHECK_EQ(Com_TriggerIPDUSend(3u), E_NOT_OK);

    Com_IpduGroupStart(0u, TRUE);
    CHECK(t_traceCount("COM GROUP_START 0") == 1);
    /* init values: AmbientLight-style signal has 255, the u16 TX signal 0x1234 packed at bits 40..55 */
    CHECK_EQ(Com_ReceiveSignal(S_IMM, &u8), E_OK);
    CHECK_EQ(u8, 255);
    CHECK_EQ(Com_ReceiveSignal(S_U16TX, &u16), E_OK);
    CHECK_EQ(u16, 0x1234);
    CHECK_EQ(buf3[5], 0x34);       /* LE: byte 5 = low byte */
    CHECK_EQ(buf3[6], 0x12);

    /* ---- RX unpack: LE16, BE16, signed 5 bit, boolean ---- */
    {
        const uint8 f[8] = { 0x34u, 0x12u, 0xABu, 0xCDu, 0x3Du, 0u, 0u, 0u };   /* byte4: 0x1D = -3 in 5 bit, bit5 = 1 */
        rx(0u, f, 8u);
        CHECK_EQ(Com_ReceiveSignal(S_LE16, &u16), E_OK);  CHECK_EQ(u16, 0x1234);
        CHECK_EQ(Com_ReceiveSignal(S_BE16, &u16), E_OK);  CHECK_EQ(u16, 0xABCD);
        CHECK_EQ(Com_ReceiveSignal(S_S8, &s8), E_OK);     CHECK_EQ(s8, -3);
        CHECK_EQ(Com_ReceiveSignal(S_BOOL, &u8), E_OK);   CHECK_EQ(u8, 1);
    }
    /* DEFERRED: no notification in the "ISR", exactly one in MainFunctionRx; second call: nothing pending */
    CHECK_EQ(n_deferred, 0);
    Com_MainFunctionRx();
    CHECK_EQ(n_deferred, 1);
    CHECK(t_traceCount("COM NOTIFY sig=0") == 1);
    Com_MainFunctionRx();
    CHECK_EQ(n_deferred, 1);

    /* IMMEDIATE: notification inside Com_RxIndication */
    {
        const uint8 f[1] = { 60u };
        rx(1u, f, 1u);
        CHECK_EQ(n_immediate, 1);
        CHECK_EQ(Com_ReceiveSignal(S_IMM, &u8), E_OK);
        CHECK_EQ(u8, 60);
    }
    /* too short -> runtime Det error, buffer untouched */
    {
        const uint8 f[1] = { 0x99u };
        rx(0u, f, 1u);
        CHECK_EQ(t_detRuntime, 1);
        CHECK_EQ(Com_ReceiveSignal(S_LE16, &u16), E_OK);  CHECK_EQ(u16, 0x1234);
    }
    /* wrong direction / bad id -> Det error */
    {
        int before = t_detErrors;
        const uint8 f[2] = { 1u, 2u };
        rx(2u, f, 2u);            /* ipdu 2 is a TX I-PDU */
        rx(40u, f, 2u);           /* out of range */
        CHECK_EQ(t_detErrors, before + 2);
    }

    /* ---- TX pack: unaligned 32 bit value crossing 5 bytes ---- */
    u32 = 0xDEADBEEFu;
    CHECK_EQ(Com_SendSignal(S_U32, &u32), E_OK);          /* PENDING: stored, no transmission */
    CHECK_EQ(m_txCalls, 0);
    CHECK_EQ(buf3[0], 0xF0); CHECK_EQ(buf3[1], 0xEE); CHECK_EQ(buf3[2], 0xDB);
    CHECK_EQ(buf3[3], 0xEA); CHECK_EQ(buf3[4], 0x0D);
    u32 = 0u;
    CHECK_EQ(Com_ReceiveSignal(S_U32, &u32), E_OK);
    CHECK(u32 == 0xDEADBEEFu);
    CHECK_EQ(buf3[5], 0x34);                              /* neighbour signal unharmed */

    /* ---- DIRECT: TRIGGERED signal transmits at once, PENDING signal does not ---- */
    u8 = 0x5Au;
    CHECK_EQ(Com_SendSignal(S_PEND, &u8), E_OK);
    CHECK_EQ(m_txCalls, 0);
    u8 = 0xA5u;
    CHECK_EQ(Com_SendSignal(S_TRIG, &u8), E_OK);
    CHECK_EQ(m_txCalls, 1);
    CHECK_EQ(m_lastPduR, 5);                              /* pdurPduId of ipdu 2 */
    CHECK_EQ(m_lastLen, 2);
    CHECK_EQ(m_lastData[0], 0xA5); CHECK_EQ(m_lastData[1], 0x5A);
    CHECK(t_traceCount("COM TX ipdu=2 len=2") == 1);
    m_txResult = E_NOT_OK;
    CHECK_EQ(Com_SendSignal(S_TRIG, &u8), COM_BUSY);
    m_txResult = E_OK;
    m_txCalls = 0;

    /* ---- PERIODIC (20 ms) with a 10 ms main function: frames in calls 1,3,5 ---- */
    {
        int i, sent = 0, pattern = 0;
        for (i = 1; i <= 5; i++) {
            int before = m_txCalls;
            Com_MainFunctionTx();
            if (m_txCalls > before) { sent++; pattern |= (1 << i); CHECK_EQ(m_lastPduR, 6); CHECK_EQ(m_lastLen, 8); }
        }
        CHECK_EQ(sent, 3);
        CHECK_EQ(pattern, (1 << 1) | (1 << 3) | (1 << 5));
        CHECK_EQ(m_lastData[5], 0x34);
    }
    /* transmit failure: retried on the next call */
    {
        int before;
        Com_MainFunctionTx();                             /* call 6: nothing (countdown) */
        m_txResult = E_NOT_OK;
        before = m_txCalls;
        Com_MainFunctionTx();                             /* call 7: due, fails */
        CHECK_EQ(m_txCalls, before + 1);
        m_txResult = E_OK;
        Com_MainFunctionTx();                             /* call 8: retry succeeds */
        CHECK_EQ(m_txCalls, before + 2);
    }

    /* ---- Com_TriggerTransmit ---- */
    {
        uint8 d[8]; PduInfoType pi; pi.SduDataPtr = d; pi.MetaDataPtr = NULL_PTR; pi.SduLength = 8u;
        CHECK_EQ(Com_TriggerTransmit(3u, &pi), E_OK);
        CHECK_EQ(pi.SduLength, 8);
        CHECK_EQ(d[0], 0xF0);
        pi.SduLength = 4u;
        CHECK_EQ(Com_TriggerTransmit(3u, &pi), E_NOT_OK);
    }
    Com_TxConfirmation(3u, E_OK);

    /* ---- group stop blocks everything; restart with initialize reloads init values ---- */
    Com_IpduGroupStop(0u);
    CHECK_EQ(Com_SendSignal(S_TRIG, &u8), COM_SERVICE_NOT_AVAILABLE);
    CHECK_EQ(Com_ReceiveSignal(S_LE16, &u16), COM_SERVICE_NOT_AVAILABLE);
    {
        int before = m_txCalls;
        Com_MainFunctionTx();
        CHECK_EQ(m_txCalls, before);
    }
    Com_IpduGroupStart(0u, TRUE);
    CHECK_EQ(Com_ReceiveSignal(S_IMM, &u8), E_OK);  CHECK_EQ(u8, 255);
    CHECK_EQ(Com_ReceiveSignal(S_LE16, &u16), E_OK); CHECK_EQ(u16, 0);

    /* ---- parameter checks ---- */
    {
        int before = t_detErrors;
        CHECK_EQ(Com_SendSignal(99u, &u8), COM_SERVICE_NOT_AVAILABLE);
        CHECK_EQ(Com_ReceiveSignal(S_LE16, NULL_PTR), COM_SERVICE_NOT_AVAILABLE);
        CHECK_EQ(Com_SendSignal(S_LE16, &u16), COM_SERVICE_NOT_AVAILABLE);   /* RX signal cannot be sent */
        CHECK_EQ(t_detErrors, before + 3);
    }
    CHECK_EQ(t_osLockDepth, 0);                           /* every exclusive area was left */
    Com_DeInit();
    CHECK(Com_GetStatus() == COM_UNINIT);
    TEST_DONE();
}
