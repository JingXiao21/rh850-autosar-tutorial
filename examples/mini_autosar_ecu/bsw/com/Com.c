/*
 * Com.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: COM (AUTOSAR_CP_SWS_COM). Implemented subset (see Com.h / Com_Types.h):
 *   Com_Init / Com_DeInit, Com_IpduGroupStart 8.3.2.3 / Com_IpduGroupStop,
 *   Com_SendSignal 8.3.3.1, Com_ReceiveSignal 8.3.3.3, Com_TriggerIPDUSend 8.3.3.19,
 *   Com_TriggerTransmit 8.4.1, Com_RxIndication 8.4.2, Com_TxConfirmation 8.4.4,
 *   Com_MainFunctionRx 8.5.1, Com_MainFunctionTx 8.5.2.
 * Signal packing: ComBitPosition = LSB position (little endian, "Intel") or MSB position in the sawtooth bit
 * numbering (big endian, "Motorola"); 1..32 bits, unsigned/signed (sign extension on unpack).
 * TX modes: PERIODIC (cyclic in Com_MainFunctionTx), DIRECT (Com_SendSignal of a TRIGGERED signal sends at once),
 * MIXED (both). RX: IMMEDIATE (notification called in the RxIndication context = Cat2 ISR) or DEFERRED
 * (flag set in the ISR, notification called from Com_MainFunctionRx in task context).
 * Not implemented: signal groups, update bits, filters, gateway, deadline monitoring, TX repetitions.
 *
 * Context rules: Com_RxIndication may run in ISR context -> only memcpy under the Com exclusive area, trace, and
 * (IMMEDIATE only) the notification. The shadow buffers (Com_IpduConfigType.buffer) are shared between tasks
 * and the ISR, so every access is inside SchM_Enter_Com_COM_EXCLUSIVE_AREA_0. The lock is NEVER held while calling
 * out (PduR, notifications): the I-PDU is copied to a stack buffer first.
 */
#include <string.h>
#include "Com.h"
#include "PduR.h"
#include "SchM.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_Cfg.h"
#include "Mini_ModuleIds.h"

/* API ids for Det (values of the Com SWS service ids where they exist) */
#define COM_API_INIT            0x01u
#define COM_API_IPDU_GROUP      0x03u
#define COM_API_SEND_SIGNAL     0x0Au
#define COM_API_RECEIVE_SIGNAL  0x0Bu
#define COM_API_TRIGGER_IPDU    0x17u
#define COM_API_RX_INDICATION   0x42u
#define COM_API_TX_CONFIRM      0x40u
#define COM_API_TRIGGER_TX      0x41u
#define COM_MAX_PDU_BYTES       8u

static const Com_ConfigType *com_cfg;
static Com_StatusType        com_status = COM_UNINIT;
static uint8                 com_groupMask;                       /* bit g = I-PDU group g started */
static uint16                com_txCountdownMs[COM_NUM_IPDUS];    /* ms until next cyclic transmission */
static boolean               com_rxPending[COM_NUM_IPDUS];        /* DEFERRED: indication not yet processed */

/* ------------------------------------------------------------------ bit packing */

/* Write the low `bits` bits of `value` into buf. LE: signal LSB at position pos, walking up.
 * BE: pos = signal MSB (sawtooth numbering: byte*8 + bit), walking down inside a byte, then to the next byte. */
static void com_packBits(uint8 *buf, uint16 pos, uint8 bits, Com_EndiannessType e, uint32 value)
{
    uint8 i;
    if (e == COM_LITTLE_ENDIAN) {
        for (i = 0u; i < bits; i++) {                         /* bit i of the value -> position pos+i */
            uint16 p = (uint16)(pos + i);
            uint8  mask = (uint8)(1u << (p & 7u));
            if (((value >> i) & 1u) != 0u) { buf[p >> 3] = (uint8)(buf[p >> 3] | mask); }
            else                           { buf[p >> 3] = (uint8)(buf[p >> 3] & (uint8)~mask); }
        }
    } else {
        uint16 byte = (uint16)(pos >> 3);
        int    bit  = (int)(pos & 7u);
        for (i = bits; i > 0u; i--) {                         /* MSB first: bit (bits-1) ... bit 0 */
            uint8 mask = (uint8)(1u << bit);
            if (((value >> (i - 1u)) & 1u) != 0u) { buf[byte] = (uint8)(buf[byte] | mask); }
            else                                  { buf[byte] = (uint8)(buf[byte] & (uint8)~mask); }
            if (bit == 0) { bit = 7; byte++; } else { bit--; }
        }
    }
}

