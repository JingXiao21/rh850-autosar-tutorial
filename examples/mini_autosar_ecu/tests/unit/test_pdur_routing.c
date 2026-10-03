// SOURCES: bsw/pdur/PduR.c
// [Educational Implementation] Host unit test of PduR: 1:1 IF routing tables (Com <-> CanIf), state handling and
// parameter checks. Com, CanIf and Det are mocked.
#define TEST_MOCK_DET
#include "test_support.h"
#include "PduR.h"
#include "Com.h"
#include "CanIf.h"

/* ---- mocks of the neighbours ---- */
static PduIdType canifTxId, comRxId, comTxConfId;
static int       canifTxCalls, comRxCalls, comTxConfCalls;
static Std_ReturnType comTxConfResult, canifResult = E_OK;
static uint8     seenByte0;
Std_ReturnType CanIf_Transmit(PduIdType id, const PduInfoType *p)
{ canifTxCalls++; canifTxId = id; seenByte0 = p->SduDataPtr[0]; return canifResult; }
void Com_RxIndication(PduIdType id, const PduInfoType *p)
{ comRxCalls++; comRxId = id; seenByte0 = p->SduDataPtr[0]; }
void Com_TxConfirmation(PduIdType id, Std_ReturnType r)
{ comTxConfCalls++; comTxConfId = id; comTxConfResult = r; }

/* ---- routing tables: deliberately NOT identity mappings so that wrong indexing is detected ---- */
static const PduR_TxPathType txPaths[2] = { { 7u, 5u }, { 8u, 2u } };      /* { comTxPduId, canIfTxPduId } */
static const PduR_RxPathType rxPaths[3] = { { 11u }, { 4u }, { 9u } };     /* { comRxPduId } */
const PduR_PBConfigType PduR_Config = { 2u, txPaths, 3u, rxPaths };

int main(void)
{
    uint8 d[2] = { 0x42u, 0x43u };
    PduInfoType pi;
    pi.SduDataPtr = d; pi.MetaDataPtr = NULL_PTR; pi.SduLength = 2u;

    /* before PduR_Init: refuse + Det */
    CHECK(PduR_GetState() == PDUR_UNINIT);
    CHECK_EQ(PduR_ComTransmit(0u, &pi), E_NOT_OK);
    PduR_CanIfRxIndication(0u, &pi);
    PduR_CanIfTxConfirmation(0u, E_OK);
    CHECK_EQ(t_detErrors, 3);
    CHECK_EQ(canifTxCalls + comRxCalls + comTxConfCalls, 0);

    PduR_Init(NULL_PTR);
    CHECK_EQ(t_detErrors, 4);
    CHECK(PduR_GetState() == PDUR_UNINIT);
    PduR_Init(&PduR_Config);
    CHECK(PduR_GetState() == PDUR_ONLINE);

    /* TX: Com handle (= path index) -> CanIf tx pdu */
    CHECK_EQ(PduR_ComTransmit(1u, &pi), E_OK);
    CHECK_EQ(canifTxCalls, 1);
    CHECK_EQ(canifTxId, 2);
    CHECK_EQ(seenByte0, 0x42);
    CHECK(t_traceCount("PDUR TX com=8 canif=2") == 1);
    canifResult = E_NOT_OK;
    CHECK_EQ(PduR_ComTransmit(0u, &pi), E_NOT_OK);      /* lower layer result is passed up */
    CHECK_EQ(canifTxId, 5);
    canifResult = E_OK;

    /* RX: CanIf rx path index -> Com I-PDU handle */
    d[0] = 0x77u;
    PduR_CanIfRxIndication(2u, &pi);
    CHECK_EQ(comRxCalls, 1);
    CHECK_EQ(comRxId, 9);
    CHECK_EQ(seenByte0, 0x77);
    CHECK(t_traceCount("PDUR RX canif=2 com=9") == 1);

    /* TX confirmation: tx path index -> Com I-PDU handle */
    PduR_CanIfTxConfirmation(0u, E_OK);
    CHECK_EQ(comTxConfCalls, 1);
    CHECK_EQ(comTxConfId, 7);
    CHECK_EQ(comTxConfResult, E_OK);
    PduR_CanIfTxConfirmation(1u, E_NOT_OK);
    CHECK_EQ(comTxConfId, 8);
    CHECK_EQ(comTxConfResult, E_NOT_OK);

    /* invalid ids / null pointers */
    {
        int before = t_detErrors;
        CHECK_EQ(PduR_ComTransmit(2u, &pi), E_NOT_OK);
        CHECK_EQ(PduR_ComTransmit(0u, NULL_PTR), E_NOT_OK);
        PduR_CanIfRxIndication(3u, &pi);
        PduR_CanIfRxIndication(0u, NULL_PTR);
        PduR_CanIfTxConfirmation(2u, E_OK);
        CHECK_EQ(t_detErrors, before + 5);
        CHECK_EQ(canifTxCalls, 2);                      /* unchanged: nothing forwarded */
        CHECK_EQ(comRxCalls, 1);
        CHECK_EQ(comTxConfCalls, 2);
    }
    {
        Std_VersionInfoType v;
        PduR_GetVersionInfo(&v);
        CHECK_EQ(v.moduleID, MINI_MODULE_PDUR);
    }
    TEST_DONE();
}
