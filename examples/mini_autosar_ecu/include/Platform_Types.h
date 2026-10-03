/*
 * Platform_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Platform_Types.h (AUTOSAR_SWS_PlatformTypes), normally supplied per
 * MCU + compiler by the MCAL vendor. Here: mapped onto <stdint.h>, valid for arm-none-eabi-gcc
 * (Cortex-M33, 32-bit little endian) and host gcc (x86 Windows); only fixed-width types are used.
 * Spec ref: Platform Types SWS is not in the R25-11 text set; per R4.x convention.
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef int8_t    sint8;
typedef int16_t   sint16;
typedef int32_t   sint32;
typedef int64_t   sint64;
typedef uint8_t   boolean;
typedef float     float32;
typedef double    float64;

#ifndef TRUE
#define TRUE  ((boolean)1u)
#endif
#ifndef FALSE
#define FALSE ((boolean)0u)
#endif

#define CPU_TYPE_32        32u
#define MSB_FIRST          0u
#define LSB_FIRST          1u
#define HIGH_BYTE_FIRST    0u
#define LOW_BYTE_FIRST     1u
#define CPU_BIT_ORDER      LSB_FIRST
#define CPU_BYTE_ORDER     LOW_BYTE_FIRST     /* little endian on both build targets */

#endif /* PLATFORM_TYPES_H */
