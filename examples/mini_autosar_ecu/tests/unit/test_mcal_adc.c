// SOURCES: mcal/adc/Adc.c sim/host/SimMmio.c sim/host/SimPeripherals.c
/*
 * test_mcal_adc.c
 *
 * [Educational Implementation]
 * Unit test of the Adc MCAL driver on the HOST build against the simulated ADC1 (SimPeripherals.c). Real AUTOSAR counterpart:
 * MCAL module test of ADC single-shot software-triggered groups (SWS_Adc_00365 Adc_Init, 00367 Adc_StartGroupConversion, 00369
 * Adc_ReadGroup, 00374 Adc_GetGroupStatus). Checks: Det errors, power-up handshake, group state machine
 * IDLE -> BUSY -> COMPLETED -> IDLE, channel/sample-time programming, 12 bit result of the wheel-speed channel (fixed value and the
 * scenario profile SimAdc_ProfileRaw(t) following the virtual time), several groups.
 */
#include "mcal_test_support.h"
#include "Adc.h"
#include "Mmio.h"
#include "SimMmio.h"

#define ADC1 0x42028000u

static const Adc_GroupConfigType groups[] = { { 6u, 4u }, { 3u, 2u }, { 12u, 5u } };
static const Adc_ConfigType cfg = { 3u, groups };