static uint32 com_unpackBits(const uint8 *buf, uint16 pos, uint8 bits, Com_EndiannessType e)
{
    uint32 v = 0u;
    uint8  i;
    if (e == COM_LITTLE_ENDIAN) {
        for (i = 0u; i < bits; i++) {
            uint16 p = (uint16)(pos + i);
            if ((buf[p >> 3] & (uint8)(1u << (p & 7u))) != 0u) { v |= (1u << i); }
        }
    } else {
        uint16 byte = (uint16)(pos >> 3);
        int    bit  = (int)(pos & 7u);
        for (i = bits; i > 0u; i--) {
            if ((buf[byte] & (uint8)(1u << bit)) != 0u) { v |= (1u << (i - 1u)); }
            if (bit == 0) { bit = 7; byte++; } else { bit--; }
        }
    }
    return v;
}

static uint8 com_typeBytes(Com_SignalTypeType t)
{
    switch (t) {
    case COM_UINT16: case COM_SINT16: return 2u;
    case COM_UINT32: case COM_SINT32: return 4u;
    default:                          return 1u;              /* BOOLEAN, UINT8, SINT8 */
    }
}

static boolean com_isSigned(Com_SignalTypeType t)
{
    return (boolean)((t == COM_SINT8) || (t == COM_SINT16) || (t == COM_SINT32));
}

/* application value (native C type of the signal) -> raw bits (caller masks to the signal width) */
static uint32 com_loadValue(Com_SignalTypeType t, const void *p)
{
    uint8 n = com_typeBytes(t);
    if (n == 1u) { uint8 v;  (void)memcpy(&v, p, 1u); return (uint32)v; }
    if (n == 2u) { uint16 v; (void)memcpy(&v, p, 2u); return (uint32)v; }
    { uint32 v; (void)memcpy(&v, p, 4u); return v; }
}

static void com_storeValue(Com_SignalTypeType t, void *p, uint32 raw)
{
    uint8 n = com_typeBytes(t);
    if (n == 1u)      { uint8 v = (uint8)raw;   (void)memcpy(p, &v, 1u); }
    else if (n == 2u) { uint16 v = (uint16)raw; (void)memcpy(p, &v, 2u); }
    else              { (void)memcpy(p, &raw, 4u); }
}

static uint32 com_bitMask(uint8 bits)
{
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

/* ------------------------------------------------------------------ helpers */

static boolean com_groupActive(Com_IpduGroupIdType g)
{
    return (boolean)((com_groupMask & (uint8)(1u << g)) != 0u);
}

/* (re)load init values of all signals of one I-PDU (buffer cleared first) */
static void com_initIpdu(PduIdType ipdu)
{
    const Com_IpduConfigType *ip = &com_cfg->ipdus[ipdu];
    uint16 s;
    PduLengthType b;
    for (b = 0u; b < ip->length; b++) { ip->buffer[b] = 0u; }
    for (s = 0u; s < com_cfg->numSignals; s++) {
        const Com_SignalConfigType *sg = &com_cfg->signals[s];
        if (sg->ipduId == ipdu) {
            com_packBits(ip->buffer, sg->bitPosition, sg->bitSize, sg->endianness, sg->initValue);
        }
    }
}

/* copy the shadow buffer and hand it to PduR (never called with the lock held) */
static Std_ReturnType com_transmit(PduIdType ipdu)
{
    const Com_IpduConfigType *ip = &com_cfg->ipdus[ipdu];
    uint8       tmp[COM_MAX_PDU_BYTES];
    PduInfoType info;
    PduLengthType i;

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    for (i = 0u; (i < ip->length) && (i < COM_MAX_PDU_BYTES); i++) { tmp[i] = ip->buffer[i]; }
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

    info.SduDataPtr  = tmp;
    info.MetaDataPtr = NULL_PTR;
    info.SduLength   = ip->length;
    TRACE(TRACE_CAT_COM, "TX ipdu=%u len=%u", (unsigned)ipdu, (unsigned)ip->length);
    return PduR_ComTransmit(ip->pdurPduId, &info);
}

/* call notifications of all signals of one I-PDU */
static void com_notify(PduIdType ipdu)
{
    uint16 s;
    for (s = 0u; s < com_cfg->numSignals; s++) {
        const Com_SignalConfigType *sg = &com_cfg->signals[s];
        if ((sg->ipduId == ipdu) && (sg->rxNotification != NULL_PTR)) {
            TRACE(TRACE_CAT_COM, "NOTIFY sig=%u", (unsigned)s);
            sg->rxNotification();
        }
    }
}

/* ------------------------------------------------------------------ API */

void Com_Init(const Com_ConfigType *config)
{
    PduIdType i;
    if (config == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_INIT, MINI_E_PARAM_POINTER);
        return;
    }
    com_cfg = config;
    com_groupMask = 0u;                                       /* all I-PDU groups stopped */
    for (i = 0u; (i < config->numIpdus) && (i < COM_NUM_IPDUS); i++) {
        com_initIpdu(i);
        com_txCountdownMs[i] = 0u;
        com_rxPending[i] = FALSE;
    }
    com_status = COM_INIT;
}

