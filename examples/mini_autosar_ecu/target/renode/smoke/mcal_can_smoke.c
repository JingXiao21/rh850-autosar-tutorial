/*
 * mcal_can_smoke.c  (target/renode/smoke)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: an MCAL integration/bring-up test (what a BSW integrator runs before the OS and the communication stack
 * exist). BARE-METAL (no OS, no CanIf/PduR/Com): exercises the REAL MCAL sources of this project on two Renode machines connected by
 * a CANHub:  Mcu_Init/InitClock/DistributePllClock, Port_Init, Dio, Adc, Can (Can_Init, Can_SetControllerMode, Can_Write,
 * Can_Isr_Rx, Can_MainFunction_Write/Read) + Can_Hw_Stm32.c. The CanIf callbacks, Det, Trace and the SchM/OS critical-section
 * services are replaced by tiny stubs defined here (UART output).
 *
 * Built twice (build_bringup.sh): -DSMOKE_ROLE=0 "NODE_A" (sender, RX in POLLING mode) and -DSMOKE_ROLE=1 "NODE_B" (receiver, RX
 * via FDCAN1_IT0 interrupt = IRQ 39 -> Can_Isr_Rx, i.e. what the Cat2 ISR does in the OS images).
 *   A: wheel speed ADC (channel 6) -> Can_Write 0x101 {raw lo, raw hi}; second Can_Write on the busy HTH -> CAN_BUSY; waits for
 *      the TX confirmation (Can_MainFunction_Write); sends 0x7AA which B's hardware filter must reject; waits for reply 0x301
 *      via Can_MainFunction_Read (polling).
 *   B: receives 0x101 in the ISR, replies 0x301 with byte0+1; 0x7AA must never show up.
 * Each node prints "SMOKE <A|B> ... PASS|FAIL" lines; run_smoke.sh checks them.
 */
#include <stdarg.h>
#include <stdint.h>
#include "Can.h"
#include "CanIf_Cbk.h"
#include "Mcu.h"
#include "Port.h"
#include "Dio.h"
#include "Adc.h"
#include "Det.h"
#include "Trace.h"
#include "Os.h"
#include "../common/mini_uart.h"

#ifndef SMOKE_ROLE
#error "define SMOKE_ROLE=0 (sender A) or 1 (receiver B)"
#endif
#define ROLE_NAME   ((SMOKE_ROLE) == 0 ? "A" : "B")

#define R32(a) (*(volatile uint32_t *)(a))
#define NVIC_ISER1   0xE000E104u

/* ------------------------------------------------------------------------------------------ configuration (hand written)  */
const Mcu_ConfigType Mcu_Config = { 80000000u, 1u };

static const Port_PinConfigType smoke_pins[] = {
    { PortConf_PortPin_Pin_Usart1Tx,       PORT_PIN_OUT,    7u, FALSE, STD_HIGH, PORT_PULL_NONE, FALSE },
    { PortConf_PortPin_Pin_Usart1Rx,       PORT_PIN_IN,     7u, FALSE, STD_LOW,  PORT_PULL_UP,   FALSE },
    { PortConf_PortPin_Pin_WheelSpeedAdc,  PORT_PIN_ANALOG, 0u, FALSE, STD_LOW,  PORT_PULL_NONE, FALSE },
    { PortConf_PortPin_Pin_HeadlightLow,   PORT_PIN_OUT,    0u, FALSE, STD_LOW,  PORT_PULL_NONE, FALSE },
    { PortConf_PortPin_Pin_HeadlightHigh,  PORT_PIN_OUT,    0u, TRUE,  STD_LOW,  PORT_PULL_NONE, FALSE },
    { 49u /* PD1 */,                       PORT_PIN_OUT,    9u, FALSE, STD_HIGH, PORT_PULL_NONE, FALSE },   /* FDCAN1_TX AF9 */
    { 48u /* PD0 */,                       PORT_PIN_IN,     9u, FALSE, STD_LOW,  PORT_PULL_UP,   FALSE },   /* FDCAN1_RX AF9 */
};
const Port_ConfigType Port_Config = { (uint16)(sizeof(smoke_pins) / sizeof(smoke_pins[0])), smoke_pins };

static const Adc_GroupConfigType smoke_adc_groups[] = { { 6u, 4u } };
const Adc_ConfigType Adc_Config = { 1u, smoke_adc_groups };

