#ifndef RH850_MMIO_H
#define RH850_MMIO_H

#include <stdint.h>

/* Injected bus operations let host tests check access width and ordering.
 * Calls must be serialized by the integration layer. Native accesses require
 * supervisor access and a correctly initialized target platform. */
typedef struct {
    void *context;
    uint8_t (*read8)(void *, uintptr_t);
    uint32_t (*read32)(void *, uintptr_t);
    void (*write8)(void *, uintptr_t, uint8_t);
    void (*write16)(void *, uintptr_t, uint16_t);
    void (*write32)(void *, uintptr_t, uint32_t);
} Rh850_Mmio;

extern const Rh850_Mmio Rh850_NativeMmio;
#endif
