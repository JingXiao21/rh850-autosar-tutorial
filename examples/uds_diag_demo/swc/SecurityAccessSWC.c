/*
 * SecurityAccessSWC.c
 *
 * [Educational Implementation] demo seed/key SW-C.
 * Real AUTOSAR counterpart: the provider of SecurityAccess_<Level>
 * (GetSeed / CompareKey, DCM SWS R20-11 SWS_Dcm_00685) - in production
 * usually an OEM-supplied library, often backed by an HSM via Csm.
 *
 * !!! INSECURE TEACHING ALGORITHM !!!
 *   seed : 4 bytes from a linear congruential generator (predictable)
 *   key  : key[i] = seed[i] XOR {0x5A, 0x3C, 0x96, 0xE1}[i]
 * Anyone who sees one seed/key pair can compute every future key. Real ECUs
 * use OEM-secret algorithms (often AES-CMAC based) and true random seeds.
 * The Dcm side (attempt counter, delay, NRC 0x35/0x36/0x37) is independent
 * of the algorithm and is what this demo is about.
 */
#include "Rte_SecurityAccessSWC.h"
#include "UdsTrace.h"

#define SECACC_SEED_SIZE 4u

static const uint8 SecurityAccessSWC_KeyMask[SECACC_SEED_SIZE] = { 0x5Au, 0x3Cu, 0x96u, 0xE1u };
static uint32 SecurityAccessSWC_Lcg = 0x1234ABCDu;
static uint8  SecurityAccessSWC_LastSeed[SECACC_SEED_SIZE];

void SecurityAccessSWC_Init(void)
{
    uint8 i;
    for (i = 0u; i < SECACC_SEED_SIZE; i++) {
        SecurityAccessSWC_LastSeed[i] = 0u;
    }
}

Std_ReturnType SecurityAccessSWC_GetSeed_Level01(Dcm_OpStatusType OpStatus, uint8 *Seed,
                                                 Dcm_NegativeResponseCodeType *ErrorCode)
{
    uint8 i;
    (void)ErrorCode;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    SecurityAccessSWC_Lcg = (SecurityAccessSWC_Lcg * 1103515245u) + 12345u;
    for (i = 0u; i < SECACC_SEED_SIZE; i++) {
        Seed[i] = (uint8)(SecurityAccessSWC_Lcg >> (24u - (8u * i)));
    }
    if ((Seed[0] | Seed[1] | Seed[2] | Seed[3]) == 0u) {
        Seed[3] = 1u;   /* an all-zero seed means "already unlocked" in UDS */
    }
    for (i = 0u; i < SECACC_SEED_SIZE; i++) {
        SecurityAccessSWC_LastSeed[i] = Seed[i];
    }
    UDS_TRACE("SWC", "SecurityAccessSWC_GetSeed_Level01: seed %s", UdsTrace_Hex(Seed, SECACC_SEED_SIZE));
    return E_OK;
}

Std_ReturnType SecurityAccessSWC_CompareKey_Level01(const uint8 *Key, Dcm_OpStatusType OpStatus,
                                                    Dcm_NegativeResponseCodeType *ErrorCode)
{
    uint8 i;
    boolean ok = TRUE;
    (void)ErrorCode;
    if (OpStatus == DCM_CANCEL) {
        return E_OK;
    }
    for (i = 0u; i < SECACC_SEED_SIZE; i++) {
        if (Key[i] != (uint8)(SecurityAccessSWC_LastSeed[i] ^ SecurityAccessSWC_KeyMask[i])) {
            ok = FALSE;
        }
    }
    UDS_TRACE("SWC", "SecurityAccessSWC_CompareKey_Level01: key %s -> %s", UdsTrace_Hex(Key, SECACC_SEED_SIZE),
              ok ? "E_OK" : "DCM_E_COMPARE_KEY_FAILED");
    return ok ? E_OK : DCM_E_COMPARE_KEY_FAILED;
}