static const Can_HohConfigType smoke_hoh[] = {
    { CanConf_CanHardwareObject_Hrh_VehicleSpeed, CAN_HOH_RECEIVE,  0x101u, 0x7FFu, 0u },
    { CanConf_CanHardwareObject_Hrh_AmbientLight, CAN_HOH_RECEIVE,  0x301u, 0x7FFu, 0u },
    { CanConf_CanHardwareObject_Hrh_EcuModeReq,   CAN_HOH_RECEIVE,  0x3F0u, 0x7FFu, 0u },
    { CanConf_CanHardwareObject_Hth_LightStatus,  CAN_HOH_TRANSMIT, 0x201u, 0x7FFu, 0u },
};
const Can_ConfigType Can_Config = { 500000u, ((SMOKE_ROLE) == 1) ? TRUE : FALSE, 4u, smoke_hoh };

/* ------------------------------------------------------------------------------------------ stubs for the upper layers */
static volatile uint32_t s_ms;
void SysTick_Handler(void) { s_ms++; }
static void delay_ms(uint32_t ms) { uint32_t t = s_ms; while ((s_ms - t) < ms) { } }

static volatile uint32_t s_nest;
void SuspendOSInterrupts(void) { __asm volatile ("cpsid i" ::: "memory"); s_nest++; }
void ResumeOSInterrupts(void)  { if ((s_nest > 0u) && (--s_nest == 0u)) { __asm volatile ("cpsie i" ::: "memory"); } }
void SuspendAllInterrupts(void) { SuspendOSInterrupts(); }
void ResumeAllInterrupts(void)  { ResumeOSInterrupts(); }

const Det_ConfigType Det_Config = { 0u };
static volatile uint32_t s_detCount;
Std_ReturnType Det_ReportError(uint16 m, uint8 i, uint8 a, uint8 e)
{
    s_detCount++;
    mu_puts("  DET mod="); mu_hex(m); mu_puts(" inst="); mu_hex(i); mu_puts(" api="); mu_hex(a); mu_puts(" err="); mu_hex(e); mu_puts("\r\n");
    return E_OK;
}
Std_ReturnType Det_ReportRuntimeError(uint16 m, uint8 i, uint8 a, uint8 e) { return Det_ReportError(m, i, a, e); }

static void put_num(unsigned v, unsigned base, unsigned width, char fill)
{
    char buf[12];
    unsigned n = 0u;
    do { buf[n++] = "0123456789abcdef"[v % base]; v /= base; } while ((v != 0u) && (n < sizeof(buf)));
    while (width > n) { mu_putc(fill); width--; }
    while (n > 0u) { mu_putc(buf[--n]); }
}
/* minimal formatter: %u %x %03x %s %B(ptr,len) %% - the same subset as include/Trace.h */
void Trace_Log(Trace_CategoryType cat, const char *fmt, ...)
{
    va_list ap;
    (void)cat;
    va_start(ap, fmt);
    mu_puts("  trace ");
    while (*fmt != '\0') {
        if (*fmt != '%') { mu_putc(*fmt++); continue; }
        fmt++;
        {
            char fill = ' ';
            unsigned width = 0u;
            if (*fmt == '0') { fill = '0'; fmt++; }
            while ((*fmt >= '0') && (*fmt <= '9')) { width = width * 10u + (unsigned)(*fmt - '0'); fmt++; }
            switch (*fmt) {
            case 'u': put_num(va_arg(ap, unsigned), 10u, width, fill); break;
            case 'x': case 'X': put_num(va_arg(ap, unsigned), 16u, width, fill); break;
            case 's': mu_puts(va_arg(ap, const char *)); break;
            case 'B': {
                const uint8_t *p = va_arg(ap, const uint8_t *);
                unsigned len = va_arg(ap, unsigned), k;
                for (k = 0u; k < len; k++) { if (k != 0u) { mu_putc(' '); } put_num(p[k], 16u, 2u, '0'); }
                break; }
            default: mu_putc('%'); break;
            }
            if (*fmt != '\0') { fmt++; }
        }
    }
    mu_puts("\r\n");
    va_end(ap);
}

typedef struct { uint32_t id; uint8_t dlc; uint8_t data[8]; uint16_t hoh; } smoke_rx_t;
static volatile uint32_t s_rxCount;
static smoke_rx_t        s_rx[8];
static volatile uint32_t s_txConf;
static volatile uint32_t s_txConfHandle;
static volatile uint32_t s_mode;

