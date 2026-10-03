// SOURCES: mcal/dio/Dio.c mcal/port/Port.c sim/host/SimMmio.c sim/host/SimPeripherals.c
/*
 * test_mcal_dio.c
 *
 * [Educational Implementation]
 * Unit test of the Port and Dio MCAL drivers on the HOST build: the unmodified driver sources run against the simulated GPIO
 * register file (SimMmio + SimPeripherals). Real AUTOSAR counterpart: MCAL module test of Port (SWS_Port_00140..00145) and Dio
 * (SWS_Dio_00133..00190). Checks: Port_Init register effects (MODER/AFR/ASCR/clock gate/initial level), Dio read/write/flip/port
 * access through BSRR/ODR/IDR, input injection, Det errors, direction/mode change guards, GPIO trace lines.
 */
#include "mcal_test_support.h"
#include "Port.h"
#include "Dio.h"
#include "Mmio.h"
#include "SimMmio.h"

#define GPIOA 0x42020000u
#define GPIOB 0x42020400u
#define GPIOC 0x42020800u
#define GPIOD 0x42020C00u

static const Port_PinConfigType pins[] = {
    /* pin  direction         mode  changeable level      pull            openDrain */
    {  7u + 32u, PORT_PIN_OUT,    0u, FALSE, STD_LOW,  PORT_PULL_NONE, FALSE },   /* PC7  headlight low  */
    {  7u + 16u, PORT_PIN_OUT,    0u, FALSE, STD_HIGH, PORT_PULL_NONE, TRUE  },   /* PB7  initial HIGH, open drain */
    {  1u,       PORT_PIN_ANALOG, 0u, FALSE, STD_LOW,  PORT_PULL_NONE, FALSE },   /* PA1  ADC input */
    {  0u,       PORT_PIN_IN,     0u, TRUE,  STD_LOW,  PORT_PULL_UP,   FALSE },   /* PA0  plain input */
    {  5u,       PORT_PIN_OUT,    0u, TRUE,  STD_LOW,  PORT_PULL_NONE, FALSE },   /* PA5  changeable */
    {  0u + 48u, PORT_PIN_IN,     9u, FALSE, STD_LOW,  PORT_PULL_UP,   FALSE },   /* PD0  FDCAN1_RX AF9 */
};
static const Port_ConfigType cfg = { (uint16)(sizeof(pins) / sizeof(pins[0])), pins };

