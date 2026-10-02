/*
 * Std_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Std_Types.h (AUTOSAR_SWS_StandardTypes) together
 * with Platform_Types.h / Compiler.h supplied by the MCAL vendor for the
 * target compiler (e.g. GHS for RH850).
 *
 * Simplification: Platform_Types.h is folded in here and mapped onto <stdint.h>
 * so the stack builds with any host C99 compiler. Compiler abstraction macros
 * (FUNC(), P2VAR(), ...) are intentionally omitted to keep the call chain
 * readable; real generated code uses them everywhere.
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ---- Platform_Types.h equivalents ------------------------------------- */
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;
typedef uint8    boolean;

#ifndef TRUE
#define TRUE  ((boolean)1u)
#endif
#ifndef FALSE
#define FALSE ((boolean)0u)
#endif

#define NULL_PTR ((void *)0)

/* ---- Std_Types.h ------------------------------------------------------- */
/* [AUTOSAR API] Std_ReturnType is uint8; values >= 0x02 are module specific
 * extensions (e.g. CAN_BUSY = 0x02, DCM_E_PENDING = 10). */
typedef uint8 Std_ReturnType;

#define E_OK     ((Std_ReturnType)0x00u)
#define E_NOT_OK ((Std_ReturnType)0x01u)

#define STD_HIGH 0x01u
#define STD_LOW  0x00u
#define STD_ON   0x01u
#define STD_OFF  0x00u

typedef struct {
    uint16 vendorID;
    uint16 moduleID;
    uint8  sw_major_version;
    uint8  sw_minor_version;
    uint8  sw_patch_version;
} Std_VersionInfoType;

#endif /* STD_TYPES_H */
