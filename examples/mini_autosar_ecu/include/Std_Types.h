/*
 * Std_Types.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Std_Types.h (AUTOSAR_SWS_StandardTypes). Platform_Types.h and
 * Compiler.h are included as in the real standard.
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"
#include "Compiler.h"

/* [AUTOSAR API] Std_ReturnType: E_OK/E_NOT_OK, values >= 0x02 are module specific. */
typedef uint8 Std_ReturnType;

#ifndef E_OK
#define E_OK     ((Std_ReturnType)0x00u)
#endif
#ifndef E_NOT_OK
#define E_NOT_OK ((Std_ReturnType)0x01u)
#endif

#define STD_HIGH   0x01u
#define STD_LOW    0x00u
#define STD_ACTIVE 0x01u
#define STD_IDLE   0x00u
#define STD_ON     0x01u
#define STD_OFF    0x00u

typedef struct {
    uint16 vendorID;
    uint16 moduleID;
    uint8  sw_major_version;
    uint8  sw_minor_version;
    uint8  sw_patch_version;
} Std_VersionInfoType;

#endif /* STD_TYPES_H */
