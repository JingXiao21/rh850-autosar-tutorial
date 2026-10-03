/*
 * SimMmio.h  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart. Register-file simulator behind Mmio_Read32/Mmio_Write32 on the host build.
 * Unmapped addresses behave as RAM (read-back of what was written, 0 initially) so drivers that
 * touch registers the models do not care about (RCC, clock enables) just work.
 * Owner: agent B (sim/host/SimMmio.c and the peripheral models Sim*.c).
 */
#ifndef SIMMMIO_H
#define SIMMMIO_H

#include "Std_Types.h"

typedef uint32 (*SimMmio_ReadFn)(uint32 offset, void *ctx);
typedef void   (*SimMmio_WriteFn)(uint32 offset, uint32 value, void *ctx);

/* Map [base, base+size) to a peripheral model. Later registrations win on overlap. */
void   SimMmio_Register(uint32 base, uint32 size, SimMmio_ReadFn rd, SimMmio_WriteFn wr, void *ctx);
void   SimMmio_Reset(void);                         /* clear register file + registrations */

/* Models registered by SimPeripherals_Init() (agent B): GPIO A..H, ADC1, RCC (plain RAM). */
void   SimPeripherals_Init(void);

/* Test / scenario access to model state (used by HostMain, tests and the scripted scenario) */
void   SimAdc_SetRaw(uint16 raw);                   /* value returned by ADC1->DR for the wheel-speed channel */
uint16 SimAdc_ProfileRaw(uint32 timeMs);            /* the scenario profile, see DESIGN 11.1 (same as Renode script) */
boolean SimDio_GetOutput(uint16 channel);           /* channel = port*16+pin; level of ODR bit */
void   SimDio_SetInput(uint16 channel, boolean level);

/* ---- additions by agent B (backward compatible) ---- */
void   SimAdc_ClearOverride(void);                  /* back from a fixed SimAdc_SetRaw value to the time based profile (default) */
void   SimAdc_SetChannelRaw(uint8 channel, uint16 raw);   /* fixed value for any ADC1 channel 0..18 (wheel speed = 6) */
uint32 SimAdc_GetConversionCount(void);             /* number of ADSTART conversions executed (test helper) */

#endif /* SIMMMIO_H */