int main(void)
{
    SimPeripherals_Init();

    /* before Port_Init: Det PORT_E_UNINIT (0x0F) */
    Port_SetPinDirection(5u, PORT_PIN_IN);
    T_CHECK((ts_det_count == 1) && (ts_det_last_module == MINI_MODULE_PORT) && (ts_det_last_error == 0x0Fu));
    Port_Init(NULL_PTR);
    T_CHECK((ts_det_count == 2) && (ts_det_last_error == 0x10u));         /* PORT_E_PARAM_POINTER */

    Port_Init(&cfg);
    T_CHECK(ts_det_count == 2);
    T_CHECK((Mmio_Read32(0x4002104Cu) & 0xFu) == 0xFu);                  /* RCC_AHB2ENR: GPIOA..GPIOD clock gates on */
    T_CHECK(((Mmio_Read32(GPIOC + 0x00u) >> 14) & 3u) == 1u);            /* PC7 MODER = output */
    T_CHECK(((Mmio_Read32(GPIOB + 0x00u) >> 14) & 3u) == 1u);            /* PB7 MODER = output */
    T_CHECK(((Mmio_Read32(GPIOB + 0x04u) >> 7) & 1u) == 1u);             /* PB7 open drain (OTYPER) */
    T_CHECK(SimDio_GetOutput(23u) == TRUE);                              /* PB7 initial level HIGH, written before MODER */
    T_CHECK(ts_trace_has("GPIO PB7=1"));
    T_CHECK(((Mmio_Read32(GPIOA + 0x00u) >> 2) & 3u) == 3u);             /* PA1 analog */
    T_CHECK(((Mmio_Read32(GPIOA + 0x2Cu) >> 1) & 1u) == 1u);             /* PA1 analog switch ASCR */
    T_CHECK(((Mmio_Read32(GPIOA + 0x0Cu)) & 3u) == 1u);                  /* PA0 pull-up in PUPDR */
    T_CHECK(((Mmio_Read32(GPIOD + 0x00u)) & 3u) == 2u);                  /* PD0 alternate function */
    T_CHECK((Mmio_Read32(GPIOD + 0x20u) & 0xFu) == 9u);                  /* AFRL nibble 0 = AF9 */

    /* Dio_WriteChannel / ReadChannel / FlipChannel */
    T_CHECK(Dio_ReadChannel(39u) == STD_LOW);
    Dio_WriteChannel(39u, STD_HIGH);
    T_CHECK(SimDio_GetOutput(39u) == TRUE);
    T_CHECK(Dio_ReadChannel(39u) == STD_HIGH);                           /* output channel: read from ODR */
    T_CHECK(ts_trace_has("GPIO PC7=1"));
    T_CHECK(Dio_FlipChannel(39u) == STD_LOW);                            /* returns the NEW level */
    T_CHECK(SimDio_GetOutput(39u) == FALSE);
    T_CHECK(ts_trace_has("GPIO PC7=0"));
    Dio_WriteChannel(39u, STD_LOW);                                      /* no change -> no new trace line */
    {
        int before = ts_trace_count;
        Dio_WriteChannel(39u, STD_LOW);
        T_CHECK(ts_trace_count == before);
    }

    /* BSRR: set and reset of the same bit in one write -> reset wins (RM0438), other pins untouched */
    Mmio_Write32(GPIOC + 0x18u, (1u << 7) | (1u << (7u + 16u)) | (1u << 3));
    T_CHECK(SimDio_GetOutput(39u) == FALSE);
    T_CHECK(SimDio_GetOutput(32u + 3u) == TRUE);
    Mmio_Write32(GPIOC + 0x18u, 1u << (3u + 16u));

    /* inputs: IDR shows the injected level, Dio_Write on an input pin only changes ODR */
    T_CHECK(Dio_ReadChannel(0u) == STD_LOW);
    SimDio_SetInput(0u, TRUE);
    T_CHECK(Dio_ReadChannel(0u) == STD_HIGH);
    SimDio_SetInput(0u, FALSE);
    T_CHECK(Dio_ReadChannel(0u) == STD_LOW);
    Dio_WriteChannel(0u, STD_HIGH);
    T_CHECK(Dio_ReadChannel(0u) == STD_LOW);

    /* ports */
    T_CHECK((Dio_ReadPort(1u) & 0x80u) == 0x80u);                        /* PB7 drives high */
    Dio_WritePort(2u, 0x0081u);                                          /* PC: ODR = 0x81 */
    T_CHECK((Mmio_Read32(GPIOC + 0x14u) & 0xFFFFu) == 0x0081u);
    T_CHECK(Dio_ReadPort(2u) == 0x0080u);                                /* only PC7 is an output; PC0 is analog (reads 0) */
    Dio_WritePort(2u, 0u);
    T_CHECK((Mmio_Read32(GPIOC + 0x14u) & 0xFFFFu) == 0u);

    /* Det */
    {
        int c = ts_det_count;
        (void)Dio_ReadChannel(0x90u);                                    /* port index 9 does not exist */
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_module == MINI_MODULE_DIO) && (ts_det_last_error == 0x0Au));
        (void)Dio_ReadPort(9u);
        T_CHECK((ts_det_count == c + 2) && (ts_det_last_error == 0x14u));
        Dio_GetVersionInfo(NULL_PTR);
        T_CHECK((ts_det_count == c + 3) && (ts_det_last_error == 0x20u));
    }

    /* Port_SetPinDirection / Port_SetPinMode guards */
    {
        int c = ts_det_count;
        Port_SetPinDirection(39u, PORT_PIN_IN);                          /* PC7: direction not changeable */
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x0Bu));
        T_CHECK(((Mmio_Read32(GPIOC + 0x00u) >> 14) & 3u) == 1u);
        Port_SetPinDirection(5u, PORT_PIN_OUT);                          /* PA5 changeable */
        T_CHECK(ts_det_count == c + 1);
        T_CHECK(((Mmio_Read32(GPIOA + 0x00u) >> 10) & 3u) == 1u);
        Port_SetPinDirection(5u, PORT_PIN_IN);
        T_CHECK(((Mmio_Read32(GPIOA + 0x00u) >> 10) & 3u) == 0u);
        Port_SetPinDirection(6u, PORT_PIN_IN);                           /* not configured */
        T_CHECK((ts_det_count == c + 2) && (ts_det_last_error == 0x0Au));
        Port_SetPinDirection(5u, PORT_PIN_ANALOG);                       /* invalid direction for this API */
        T_CHECK((ts_det_count == c + 3) && (ts_det_last_error == 0x11u));
        Port_SetPinMode(5u, 7u);
        T_CHECK(((Mmio_Read32(GPIOA + 0x00u) >> 10) & 3u) == 2u);
        T_CHECK(((Mmio_Read32(GPIOA + 0x20u) >> 20) & 0xFu) == 7u);
        Port_SetPinMode(5u, 0u);                                         /* back to GPIO with the CONFIGURED direction (output) */
        T_CHECK(((Mmio_Read32(GPIOA + 0x00u) >> 10) & 3u) == 1u);
        Port_SetPinMode(39u, 3u);                                        /* PC7: mode not changeable */
        T_CHECK((ts_det_count == c + 4) && (ts_det_last_error == 0x0Eu));
        Port_SetPinMode(5u, 16u);
        T_CHECK((ts_det_count == c + 5) && (ts_det_last_error == 0x0Du));
    }

    /* Port_RefreshPortDirection re-writes the direction of unchangeable pins */
    Mmio_Modify32(GPIOC + 0x00u, 3u << 14, 0u);                          /* "disturb" PC7 direction */
    Port_RefreshPortDirection();
    T_CHECK(((Mmio_Read32(GPIOC + 0x00u) >> 14) & 3u) == 1u);

    T_DONE();
}