void CanIf_RxIndication(const Can_HwType *mb, const PduInfoType *pdu)       /* called in ISR (B) or from Can_MainFunction_Read (A) */
{
    uint32_t n = s_rxCount;
    if (n < 8u) {
        uint8_t i;
        s_rx[n].id  = mb->CanId;
        s_rx[n].hoh = mb->Hoh;
        s_rx[n].dlc = (uint8_t)pdu->SduLength;
        for (i = 0u; (i < 8u) && (i < pdu->SduLength); i++) { s_rx[n].data[i] = pdu->SduDataPtr[i]; }
    }
    s_rxCount = n + 1u;
}
void CanIf_TxConfirmation(PduIdType h) { s_txConfHandle = h; s_txConf++; }
void CanIf_ControllerBusOff(uint8 c) { (void)c; mu_puts("  BUSOFF\r\n"); }
void CanIf_ControllerModeIndication(uint8 c, Can_ControllerStateType m) { (void)c; s_mode = (uint32_t)m; }

void FDCAN1_IT0_IRQHandler(void) { Can_Isr_Rx(); }                          /* = ISR(Isr_CanRx) { Can_Isr_Rx(); } in the OS images */

/* ------------------------------------------------------------------------------------------ test steps */
static unsigned s_fail;
static void check(const char *name, int ok)
{
    mu_puts("SMOKE "); mu_puts(ROLE_NAME); mu_puts(ok ? " ok   " : " FAIL ");
    mu_puts(name); mu_puts("\r\n");
    if (!ok) { s_fail++; }
}
static void show(const char *name, uint32_t v) { mu_puts("SMOKE "); mu_puts(ROLE_NAME); mu_puts(" info "); mu_reg(name, v); }

static void start_systick(void)
{
    R32(0xE000E014u) = 80000000u / 1000u - 1u;
    R32(0xE000E018u) = 0u;
    R32(0xE000E010u) = 7u;
}

static void test_mcu_port_dio_adc(uint16_t *raw)
{
    Adc_ValueGroupType buf = 0u, res = 0u;
    Mcu_Init(&Mcu_Config);
    show("resetReason", (uint32_t)Mcu_GetResetReason());
    check("Mcu_InitClock", Mcu_InitClock(McuConf_McuClockSettingConfig_0) == E_OK);
    check("Mcu_GetPllStatus==LOCKED", Mcu_GetPllStatus() == MCU_PLL_LOCKED);
    check("Mcu_DistributePllClock", Mcu_DistributePllClock() == E_OK);
    show("sysClockHz", Mcu_GetSystemClockHz());
    show("RCC.CFGR", R32(0x40021008u));

    Port_Init(&Port_Config);
    show("GPIOC.MODER", R32(0x42020800u));
    Dio_WriteChannel(DioConf_DioChannel_HeadlightLow, STD_HIGH);
    check("Dio low beam high", Dio_ReadChannel(DioConf_DioChannel_HeadlightLow) == STD_HIGH);
    show("GPIOC.ODR", R32(0x42020814u));
    check("Dio_FlipChannel returns new level", Dio_FlipChannel(DioConf_DioChannel_HeadlightLow) == STD_LOW);
    check("Dio low beam low", Dio_ReadChannel(DioConf_DioChannel_HeadlightLow) == STD_LOW);
    Dio_WriteChannel(DioConf_DioChannel_HeadlightHigh, STD_HIGH);
    check("Dio_ReadPort(B) bit7", (Dio_ReadPort(1u) & 0x80u) != 0u);

    Adc_Init(&Adc_Config);
    check("Adc_SetupResultBuffer", Adc_SetupResultBuffer(AdcConf_AdcGroup_WheelSpeed, &buf) == E_OK);
    Adc_StartGroupConversion(AdcConf_AdcGroup_WheelSpeed);
    check("Adc status COMPLETED", Adc_GetGroupStatus(AdcConf_AdcGroup_WheelSpeed) == ADC_COMPLETED);
    check("Adc_ReadGroup", Adc_ReadGroup(AdcConf_AdcGroup_WheelSpeed, &res) == E_OK);
    show("adc.raw", res);
    check("Adc status IDLE after read", Adc_GetGroupStatus(AdcConf_AdcGroup_WheelSpeed) == ADC_IDLE);
    *raw = res;
}

