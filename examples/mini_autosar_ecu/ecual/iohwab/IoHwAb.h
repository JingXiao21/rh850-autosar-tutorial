/*
 * IoHwAb.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: I/O Hardware Abstraction (AUTOSAR_CP_SWS_IOHardwareAbstraction). IoHwAb
 * is ECU specific: its ports are exposed to SWCs through the RTE as client/server interfaces
 * (here: WheelSpeedIf, LightHwIf). The functions below are the server operations; the generated
 * RTE maps `Rte_Call_<port>_<op>` to them (config key "bswServer"). They convert physical hardware
 * (ADC counts, GPIO levels) into the signals the application wants; no scaling to engineering units.
 * Owner: agent B. Uses Adc.h and Dio.h only (never MCAL registers).
 */
#ifndef IOHWAB_H
#define IOHWAB_H

#include "Std_Types.h"

/* Headlight state values shared by IoHwAb and the SWCs (uint8 on the wire as well) */
#define IOHWAB_LIGHT_OFF    0u
#define IOHWAB_LIGHT_LOW    1u      /* low beam  -> DioConf_DioChannel_HeadlightLow  (PC7) */
#define IOHWAB_LIGHT_HIGH   2u      /* high beam -> DioConf_DioChannel_HeadlightHigh (PB7), low stays on */

void           IoHwAb_Init(void);                              /* sets up the ADC result buffer */
/* Triggers ADC group WheelSpeed (single shot), waits for ADC_COMPLETED (bounded polling, 100 us timeout)
 * and returns the 12-bit raw value 0..4095. E_NOT_OK on timeout. */
Std_ReturnType IoHwAb_GetWheelSpeed(uint16 *Raw);
/* State = IOHWAB_LIGHT_*; E_NOT_OK for other values. */
Std_ReturnType IoHwAb_SetHeadlight(uint8 State);
Std_ReturnType IoHwAb_GetHeadlight(uint8 *State);              /* read-back from the Dio output pins */

#endif /* IOHWAB_H */
