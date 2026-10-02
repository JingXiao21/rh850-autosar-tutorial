#include "Rh850_Mmio.h"
#include <stddef.h>

static uint8_t native_read8(void *context, uintptr_t address)
{ (void)context; return *(volatile uint8_t *)address; }
static uint32_t native_read32(void *context, uintptr_t address)
{ (void)context; return *(volatile uint32_t *)address; }
static void native_write8(void *context, uintptr_t address, uint8_t value)
{ (void)context; *(volatile uint8_t *)address = value; }
static void native_write16(void *context, uintptr_t address, uint16_t value)
{ (void)context; *(volatile uint16_t *)address = value; }
static void native_write32(void *context, uintptr_t address, uint32_t value)
{ (void)context; *(volatile uint32_t *)address = value; }

const Rh850_Mmio Rh850_NativeMmio = {
    NULL, native_read8, native_read32,
    native_write8, native_write16, native_write32
};
