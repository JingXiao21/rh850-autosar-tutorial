/*
 * UdsTrace.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none. Closest real-world equivalents are a
 * debugger trace (Lauterbach/GHS MULTI), a DLT/log buffer, or a CAN bus trace
 * (CANoe). It exists so that every hop of a UDS request through
 * Can -> CanIf -> CanTp -> PduR -> Dcm -> Rte -> SWC can be *read* as text.
 *
 * Compile-time switch: UDS_TRACE_ENABLE (default 1).
 * Runtime switch:      UdsTrace_SetEnabled() (host tests turn it off).
 */
#ifndef UDS_TRACE_H
#define UDS_TRACE_H

#include "Std_Types.h"
#include <stdio.h>

#ifndef UDS_TRACE_ENABLE
#define UDS_TRACE_ENABLE 1
#endif

void UdsTrace_SetEnabled(boolean enabled);
boolean UdsTrace_IsEnabled(void);
void UdsTrace_SetSink(FILE *sink);

#if defined(__GNUC__)
void UdsTrace_Log(const char *module, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void UdsTrace_Log(const char *module, const char *fmt, ...);
#endif

/* Formats bytes as "22 F1 90". Uses a small ring of static buffers so that
 * up to 4 results can be used in one printf call. Max 64 bytes shown. */
const char *UdsTrace_Hex(const uint8 *data, uint16 length);

#if UDS_TRACE_ENABLE
#define UDS_TRACE(module, ...) UdsTrace_Log((module), __VA_ARGS__)
#else
/* "if (0)" keeps arguments type-checked and avoids unused-variable warnings. */
#define UDS_TRACE(module, ...) do { if (0) { UdsTrace_Log((module), __VA_ARGS__); } } while (0)
#endif

#endif /* UDS_TRACE_H */
