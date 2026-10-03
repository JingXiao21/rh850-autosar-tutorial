/*
 * Mcu.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: MCU Driver (AUTOSAR_CP_SWS_MCUDriver). Implemented subset: Mcu_Init,
 * Mcu_InitClock, Mcu_GetPllStatus, Mcu_DistributePllClock, Mcu_GetResetReason, Mcu_PerformReset,
 * Mcu_GetVersionInfo. Not implemented: RAM sections init, modes/low power (Mcu_SetMode is a stub).
 * Hardware: STM32L552 RCC/PWR (target); host: no-op model. MCAL layer, called by EcuM only.
 * Owner: agent B.
 */
#ifndef MCU_H
#define MCU_H

#include "Std_Types.h"
#include "Mcu_Cfg.h"          /* generated: McuConf_McuClockSettingConfig_* ids */

typedef uint8  Mcu_ClockType;
typedef uint8  Mcu_ModeType;
typedef uint32 Mcu_RawResetType;
typedef enum { MCU_PLL_LOCKED = 0, MCU_PLL_UNLOCKED, MCU_PLL_STATUS_UNDEFINED } Mcu_PllStatusType;
typedef enum { MCU_POWER_ON_RESET = 0, MCU_WATCHDOG_RESET, MCU_SW_RESET, MCU_RESET_UNDEFINED } Mcu_ResetType;

typedef struct {
    uint32 sysClockHz;        /* McuClockReferencePointFrequency / target SYSCLK, 80 MHz */
    uint8  clockSettings;     /* number of clock settings (1) */
} Mcu_ConfigType;
extern const Mcu_ConfigType Mcu_Config;          /* generated Mcu_Cfg.c */

void              Mcu_Init(const Mcu_ConfigType *ConfigPtr);
Std_ReturnType    Mcu_InitClock(Mcu_ClockType ClockSetting);     /* target: MSI -> PLL 80 MHz, flash latency; host: no-op */
Std_ReturnType    Mcu_DistributePllClock(void);
Mcu_PllStatusType Mcu_GetPllStatus(void);
Mcu_ResetType     Mcu_GetResetReason(void);
Mcu_RawResetType  Mcu_GetResetRawValue(void);
void              Mcu_PerformReset(void);
void              Mcu_SetMode(Mcu_ModeType McuMode);
void              Mcu_GetVersionInfo(Std_VersionInfoType *versioninfo);
uint32            Mcu_GetSystemClockHz(void);                    /* educational helper */

#endif /* MCU_H */
