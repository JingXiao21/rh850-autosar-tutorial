/*
 * Adc.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: ADC Driver (AUTOSAR_CP_SWS_ADCDriver). Implemented: single-shot,
 * software-triggered, one-channel groups, polling completion (no streaming, no DMA, no notifications).
 * Adc_StartGroupConversion starts the conversion; Adc_GetGroupStatus reports ADC_COMPLETED when done;
 * Adc_ReadGroup copies the result. Hardware: STM32L552 ADC1 (CR.ADSTART, ISR.EOC, DR). Under Renode
 * ADC1 is modelled by Analog.STM32L5_ADC (stm32l552.repl); the wheel-speed input is channel 6 (PA1), driven in the .resc via the ramn "wheel" potentiometer.
 * Owner: agent B.
 */
#ifndef ADC_H
#define ADC_H

#include "Std_Types.h"
#include "Adc_Cfg.h"          /* generated: AdcConf_AdcGroup_* ids */

typedef uint8   Adc_GroupType;
typedef uint8   Adc_ChannelType;
typedef uint16  Adc_ValueGroupType;          /* 12-bit right aligned result */
typedef uint8   Adc_NumberOfValuesType;

typedef enum { ADC_IDLE = 0, ADC_BUSY, ADC_COMPLETED, ADC_STREAM_COMPLETED } Adc_StatusType;

typedef struct {
    Adc_ChannelType channel;                 /* ADC1 regular channel number (IN0..IN18) */
    uint8           sampleTime;              /* SMPR code */
} Adc_GroupConfigType;

typedef struct {
    uint8                      numGroups;
    const Adc_GroupConfigType *groups;
} Adc_ConfigType;
extern const Adc_ConfigType Adc_Config;          /* generated Adc_Cfg.c */

void           Adc_Init(const Adc_ConfigType *ConfigPtr);
void           Adc_DeInit(void);
Std_ReturnType Adc_SetupResultBuffer(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr);
void           Adc_StartGroupConversion(Adc_GroupType Group);
void           Adc_StopGroupConversion(Adc_GroupType Group);
Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr);
Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group);
void           Adc_GetVersionInfo(Std_VersionInfoType *versioninfo);

#endif /* ADC_H */
