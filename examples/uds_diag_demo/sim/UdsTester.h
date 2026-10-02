/*
 * UdsTester.h
 *
 * [Educational Implementation] PC-side diagnostic tester.
 * Real counterpart: a tester tool (CANoe.DiVa, CANalyzer, ODIS, python-udsoncan
 * + python-can-isotp) running its own ISO 15765-2 stack on the PC. It is
 * deliberately independent from the ECU-side CanTp so that the two
 * implementations check each other (FF/FC/CF exchange in both directions).
 */
#ifndef UDS_TESTER_H
#define UDS_TESTER_H

#include "Std_Types.h"

#define UDS_TESTER_MAX_MSG      256u
#define UDS_TESTER_MAX_RESPONSES 16u

typedef struct {
    uint32 physReqId;   /* 0x7E0 */
    uint32 funcReqId;   /* 0x7DF */
    uint32 respId;      /* 0x7E8 */
    uint8  bs;          /* block size the tester announces in its FC     */
    uint8  stMin;       /* STmin the tester announces in its FC (ms)      */
    uint8  padding;
} UdsTester_ConfigType;

typedef struct {
    uint16 length;
    uint8  data[UDS_TESTER_MAX_MSG];
    uint32 timeMs;
} UdsTester_ResponseType;

void UdsTester_Init(const UdsTester_ConfigType *cfg);
Std_ReturnType UdsTester_SendRequest(const uint8 *data, uint16 length, boolean functional);
void UdsTester_MainFunction(void);   /* call once per simulated ms */
boolean UdsTester_GetResponse(UdsTester_ResponseType *resp);
boolean UdsTester_IsTxBusy(void);
uint32 UdsTester_GetFcSentCount(void);
uint32 UdsTester_GetFcReceivedCount(void);

#endif /* UDS_TESTER_H */
