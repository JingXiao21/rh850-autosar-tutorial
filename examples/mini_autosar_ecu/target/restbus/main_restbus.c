/*
 * main_restbus.c  (REST node image, TARGET ONLY, bare metal, no OS)
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart (a rest-bus simulator is test infrastructure). Third machine "REST" of the Renode scenario
 * (target/renode/mini_autosar_2ecu.resc): sends AmbientLight 0x301 / EcuModeReq 0x3F0 on the shared CAN bus
 * (RestBus_Step, see sim/restbus/RestBus.c) and prints every 0x101 / 0x201 frame it sees.
 * Uses the REAL MCAL drivers Mcu/Port/Can (polling RX), the Trace backend (USART1) and the 1 kHz SysTick of
 * Mini_Time_Target.c. The CAN driver upcalls (CanIf_Cbk.h names) are implemented here directly: there is no CanIf.
 * The MCAL and Trace sources expect a few OS services (interrupt locks) and Det; tiny bare-metal replacements
 * for them are at the bottom of this file (PRIMASK based, nesting counted).
 *
 * Main loop: once per millisecond (Mini_Time_GetMs changed) -> RestBus_Step + Can_MainFunction_Read/Write; then WFI
 * (SysTick wakes the core).
 */
#include "Mcu.h"
#include "Port.h"
#include "Can.h"
#include "CanIf_Cbk.h"
#include "RestBus.h"
#include "Trace.h"
#include "Mini_Time.h"
#include "Det.h"
#include "Os.h"

static uint8 restbus_txBuf[8];

static boolean restbus_send(uint32 id, uint8 dlc, const uint8 *data)
{
    Can_PduType pdu;
    uint8 i;
    for (i = 0u; (i < dlc) && (i < 8u); i++) { restbus_txBuf[i] = data[i]; }
    pdu.swPduHandle = 0u;
    pdu.length = dlc;
    pdu.id = id;
    pdu.sdu = restbus_txBuf;
    return (boolean)(Can_Write(CanConf_CanHardwareObject_Hth_RestBus, &pdu) == CAN_OK);
}

int main(void)
{
    uint32 lastMs = 0xFFFFFFFFu;

    Mcu_Init(&Mcu_Config);
    (void)Mcu_InitClock(McuConf_McuClockSettingConfig_0);
    (void)Mcu_DistributePllClock();
    Port_Init(&Port_Config);
    Trace_Init(NULL_PTR);
    Mini_Time_Init();                                /* SysTick 1 kHz */
    TRACE(TRACE_CAT_SIM, "REST node start");

    Can_Init(&Can_Config);
    (void)Can_SetControllerMode(0u, CAN_T_START);
    RestBus_Init(restbus_send, 0u);

    for (;;) {
        uint32 ms = Mini_Time_GetMs();
        if (ms != lastMs) {
            lastMs = ms;
            RestBus_Step(ms);
            Can_MainFunction_Read();
            Can_MainFunction_Write();
        }
        __asm__ volatile ("wfi");
    }
}

/* ---- CAN driver upcalls (normally CanIf) ------------------------------------------------------------------ */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
    RestBus_OnRx(Mailbox->CanId, (uint8)PduInfoPtr->SduLength, PduInfoPtr->SduDataPtr);
}
void CanIf_TxConfirmation(PduIdType CanTxPduId)       { (void)CanTxPduId; }
void CanIf_ControllerBusOff(uint8 ControllerId)       { (void)ControllerId; }
void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    (void)ControllerId;
    (void)ControllerMode;
}

/* ---- bare-metal replacements for services the shared sources expect ----------------------------------------- */
static uint32 rest_nest;
static uint32 rest_savedPrimask;

void SuspendAllInterrupts(void)
{
    uint32 pm;
    __asm__ volatile ("mrs %0, primask" : "=r" (pm));
    __asm__ volatile ("cpsid i" ::: "memory");
    if (rest_nest == 0u) { rest_savedPrimask = pm; }
    rest_nest++;
}
void ResumeAllInterrupts(void)
{
    if (rest_nest > 0u) {
        rest_nest--;
        if ((rest_nest == 0u) && (rest_savedPrimask == 0u)) {
            __asm__ volatile ("cpsie i" ::: "memory");
        }
    }
}
void SuspendOSInterrupts(void) { SuspendAllInterrupts(); }
void ResumeOSInterrupts(void)  { ResumeAllInterrupts(); }

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    TRACE(TRACE_CAT_DET, "ERROR mod=%u inst=%u api=%u err=%u", (unsigned)ModuleId, (unsigned)InstanceId,
          (unsigned)ApiId, (unsigned)ErrorId);
    return E_OK;
}
Std_ReturnType Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    TRACE(TRACE_CAT_DET, "RUNTIME mod=%u inst=%u api=%u err=%u", (unsigned)ModuleId, (unsigned)InstanceId,
          (unsigned)ApiId, (unsigned)ErrorId);
    return E_OK;
}
Std_ReturnType Det_ReportTransientFault(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 FaultId)
{
    return Det_ReportRuntimeError(ModuleId, InstanceId, ApiId, FaultId);
}
