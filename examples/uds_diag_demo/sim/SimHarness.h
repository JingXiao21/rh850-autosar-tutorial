/*
 * SimHarness.h
 *
 * [Educational Implementation] glue between the simulated ECU and the PC
 * tester for main_demo.c and the host tests. No AUTOSAR counterpart: in the
 * lab this is "power on the ECU, open CANoe, send a request, wait".
 */
#ifndef SIM_HARNESS_H
#define SIM_HARNESS_H

#include "Std_Types.h"
#include "UdsTester.h"

typedef struct {
    boolean received;              /* a final (non-0x78) response arrived      */
    uint16  length;
    uint8   data[UDS_TESTER_MAX_MSG];
    uint8   numResponsePending;    /* how many 7F xx 78 came before it          */
    uint32  elapsedMs;             /* request sent -> final response received   */
} Sim_ResultType;

/* Reset simulated time, bus and ECU (EcuM_Init), tester with BS/STmin. */
void Sim_PowerOn(uint8 testerBs, uint8 testerStMin);
void Sim_RunMs(uint32 ms);
/* Send one request and run the simulation until the final response or timeout. */
boolean Sim_Request(const uint8 *req, uint16 length, boolean functional, uint32 timeoutMs, Sim_ResultType *out);

#endif /* SIM_HARNESS_H */
