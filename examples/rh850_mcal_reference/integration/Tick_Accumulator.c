#include "Tick_Accumulator.h"
#include <stddef.h>

bool Tick_Init(Tick_Accumulator *state, uint32_t counter_hz, uint32_t raw_now)
{
    if (state == NULL || counter_hz == 0U || counter_hz % 1000U != 0U) return false;
    state->last_raw = raw_now;
    state->counts_per_tick = counter_hz / 1000U;
    state->remainder = 0U;
    state->os_tick = 0U;
    return true;
}

bool Tick_Update(Tick_Accumulator *state, uint32_t raw_now, uint16_t *os_tick)
{
    uint32_t delta;
    uint64_t total, ticks;
    if (state == NULL || os_tick == NULL || state->counts_per_tick == 0U ||
        state->remainder >= state->counts_per_tick) return false;
    delta = raw_now - state->last_raw; /* Defined unsigned modulo-2^32 difference. */
    total = (uint64_t)delta + state->remainder;
    ticks = total / state->counts_per_tick;
    state->remainder = (uint32_t)(total % state->counts_per_tick);
    state->os_tick = (uint16_t)((uint64_t)state->os_tick + ticks);
    state->last_raw = raw_now;
    *os_tick = state->os_tick;
    return true;
}