void Com_DeInit(void)
{
    com_groupMask = 0u;
    com_status = COM_UNINIT;
}

void Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize)
{
    PduIdType i;
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_IPDU_GROUP, MINI_E_UNINIT);
        return;
    }
    if (IpduGroupId > 7u) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_IPDU_GROUP, MINI_E_PARAM_INVALID);
        return;
    }
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    for (i = 0u; (i < com_cfg->numIpdus) && (i < COM_NUM_IPDUS); i++) {
        const Com_IpduConfigType *ip = &com_cfg->ipdus[i];
        if (ip->group == IpduGroupId) {
            if (initialize) { com_initIpdu(i); }
            com_txCountdownMs[i] = ip->txOffsetMs;            /* first cyclic frame after the offset */
            com_rxPending[i] = FALSE;
        }
    }
    com_groupMask = (uint8)(com_groupMask | (uint8)(1u << IpduGroupId));
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    TRACE(TRACE_CAT_COM, "GROUP_START %u", (unsigned)IpduGroupId);
}

void Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId)
{
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_IPDU_GROUP, MINI_E_UNINIT);
        return;
    }
    if (IpduGroupId > 7u) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_IPDU_GROUP, MINI_E_PARAM_INVALID);
        return;
    }
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    com_groupMask = (uint8)(com_groupMask & (uint8)~(1u << IpduGroupId));
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
}

uint8 Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr)
{
    const Com_SignalConfigType *sg;
    const Com_IpduConfigType   *ip;
    uint32 raw;
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_SEND_SIGNAL, MINI_E_UNINIT);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (SignalId >= com_cfg->numSignals) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_SEND_SIGNAL, MINI_E_PARAM_INVALID);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (SignalDataPtr == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_SEND_SIGNAL, MINI_E_PARAM_POINTER);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    sg = &com_cfg->signals[SignalId];
    ip = &com_cfg->ipdus[sg->ipduId];
    if (ip->direction != COM_PDU_TX) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_SEND_SIGNAL, MINI_E_PARAM_INVALID);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (!com_groupActive(ip->group)) {
        return COM_SERVICE_NOT_AVAILABLE;                     /* I-PDU group stopped */
    }
    raw = com_loadValue(sg->type, SignalDataPtr) & com_bitMask(sg->bitSize);
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    com_packBits(ip->buffer, sg->bitPosition, sg->bitSize, sg->endianness, raw);
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

    if ((sg->transferProperty == COM_TRIGGERED) &&
        ((ip->txMode == COM_TX_MODE_DIRECT) || (ip->txMode == COM_TX_MODE_MIXED))) {
        if (com_transmit(sg->ipduId) != E_OK) {
            return COM_BUSY;                                  /* value is stored; a cyclic frame (if any) carries it */
        }
    }
    return E_OK;
}

uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr)
{
    const Com_SignalConfigType *sg;
    const Com_IpduConfigType   *ip;
    uint32 raw;
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, MINI_E_UNINIT);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (SignalId >= com_cfg->numSignals) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, MINI_E_PARAM_INVALID);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (SignalDataPtr == NULL_PTR) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, MINI_E_PARAM_POINTER);
        return COM_SERVICE_NOT_AVAILABLE;
    }
    sg = &com_cfg->signals[SignalId];
    ip = &com_cfg->ipdus[sg->ipduId];
    if (!com_groupActive(ip->group)) {
        return COM_SERVICE_NOT_AVAILABLE;
    }
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    raw = com_unpackBits(ip->buffer, sg->bitPosition, sg->bitSize, sg->endianness);
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    if (com_isSigned(sg->type) && (sg->bitSize < 32u) && ((raw & (1u << (sg->bitSize - 1u))) != 0u)) {
        raw |= ~com_bitMask(sg->bitSize);                     /* sign extension */
    }
    com_storeValue(sg->type, SignalDataPtr, raw);
    return E_OK;
}

Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TRIGGER_IPDU, MINI_E_UNINIT);
        return E_NOT_OK;
    }
    if ((PduId >= com_cfg->numIpdus) || (com_cfg->ipdus[PduId].direction != COM_PDU_TX)) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TRIGGER_IPDU, MINI_E_PARAM_INVALID);
        return E_NOT_OK;
    }
    if (!com_groupActive(com_cfg->ipdus[PduId].group)) {
        return E_NOT_OK;
    }
    return com_transmit(PduId);
}

Com_StatusType Com_GetStatus(void)
{
    return com_status;
}

void Com_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo != NULL_PTR) {
        versioninfo->vendorID = MINI_VENDOR_ID;
        versioninfo->moduleID = MINI_MODULE_COM;
        versioninfo->sw_major_version = 1u;
        versioninfo->sw_minor_version = 0u;
        versioninfo->sw_patch_version = 0u;
    }
}

