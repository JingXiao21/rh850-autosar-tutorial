#ifndef RH850_OSTM_H
#define RH850_OSTM_H
#include "Rh850_Mmio.h"
#include <stdbool.h>

/* P1M-E manual R01UH0585EJ0120, pp.1543,1551-1567.
 * Only OSTM0/1: OSTM3..7 use FEINT and need a different integration.
 * These are reference low-level APIs, not AUTOSAR Gpt APIs or RTA callbacks. */
typedef enum { OSTM_OK, OSTM_INVALID, OSTM_BUSY, OSTM_TIMEOUT } Ostm_Result;
typedef enum { OSTM_INTERVAL = 0, OSTM_FREE_RUNNING = 2 } Ostm_Mode;

/* Caller owns channel, masks its interrupt, keeps TSST low, and serializes
 * all operations. Requires stopped timer. Selects PCLK (80 MHz per manual),
 * disables timer output and interrupt-at-start. Does not touch EIC/OS. */
Ostm_Result Ostm_InitPclk(const Rh850_Mmio *io, uint8_t unit,
                         Ostm_Mode mode, uint32_t compare);
Ostm_Result Ostm_Start(const Rh850_Mmio *io, uint8_t unit);
/* Stopping resets the time origin on the next free-running start.
 * Do not use this as an OS alarm cancellation implementation. */
Ostm_Result Ostm_Stop(const Rh850_Mmio *io, uint8_t unit, uint32_t poll_limit);
Ostm_Result Ostm_SetCompare(const Rh850_Mmio *io, uint8_t unit, uint32_t compare);
Ostm_Result Ostm_ReadCounter(const Rh850_Mmio *io, uint8_t unit, uint32_t *value);
/* Exact conversion only; rejects non-integral periods and overflow.
 * This helper does not establish the actual clock source. */
bool Ostm_IntervalCompare(uint32_t counter_hz, uint32_t period_us, uint32_t *compare);
#endif
