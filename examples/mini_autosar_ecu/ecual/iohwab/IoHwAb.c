/*
 * IoHwAb.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: I/O Hardware Abstraction (AUTOSAR_CP_SWS_IOHardwareAbstraction, R25-11, chapter 8; concept chapter 7.5
 * "I/O Hardware Abstraction Scheduling concept", mandatory interfaces 8.8.1, IoHwAb_Init<Init_Id> 8.3.1). IoHwAb is ECU specific and
 * has no standardised API: it abstracts the ECU's sensors/actuators ("ECU signals") from the MCAL drivers and exposes them to the
 * SWCs as client/server operations through the RTE. In this project the two ports are:
 *   WheelSpeedIf.GetWheelSpeed -> IoHwAb_GetWheelSpeed   (ADC single conversion, raw 12 bit, no scaling to engineering units)
 *   LightHwIf.SetHeadlight     -> IoHwAb_SetHeadlight    (state OFF/LOW/HIGH -> two DIO output channels), IoHwAb_GetHeadlight
 * IoHwAb uses ONLY Adc.h and Dio.h (never registers), the generated ids AdcConf_AdcGroup_WheelSpeed and DioConf_DioChannel_Headlight*.
 * Output pins: low beam = PC7, high beam = PB7 (high beam is only meaningful together with low beam: HIGH switches both on).
 * Not implemented: signal inversion / debouncing / filtering, diagnostics, ADC notification and result buffers of several values.
 * The headlight part exists only when the generated Dio_Cfg.h defines the channel ids (LightEcu); on SensorEcu SetHeadlight /
 * GetHeadlight return E_NOT_OK.
 *
 * ADC wait: the conversion is asynchronous in AUTOSAR (Adc_GetGroupStatus); IoHwAb polls ADC_COMPLETED with a bounded loop
 * (about 100 us at 80 MHz; the sim and Renode ADC models finish at once), E_NOT_OK on timeout.
 * Spec: AUTOSAR_CP_SWS_IOHardwareAbstraction 8.3.1 IoHwAb_Init, 7.5 scheduling concept; Adc: SWS_Adc_00367/00374/00369, Dio: SWS_Dio_00133/00134.
 * Trace (DESIGN 12): "BSW IOHWAB ADC raw=%u" (every 50th conversion = 500 ms at the 10 ms task rate, to keep the UART quiet),
 * "BSW IOHWAB LIGHT state=%u" (every state change).
 */
#include "IoHwAb.h"
#include "Adc.h"
#include "Dio.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_ModuleIds.h"

#define IOHWAB_E_UNINIT        0x01u
#define IOHWAB_E_PARAM_POINTER 0x02u
#define IOHWAB_SID_INIT        0x00u
#define IOHWAB_SID_GETWHEEL    0x10u
#define IOHWAB_ADC_POLL_LOOPS  2000u
#define IOHWAB_ADC_TRACE_EVERY 50u

#if defined(DioConf_DioChannel_HeadlightLow) && defined(DioConf_DioChannel_HeadlightHigh)
#define IOHWAB_HAS_HEADLIGHT   1
#else
#define IOHWAB_HAS_HEADLIGHT   0
#endif

#define IOHWAB_DET(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_IOHWAB, 0u, (api), (err)); } } while (0)

static boolean            iohwab_init;
static Adc_ValueGroupType iohwab_adcBuf;
static uint32             iohwab_adcCalls;
#if IOHWAB_HAS_HEADLIGHT
static uint8              iohwab_lastLight = 0xFFu;
#endif

void IoHwAb_Init(void)
{
    if (Adc_SetupResultBuffer(AdcConf_AdcGroup_WheelSpeed, &iohwab_adcBuf) == E_OK) {
        iohwab_init = TRUE;
    }
    iohwab_adcCalls = 0u;
}

Std_ReturnType IoHwAb_GetWheelSpeed(uint16 *Raw)
{
    uint32 n;
    Adc_ValueGroupType v = 0u;
    if (!iohwab_init) {
        IOHWAB_DET(IOHWAB_SID_GETWHEEL, IOHWAB_E_UNINIT);
        return E_NOT_OK;
    }
    if (Raw == NULL_PTR) {
        IOHWAB_DET(IOHWAB_SID_GETWHEEL, IOHWAB_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    Adc_StartGroupConversion(AdcConf_AdcGroup_WheelSpeed);
    for (n = 0u; n < IOHWAB_ADC_POLL_LOOPS; n++) {
        if (Adc_GetGroupStatus(AdcConf_AdcGroup_WheelSpeed) == ADC_COMPLETED) {
            break;
        }
    }
    if (n >= IOHWAB_ADC_POLL_LOOPS) {
        Adc_StopGroupConversion(AdcConf_AdcGroup_WheelSpeed);
        return E_NOT_OK;
    }
    if (Adc_ReadGroup(AdcConf_AdcGroup_WheelSpeed, &v) != E_OK) {
        return E_NOT_OK;
    }
    *Raw = (uint16)(v & 0x0FFFu);
    if ((iohwab_adcCalls % IOHWAB_ADC_TRACE_EVERY) == 0u) {
        TRACE(TRACE_CAT_BSW, "IOHWAB ADC raw=%u", (unsigned)*Raw);
    }
    iohwab_adcCalls++;
    return E_OK;
}

#if IOHWAB_HAS_HEADLIGHT
Std_ReturnType IoHwAb_SetHeadlight(uint8 State)
{
    Dio_LevelType low;
    Dio_LevelType high;
    switch (State) {
    case IOHWAB_LIGHT_OFF:  low = STD_LOW;  high = STD_LOW;  break;
    case IOHWAB_LIGHT_LOW:  low = STD_HIGH; high = STD_LOW;  break;
    case IOHWAB_LIGHT_HIGH: low = STD_HIGH; high = STD_HIGH; break;     /* high beam: low beam stays on */
    default:                return E_NOT_OK;
    }
    Dio_WriteChannel(DioConf_DioChannel_HeadlightLow, low);
    Dio_WriteChannel(DioConf_DioChannel_HeadlightHigh, high);
    if (State != iohwab_lastLight) {
        iohwab_lastLight = State;
        TRACE(TRACE_CAT_BSW, "IOHWAB LIGHT state=%u", (unsigned)State);
    }
    return E_OK;
}

Std_ReturnType IoHwAb_GetHeadlight(uint8 *State)
{
    if (State == NULL_PTR) {
        IOHWAB_DET(IOHWAB_SID_GETWHEEL, IOHWAB_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (Dio_ReadChannel(DioConf_DioChannel_HeadlightHigh) == STD_HIGH) {
        *State = IOHWAB_LIGHT_HIGH;
    } else if (Dio_ReadChannel(DioConf_DioChannel_HeadlightLow) == STD_HIGH) {
        *State = IOHWAB_LIGHT_LOW;
    } else {
        *State = IOHWAB_LIGHT_OFF;
    }
    return E_OK;
}
#else
Std_ReturnType IoHwAb_SetHeadlight(uint8 State)
{
    (void)State;
    return E_NOT_OK;                                       /* this ECU has no headlight outputs (SensorEcu) */
}

Std_ReturnType IoHwAb_GetHeadlight(uint8 *State)
{
    (void)State;
    return E_NOT_OK;
}
#endif