/* ------------------------------------------------------------------ scheduled functions */

void Com_MainFunctionRx(void)
{
    PduIdType i;
    if (com_status != COM_INIT) { return; }
    for (i = 0u; (i < com_cfg->numIpdus) && (i < COM_NUM_IPDUS); i++) {
        boolean pending;
        SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();                /* the flag is written by the RX ISR */
        pending = com_rxPending[i];
        com_rxPending[i] = FALSE;
        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        if (pending) {
            com_notify(i);                                    /* task context: may call SetEvent etc. */
        }
    }
}

void Com_MainFunctionTx(void)
{
    PduIdType i;
    uint16 period;
    if (com_status != COM_INIT) { return; }
    period = com_cfg->mainFunctionPeriodMs;
    for (i = 0u; (i < com_cfg->numIpdus) && (i < COM_NUM_IPDUS); i++) {
        const Com_IpduConfigType *ip = &com_cfg->ipdus[i];
        boolean periodic = (boolean)((ip->direction == COM_PDU_TX) &&
                                     ((ip->txMode == COM_TX_MODE_PERIODIC) || (ip->txMode == COM_TX_MODE_MIXED)));
        if (!periodic || !com_groupActive(ip->group)) { continue; }
        /* countdown model: send when it reached 0, reload, then age by one main-function period */
        if (com_txCountdownMs[i] == 0u) {
            if (com_transmit(i) == E_OK) {
                com_txCountdownMs[i] = ip->txPeriodMs;
            }                                                 /* else: stays 0 -> retried next cycle */
        }
        com_txCountdownMs[i] = (com_txCountdownMs[i] > period) ? (uint16)(com_txCountdownMs[i] - period) : 0u;
    }
}

/* ------------------------------------------------------------------ callbacks from PduR */

void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    const Com_IpduConfigType *ip;
    PduLengthType i;
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_RX_INDICATION, MINI_E_UNINIT);
        return;
    }
    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_RX_INDICATION, MINI_E_PARAM_POINTER);
        return;
    }
    if ((RxPduId >= com_cfg->numIpdus) || (com_cfg->ipdus[RxPduId].direction != COM_PDU_RX)) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_RX_INDICATION, MINI_E_PARAM_INVALID);
        return;
    }
    ip = &com_cfg->ipdus[RxPduId];
    if (!com_groupActive(ip->group)) {
        return;                                               /* stopped group: indication is dropped silently */
    }
    if (PduInfoPtr->SduLength < ip->length) {
        (void)Det_ReportRuntimeError(MINI_MODULE_COM, 0u, COM_API_RX_INDICATION, MINI_E_PARAM_INVALID);
        return;
    }
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    for (i = 0u; i < ip->length; i++) { ip->buffer[i] = PduInfoPtr->SduDataPtr[i]; }
    if (ip->rxProcessing == COM_RX_DEFERRED) { com_rxPending[RxPduId] = TRUE; }
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    TRACE(TRACE_CAT_COM, "RX ipdu=%u len=%u", (unsigned)RxPduId, (unsigned)ip->length);
    if (ip->rxProcessing == COM_RX_IMMEDIATE) {
        com_notify(RxPduId);                                  /* ISR context (Cat2): callbacks must be ISR-safe */
    }
}

void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)result;
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TX_CONFIRM, MINI_E_UNINIT);
        return;
    }
    if ((TxPduId >= com_cfg->numIpdus) || (com_cfg->ipdus[TxPduId].direction != COM_PDU_TX)) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TX_CONFIRM, MINI_E_PARAM_INVALID);
        return;
    }
    /* No tx notifications / deadline monitoring / repetitions in this subset: nothing to do. */
}

Std_ReturnType Com_TriggerTransmit(PduIdType TxPduId, PduInfoType *PduInfoPtr)
{
    const Com_IpduConfigType *ip;
    PduLengthType i;
    if (com_status != COM_INIT) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TRIGGER_TX, MINI_E_UNINIT);
        return E_NOT_OK;
    }
    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TRIGGER_TX, MINI_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if ((TxPduId >= com_cfg->numIpdus) || (com_cfg->ipdus[TxPduId].direction != COM_PDU_TX)) {
        (void)Det_ReportError(MINI_MODULE_COM, 0u, COM_API_TRIGGER_TX, MINI_E_PARAM_INVALID);
        return E_NOT_OK;
    }
    ip = &com_cfg->ipdus[TxPduId];
    if (PduInfoPtr->SduLength < ip->length) {
        return E_NOT_OK;
    }
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    for (i = 0u; i < ip->length; i++) { PduInfoPtr->SduDataPtr[i] = ip->buffer[i]; }
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    PduInfoPtr->SduLength = ip->length;
    return E_OK;
}