int main(void)
{
    Adc_ValueGroupType buf0 = 0u, buf1 = 0u, v = 0u;
    SimPeripherals_Init();

    /* not initialised */
    Adc_StartGroupConversion(0u);
    T_CHECK((ts_det_count == 1) && (ts_det_last_module == MINI_MODULE_ADC) && (ts_det_last_error == 0x0Au));
    Adc_Init(NULL_PTR);
    T_CHECK((ts_det_count == 2) && (ts_det_last_error == 0x14u));

    Adc_Init(&cfg);
    T_CHECK(ts_det_count == 2);
    T_CHECK((Mmio_Read32(ADC1 + 0x08u) & 1u) == 1u);                 /* CR.ADEN */
    T_CHECK((Mmio_Read32(ADC1 + 0x00u) & 1u) == 1u);                 /* ISR.ADRDY */
    T_CHECK((Mmio_Read32(ADC1 + 0x08u) & (1u << 31)) == 0u);         /* calibration finished */
    T_CHECK((Mmio_Read32(0x4002104Cu) & (1u << 13)) != 0u);          /* ADC clock gate */
    Adc_Init(&cfg);
    T_CHECK((ts_det_count == 3) && (ts_det_last_error == 0x0Du));    /* ADC_E_ALREADY_INITIALIZED */

    /* buffer handling */
    Adc_StartGroupConversion(0u);
    T_CHECK((ts_det_count == 4) && (ts_det_last_error == 0x19u));    /* no result buffer yet */
    T_CHECK(Adc_SetupResultBuffer(0u, &buf0) == E_OK);
    T_CHECK(Adc_SetupResultBuffer(1u, &buf1) == E_OK);
    T_CHECK(Adc_SetupResultBuffer(7u, &buf0) == E_NOT_OK);
    T_CHECK((ts_det_count == 5) && (ts_det_last_error == 0x15u));    /* ADC_E_PARAM_GROUP */
    T_CHECK(Adc_SetupResultBuffer(0u, NULL_PTR) == E_NOT_OK);
    T_CHECK((ts_det_count == 6) && (ts_det_last_error == 0x14u));
    T_CHECK(Adc_GetGroupStatus(0u) == ADC_IDLE);

    /* one-shot conversion of the wheel-speed channel with a fixed value */
    SimAdc_SetRaw(1234u);
    Adc_StartGroupConversion(0u);
    T_CHECK(((Mmio_Read32(ADC1 + 0x14u) >> 18) & 7u) == 4u);        /* SMPR1.SMP6 = 4 */
    T_CHECK(((Mmio_Read32(ADC1 + 0x30u) >> 6) & 0x1Fu) == 6u);       /* SQR1.SQ1 = channel 6 */
    T_CHECK((Mmio_Read32(ADC1 + 0x30u) & 0xFu) == 0u);               /* L = 0: one conversion */
    Adc_StartGroupConversion(0u);                                    /* still BUSY (result not fetched yet) */
    T_CHECK((ts_det_count == 7) && (ts_det_last_error == 0x0Bu));    /* ADC_E_BUSY */
    T_CHECK(Adc_GetGroupStatus(0u) == ADC_COMPLETED);
    T_CHECK(buf0 == 1234u);
    T_CHECK(Adc_ReadGroup(0u, &v) == E_OK);
    T_CHECK(v == 1234u);
    T_CHECK(Adc_GetGroupStatus(0u) == ADC_IDLE);                     /* single access: result consumed */
    T_CHECK(Adc_ReadGroup(0u, &v) == E_NOT_OK);
    T_CHECK((ts_det_count == 8) && (ts_det_last_error == 0x0Cu));    /* ADC_E_IDLE */
    T_CHECK(SimAdc_GetConversionCount() == 1u);

    /* 12 bit masking and the full range */
    SimAdc_SetRaw(0x1FFFu);
    Adc_StartGroupConversion(0u);
    T_CHECK(Adc_ReadGroup(0u, &v) == E_OK);
    T_CHECK(v == 0x0FFFu);
    SimAdc_SetRaw(0u);
    Adc_StartGroupConversion(0u);
    T_CHECK((Adc_ReadGroup(0u, &v) == E_OK) && (v == 0u));

    /* another group / channel with its own sample time */
    SimAdc_SetChannelRaw(3u, 77u);
    Adc_StartGroupConversion(1u);
    T_CHECK(((Mmio_Read32(ADC1 + 0x14u) >> 9) & 7u) == 2u);          /* SMPR1.SMP3 */
    T_CHECK((Adc_ReadGroup(1u, &v) == E_OK) && (v == 77u));          /* ReadGroup polls the completion itself */
    T_CHECK(buf1 == 77u);
    {
        int c = ts_det_count;
        Adc_StartGroupConversion(2u);                                /* group 2 has no buffer */
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x19u));
    }

    /* stop */
    Adc_StartGroupConversion(0u);
    Adc_StopGroupConversion(0u);
    T_CHECK(Adc_GetGroupStatus(0u) == ADC_IDLE);

    /* scenario profile: raw(t) = 0 (t <= 200 ms) else min(3000, 3*(t-200)), driven by the virtual time */
    T_CHECK(SimAdc_ProfileRaw(0u) == 0u);
    T_CHECK(SimAdc_ProfileRaw(200u) == 0u);
    T_CHECK(SimAdc_ProfileRaw(201u) == 3u);
    T_CHECK(SimAdc_ProfileRaw(210u) == 30u);
    T_CHECK(SimAdc_ProfileRaw(1200u) == 3000u);
    T_CHECK(SimAdc_ProfileRaw(4000u) == 3000u);
    SimAdc_ClearOverride();
    ts_now_us = 500000u;                                             /* t = 500 ms -> raw 900 */
    Adc_StartGroupConversion(0u);
    T_CHECK((Adc_ReadGroup(0u, &v) == E_OK) && (v == 900u));
    ts_now_us = 2000000u;                                            /* t = 2 s -> saturated at 3000 */
    Adc_StartGroupConversion(0u);
    T_CHECK((Adc_ReadGroup(0u, &v) == E_OK) && (v == 3000u));
    ts_now_us = 100000u;                                             /* t = 100 ms -> still 0 */
    Adc_StartGroupConversion(0u);
    T_CHECK((Adc_ReadGroup(0u, &v) == E_OK) && (v == 0u));

    /* DeInit */
    Adc_DeInit();
    T_CHECK((Mmio_Read32(ADC1 + 0x08u) & 1u) == 0u);                 /* ADEN cleared */
    {
        int c = ts_det_count;
        Adc_StartGroupConversion(0u);
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x0Au));
    }
    T_DONE();
}
