#include "Ostm.h"
#include <stddef.h>

static const uintptr_t base[2] = {UINT32_C(0xFFDD8000), UINT32_C(0xFFDD9000)};
static const uintptr_t clock_select[2] = {UINT32_C(0xFFDD6000), UINT32_C(0xFFDD6004)};
enum { CMP = 0x00, CNT = 0x04, TOE = 0x0C, TE = 0x10,
       TS = 0x14, TT = 0x18, CTL = 0x20 };

static bool valid(const Rh850_Mmio *io, uint8_t unit)
{
    return unit < 2U && io != NULL && io->read8 != NULL &&
           io->read32 != NULL && io->write8 != NULL &&
           io->write16 != NULL && io->write32 != NULL;
}

Ostm_Result Ostm_InitPclk(const Rh850_Mmio *io, uint8_t unit,
                         Ostm_Mode mode, uint32_t compare)
{
    if (!valid(io, unit) || (mode != OSTM_INTERVAL && mode != OSTM_FREE_RUNNING))
        return OSTM_INVALID;
    if ((io->read8(io->context, base[unit] + TE) & 1U) != 0U)
        return OSTM_BUSY;
    /* IC0TMEN=0 selects PCLK. Reserved fields are written with reset values.
     * Never use a 32-bit write to this 16-bit clock register. */
    io->write16(io->context, clock_select[unit], 0U);
    io->write8(io->context, base[unit] + TOE, 0U);
    io->write32(io->context, base[unit] + CMP, compare);
    io->write8(io->context, base[unit] + CTL, (uint8_t)mode);
    return OSTM_OK;
}

Ostm_Result Ostm_Start(const Rh850_Mmio *io, uint8_t unit)
{
    if (!valid(io, unit)) return OSTM_INVALID;
    if ((io->read8(io->context, base[unit] + TE) & 1U) != 0U)
        return OSTM_BUSY;
    io->write8(io->context, base[unit] + TS, 1U);
    return OSTM_OK;
}

Ostm_Result Ostm_Stop(const Rh850_Mmio *io, uint8_t unit, uint32_t poll_limit)
{
    uint32_t i;
    if (!valid(io, unit) || poll_limit == 0U) return OSTM_INVALID;
    io->write8(io->context, base[unit] + TT, 1U);
    for (i = 0U; i < poll_limit; ++i) {
        if ((io->read8(io->context, base[unit] + TE) & 1U) == 0U)
            return OSTM_OK;
    }
    return OSTM_TIMEOUT;
}

Ostm_Result Ostm_SetCompare(const Rh850_Mmio *io, uint8_t unit, uint32_t compare)
{
    if (!valid(io, unit)) return OSTM_INVALID;
    /* Does not clear/force pending IRQs or handle a passed deadline. Those
     * operations belong to the OS integration layer and its critical section. */
    io->write32(io->context, base[unit] + CMP, compare);
    return OSTM_OK;
}

Ostm_Result Ostm_ReadCounter(const Rh850_Mmio *io, uint8_t unit, uint32_t *value)
{
    if (!valid(io, unit) || value == NULL) return OSTM_INVALID;
    *value = io->read32(io->context, base[unit] + CNT);
    return OSTM_OK;
}

bool Ostm_IntervalCompare(uint32_t counter_hz, uint32_t period_us, uint32_t *compare)
{
    uint64_t product = (uint64_t)counter_hz * period_us;
    uint64_t counts = product / UINT64_C(1000000);
    if (compare == NULL || product % UINT64_C(1000000) != 0U ||
        counts == 0U || counts > (UINT64_C(1) << 32)) return false;
    *compare = (uint32_t)(counts - 1U);
    return true;
}
