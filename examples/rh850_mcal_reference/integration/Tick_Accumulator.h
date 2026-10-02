#ifndef RH850_TICK_ACCUMULATOR_H
#define RH850_TICK_ACCUMULATOR_H
#include <stdint.h>
#include <stdbool.h>

/* Sample a free-running UP-counter at intervals strictly shorter than one
 * hardware wrap. Caller must serialize updates and reads, and reinitialize
 * after a timer reset. This does not implement the RTA-OS callback contract. */
typedef struct {
    uint32_t last_raw, counts_per_tick, remainder;
    uint16_t os_tick;
} Tick_Accumulator;
bool Tick_Init(Tick_Accumulator *state, uint32_t counter_hz, uint32_t raw_now);
bool Tick_Update(Tick_Accumulator *state, uint32_t raw_now, uint16_t *os_tick);
#endif
