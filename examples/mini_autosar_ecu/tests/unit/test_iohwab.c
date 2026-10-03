// SOURCES: ecual/iohwab/IoHwAb.c mcal/adc/Adc.c mcal/dio/Dio.c mcal/port/Port.c gen/LightEcu/Port_Cfg.c gen/LightEcu/Adc_Cfg.c sim/host/SimMmio.c sim/host/SimPeripherals.c
/*
 * test_iohwab.c
 *
 * [Educational Implementation]
 * Unit test of the I/O Hardware Abstraction (host build, LightEcu flavour). IoHwAb.c runs on the REAL Adc, Dio and Port drivers and the
 * generated LightEcu configuration (gen/LightEcu/Port_Cfg.c, Adc_Cfg.c) against the simulated GPIO/ADC1 peripherals, so the whole
 * vertical SWC-facing path IoHwAb -> MCAL -> register file is covered. Real AUTOSAR counterpart: ECU specific IoHwAb integration
 * test (AUTOSAR_CP_SWS_IOHardwareAbstraction, mandatory interfaces 8.8.1).
 * Checks: init guard + Det, wheel speed raw values (fixed and scenario profile following virtual time), ADC timeout (hardware not
 * enabled -> E_NOT_OK and the group is stopped), headlight state mapping OFF/LOW/HIGH to PC7/PB7 incl. read-back, invalid state,
 * trace lines (ADC raw every 50th call, LIGHT state on change).
 */
#include "mcal_test_support.h"
#include "IoHwAb.h"
#include "Adc.h"
#include "Dio.h"
#include "Port.h"
#include "SimMmio.h"

static int count_trace(const char *needle)
{
    int i, n = 0;
    for (i = 0; (i < ts_trace_count) && (i < TS_TRACE_LINES); i++) {
        if (strstr(ts_trace[i], needle) != NULL) { n++; }
    }
    return n;
}

int main(void)
{
    uint16 raw = 0u;
    uint8  st = 99u;
    int    i;

    SimPeripherals_Init();

    /* ---- before IoHwAb_Init ---- */
    T_CHECK(IoHwAb_GetWheelSpeed(&raw) == E_NOT_OK);
    T_CHECK((ts_det_count == 1) && (ts_det_last_module == MINI_MODULE_IOHWAB) && (ts_det_last_error == 0x01u));

    Port_Init(&Port_Config);
    Adc_Init(&Adc_Config);
    IoHwAb_Init();
    T_CHECK(IoHwAb_GetWheelSpeed(NULL_PTR) == E_NOT_OK);
    T_CHECK((ts_det_count == 2) && (ts_det_last_error == 0x02u));

    /* ---- wheel speed: fixed values ---- */
    SimAdc_SetRaw(2048u);
    T_CHECK((IoHwAb_GetWheelSpeed(&raw) == E_OK) && (raw == 2048u));
    SimAdc_SetRaw(4095u);
    T_CHECK((IoHwAb_GetWheelSpeed(&raw) == E_OK) && (raw == 4095u));
    SimAdc_SetRaw(0u);
    T_CHECK((IoHwAb_GetWheelSpeed(&raw) == E_OK) && (raw == 0u));

    /* ---- wheel speed: scenario profile follows the virtual time ---- */
    SimAdc_ClearOverride();
    ts_now_us = 1000000u;                                              /* 1 s: 3*(1000-200) = 2400 */
    T_CHECK((IoHwAb_GetWheelSpeed(&raw) == E_OK) && (raw == 2400u));
    ts_now_us = 150000u;
    T_CHECK((IoHwAb_GetWheelSpeed(&raw) == E_OK) && (raw == 0u));
    T_CHECK(Adc_GetGroupStatus(AdcConf_AdcGroup_WheelSpeed) == ADC_IDLE);   /* result consumed */

    /* ---- trace: ADC raw only every 50th call ---- */
    ts_trace_clear();
    for (i = 0; i < 101; i++) {
        (void)IoHwAb_GetWheelSpeed(&raw);
    }
    T_CHECK(count_trace("IOHWAB ADC raw=") == 2);                      /* the call counter is global (5 calls so far): calls 50 and 100 trace */

    /* ---- headlight ---- */
    T_CHECK(IoHwAb_GetHeadlight(&st) == E_OK);
    T_CHECK(st == IOHWAB_LIGHT_OFF);
    ts_trace_clear();
    T_CHECK(IoHwAb_SetHeadlight(IOHWAB_LIGHT_LOW) == E_OK);
    T_CHECK((SimDio_GetOutput(DioConf_DioChannel_HeadlightLow) == TRUE) && (SimDio_GetOutput(DioConf_DioChannel_HeadlightHigh) == FALSE));
    T_CHECK((IoHwAb_GetHeadlight(&st) == E_OK) && (st == IOHWAB_LIGHT_LOW));
    T_CHECK(IoHwAb_SetHeadlight(IOHWAB_LIGHT_LOW) == E_OK);            /* same state again: no new trace line */
    T_CHECK(count_trace("IOHWAB LIGHT state=1") == 1);
    T_CHECK(IoHwAb_SetHeadlight(IOHWAB_LIGHT_HIGH) == E_OK);           /* high beam: low beam stays on */
    T_CHECK((SimDio_GetOutput(DioConf_DioChannel_HeadlightLow) == TRUE) && (SimDio_GetOutput(DioConf_DioChannel_HeadlightHigh) == TRUE));
    T_CHECK((IoHwAb_GetHeadlight(&st) == E_OK) && (st == IOHWAB_LIGHT_HIGH));
    T_CHECK(ts_trace_has("IOHWAB LIGHT state=2"));
    T_CHECK(IoHwAb_SetHeadlight(7u) == E_NOT_OK);                      /* invalid state: outputs unchanged */
    T_CHECK((SimDio_GetOutput(DioConf_DioChannel_HeadlightLow) == TRUE) && (SimDio_GetOutput(DioConf_DioChannel_HeadlightHigh) == TRUE));
    T_CHECK(IoHwAb_SetHeadlight(IOHWAB_LIGHT_OFF) == E_OK);
    T_CHECK((SimDio_GetOutput(DioConf_DioChannel_HeadlightLow) == FALSE) && (SimDio_GetOutput(DioConf_DioChannel_HeadlightHigh) == FALSE));
    T_CHECK((IoHwAb_GetHeadlight(&st) == E_OK) && (st == IOHWAB_LIGHT_OFF));
    T_CHECK(IoHwAb_GetHeadlight(NULL_PTR) == E_NOT_OK);

    /* ---- ADC timeout: hardware reset behind the driver's back -> ADEN = 0 -> conversion never completes ---- */
    SimPeripherals_Init();
    SimAdc_SetRaw(1000u);
    T_CHECK(IoHwAb_GetWheelSpeed(&raw) == E_NOT_OK);
    T_CHECK(Adc_GetGroupStatus(AdcConf_AdcGroup_WheelSpeed) == ADC_IDLE);   /* IoHwAb stopped the group */

    T_DONE();
}