int main(void)
{
    uint16_t raw = 0u;
    Can_PduType pdu;
    uint8_t data[8];
    uint32_t t;

    mu_init();
    mu_puts("SMOKE "); mu_puts(ROLE_NAME); mu_puts(" start\r\n");
    start_systick();
    test_mcu_port_dio_adc(&raw);

    Can_Init(&Can_Config);
    check("Can_Write before START -> CAN_NOT_OK", (pdu.swPduHandle = 0u, pdu.length = 0u, pdu.id = 0x201u, pdu.sdu = data,
                                                  Can_Write(CanConf_CanHardwareObject_Hth_LightStatus, &pdu)) == CAN_NOT_OK);
    check("Can_SetControllerMode(START)", Can_SetControllerMode(0u, CAN_T_START) == E_OK);
    check("CanIf mode indication STARTED", s_mode == (uint32_t)CAN_CS_STARTED);
    s_detCount = 0u;
    check("second START rejected (CAN_E_TRANSITION)", (Can_SetControllerMode(0u, CAN_T_START) == E_NOT_OK) && (s_detCount == 1u));
    if (Can_Config.rxInterrupt) {
        R32(NVIC_ISER1) = 1u << (39u - 32u);                 /* enable IRQ 39 (FDCAN1_IT0) in the NVIC */
    }

#if SMOKE_ROLE == 0
    delay_ms(30);                                            /* let node B come up */
    data[0] = (uint8_t)(raw & 0xFFu); data[1] = (uint8_t)(raw >> 8);
    pdu.swPduHandle = 7u; pdu.length = 2u; pdu.id = 0x101u; pdu.sdu = data;
    check("Can_Write 0x101 -> CAN_OK", Can_Write(CanConf_CanHardwareObject_Hth_LightStatus, &pdu) == CAN_OK);
    check("Can_Write while HTH busy -> CAN_BUSY", Can_Write(CanConf_CanHardwareObject_Hth_LightStatus, &pdu) == CAN_BUSY);
    for (t = 0u; (t < 50u) && (s_txConf == 0u); t++) { Can_MainFunction_Write(); delay_ms(1); }
    check("TX confirmation for handle 7", (s_txConf == 1u) && (s_txConfHandle == 7u));
    data[0] = 0xEEu;
    pdu.swPduHandle = 8u; pdu.length = 1u; pdu.id = 0x7AAu;
    check("Can_Write 0x7AA -> CAN_OK", Can_Write(CanConf_CanHardwareObject_Hth_LightStatus, &pdu) == CAN_OK);
    for (t = 0u; (t < 50u) && (s_txConf < 2u); t++) { Can_MainFunction_Write(); delay_ms(1); }
    check("TX confirmation for handle 8", (s_txConf == 2u) && (s_txConfHandle == 8u));
    for (t = 0u; (t < 200u) && (s_rxCount == 0u); t++) { Can_MainFunction_Read(); delay_ms(1); }   /* polling RX mode */
    check("reply 0x301 received by polling", (s_rxCount == 1u) && (s_rx[0].id == 0x301u) && (s_rx[0].hoh == CanConf_CanHardwareObject_Hrh_AmbientLight));
    check("reply data == raw+1", (s_rx[0].dlc == 1u) && (s_rx[0].data[0] == (uint8_t)((raw & 0xFFu) + 1u)));
    show("reply.id", s_rx[0].id);
    check("Can_SetControllerMode(STOP)", Can_SetControllerMode(0u, CAN_T_STOP) == E_OK);
    check("mode indication STOPPED", s_mode == (uint32_t)CAN_CS_STOPPED);
#else
    for (t = 0u; (t < 300u) && (s_rxCount == 0u); t++) { delay_ms(1); }   /* wait for the interrupt */
    check("0x101 received by interrupt", (s_rxCount >= 1u) && (s_rx[0].id == 0x101u) && (s_rx[0].dlc == 2u) &&
                                         (s_rx[0].hoh == CanConf_CanHardwareObject_Hrh_VehicleSpeed));
    show("rx.data0", s_rx[0].data[0]); show("rx.data1", s_rx[0].data[1]);
    check("0x101 payload == ADC raw of node A (1024 @ 825 mV)", (s_rx[0].data[0] == 0x00u) && (s_rx[0].data[1] == 0x04u));
    data[0] = (uint8_t)(s_rx[0].data[0] + 1u);                /* reply: 0x301 */
    pdu.swPduHandle = 3u; pdu.length = 1u; pdu.id = 0x301u; pdu.sdu = data;
    check("Can_Write reply 0x301", Can_Write(CanConf_CanHardwareObject_Hth_LightStatus, &pdu) == CAN_OK);
    for (t = 0u; (t < 50u) && (s_txConf == 0u); t++) { Can_MainFunction_Write(); delay_ms(1); }
    check("TX confirmation of reply", (s_txConf == 1u) && (s_txConfHandle == 3u));
    delay_ms(150);                                           /* the filtered 0x7AA frame must NOT show up */
    check("hardware filter rejected 0x7AA (only 1 frame received)", s_rxCount == 1u);
#endif
    mu_puts("SMOKE "); mu_puts(ROLE_NAME); mu_puts(s_fail == 0u ? " PASS\r\n" : " FAILED\r\n");
    for (;;) { }
}
