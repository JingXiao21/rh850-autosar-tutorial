/*
 * Mmio.h
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart: real MCAL drivers access registers through vendor register structs. This
 * tiny layer is what lets ONE hardware-independent driver source (Dio.c, Adc.c, Port.c, Mcu.c) run
 * on both builds:
 *   - MINI_PLATFORM_TARGET: inline volatile accesses to the real STM32L552 addresses.
 *   - MINI_PLATFORM_HOST  : out-of-line functions implemented by the simulated register file
 *                           (sim/host/SimMmio.c, owner agent B) which dispatches to behavioural
 *                           peripheral models (GPIO, ADC, RCC, ...).
 * Addresses are 32-bit target addresses in BOTH builds (host never dereferences them).
 */
#ifndef MMIO_H
#define MMIO_H

#include "Std_Types.h"
#include "Mini_Cfg.h"

#if defined(MINI_PLATFORM_TARGET)

static inline uint32 Mmio_Read32(uint32 addr)               { return *(volatile uint32 *)(uintptr_t)addr; }
static inline void   Mmio_Write32(uint32 addr, uint32 val)  { *(volatile uint32 *)(uintptr_t)addr = val; }

#else /* MINI_PLATFORM_HOST */

uint32 Mmio_Read32(uint32 addr);
void   Mmio_Write32(uint32 addr, uint32 val);

#endif

/* read-modify-write helper, identical on both builds */
static inline void Mmio_Modify32(uint32 addr, uint32 clearMask, uint32 setMask)
{
    Mmio_Write32(addr, (Mmio_Read32(addr) & ~clearMask) | setMask);
}

#endif /* MMIO_H */
