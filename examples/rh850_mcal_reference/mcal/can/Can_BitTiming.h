#ifndef RH850_CAN_BIT_TIMING_H
#define RH850_CAN_BIT_TIMING_H
#include <stdint.h>
#include <stdbool.h>

/* Physical Tq counts and divider, NOT register-encoded values. */
typedef struct {
    uint16_t divider, tseg1, tseg2, sjw;
} Can_Timing;
typedef struct {
    uint32_t nominal_bps, data_bps, ncfg, dcfg;
    uint16_t nominal_sample_permyriad, data_sample_permyriad;
} Can_FdTimingResult;

/* P1M-E CAN FD interface only; not the classical CAN register layout.
 * Conservative policy: SJW < TSEG2 per Fig.17.17, although register text
 * also describes <=. See docs/hardware-findings.md for this discrepancy.
 * TDC requires a divider <= 2. No MMIO is performed. */
bool Can_ComputeFdTiming(uint32_t fcan_hz, const Can_Timing *nominal,
                        const Can_Timing *data, bool tdc_enabled,
                        Can_FdTimingResult *result);
#endif
