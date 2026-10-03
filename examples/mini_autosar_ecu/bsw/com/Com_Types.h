/*
 * Com_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Com_Types.h + the ECUC containers ComConfig / ComSignal / ComIPdu /
 * ComIPduGroup / ComTxMode (AUTOSAR_CP_SWS_COM, chapter 10 configuration). THIS is the contract
 * between Com.c (agent C) and the generated Com_Cfg.c (agent D).
 *
 * Handle spaces: Com_SignalIdType = index in Com_Config.signals; Com I-PDU handle = index in
 * Com_Config.ipdus (tx and rx I-PDUs share the table; Com_RxIndication/Com_TxConfirmation/
 * Com_TriggerTransmit take this handle).
 */
#ifndef COM_TYPES_H
#define COM_TYPES_H

#include "ComStack_Types.h"

typedef uint16 Com_SignalIdType;
typedef uint16 Com_SignalGroupIdType;     /* signal groups are not implemented; type kept for API shape */
typedef uint8  Com_IpduGroupIdType;       /* 0..7, one bit in the 8-bit group mask of an I-PDU */

typedef enum { COM_UNINIT = 0, COM_INIT } Com_StatusType;

typedef enum {                            /* ComSignalType */
    COM_BOOLEAN = 0, COM_UINT8, COM_UINT16, COM_UINT32, COM_SINT8, COM_SINT16, COM_SINT32
} Com_SignalTypeType;

typedef enum { COM_LITTLE_ENDIAN = 0, COM_BIG_ENDIAN } Com_EndiannessType;      /* ComSignalEndianness (opaque not supported) */
typedef enum { COM_PDU_TX = 0, COM_PDU_RX } Com_PduDirectionType;               /* ComIPduDirection */
typedef enum { COM_TX_MODE_NONE = 0, COM_TX_MODE_DIRECT, COM_TX_MODE_PERIODIC, COM_TX_MODE_MIXED } Com_TxModeType;
typedef enum { COM_RX_IMMEDIATE = 0, COM_RX_DEFERRED } Com_RxProcessingType;    /* ComIPduSignalProcessing */
typedef enum { COM_PENDING = 0, COM_TRIGGERED } Com_TransferPropertyType;       /* ComTransferProperty */

/* Return values of Com_SendSignal / Com_ReceiveSignal (uint8, E_OK = 0) */
#define COM_SERVICE_NOT_AVAILABLE  ((uint8)0x80u)   /* I-PDU group stopped */
#define COM_BUSY                   ((uint8)0x81u)   /* could not trigger transmission now */

typedef struct {
    uint16                    bitPosition;     /* ComBitPosition, LSB of the signal (little endian) in the PDU */
    uint8                     bitSize;         /* ComBitSize 1..32 */
    Com_SignalTypeType        type;
    Com_EndiannessType        endianness;
    PduIdType                 ipduId;          /* ComIPduSignalRef: index in Com_Config.ipdus */
    uint32                    initValue;       /* ComSignalInitValue (raw bits) */
    Com_TransferPropertyType  transferProperty;/* TRIGGERED: Com_SendSignal sends at once if the PDU is DIRECT/MIXED */
    void                    (*rxNotification)(void);  /* ComNotification: called after reception (immediate: from
                                                  ISR context; deferred: from Com_MainFunctionRx) or NULL_PTR */
} Com_SignalConfigType;

typedef struct {
    Com_PduDirectionType      direction;
    PduLengthType             length;          /* ComPduIdRef length in bytes (1..8) */
    uint8                    *buffer;          /* generated static array of `length` bytes (shadow copy: Com.c) */
    PduIdType                 pdurPduId;       /* TX: handle passed to PduR_ComTransmit; RX: unused */
    Com_IpduGroupIdType       group;           /* ComIPduGroupRef (single group per I-PDU) */
    /* TX only */
    Com_TxModeType            txMode;
    uint16                    txPeriodMs;      /* ComTxModeTimePeriod (PERIODIC/MIXED) in ms; multiple of MainFunctionTx period */
    uint16                    txOffsetMs;      /* ComTxModeTimeOffset, first transmission after group start */
    /* RX only */
    Com_RxProcessingType      rxProcessing;
} Com_IpduConfigType;

typedef struct {
    uint16                       numSignals;
    const Com_SignalConfigType  *signals;
    uint16                       numIpdus;
    const Com_IpduConfigType    *ipdus;
    uint16                       mainFunctionPeriodMs;   /* period at which MainFunctionTx/Rx are called (10) */
} Com_ConfigType;
extern const Com_ConfigType Com_Config;          /* generated Com_Cfg.c */

#endif /* COM_TYPES_H */
