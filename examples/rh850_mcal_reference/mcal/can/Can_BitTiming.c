#include "Can_BitTiming.h"
#include <stddef.h>

static bool valid(const Can_Timing *t, bool data_phase)
{
    uint32_t total;
    if (t == NULL) return false;
    total = 1U + (uint32_t)t->tseg1 + t->tseg2;
    return t->divider >= 1U && t->divider <= 256U &&
           t->tseg1 >= (data_phase ? 2U : 4U) &&
           t->tseg1 <= (data_phase ? 16U : 128U) &&
           t->tseg2 >= 2U && t->tseg2 <= (data_phase ? 8U : 32U) &&
           t->sjw >= 1U && t->sjw <= (data_phase ? 8U : 32U) &&
           t->tseg1 > t->tseg2 && t->tseg2 > t->sjw &&
           total >= (data_phase ? 5U : 8U) &&
           total <= (data_phase ? 25U : 161U);
}

bool Can_ComputeFdTiming(uint32_t fcan_hz, const Can_Timing *nominal,
                        const Can_Timing *data, bool tdc_enabled,
                        Can_FdTimingResult *result)
{
    uint32_t ntq, dtq, ndiv, ddiv;
    Can_FdTimingResult output;
    if (result == NULL || (fcan_hz != 16000000U && fcan_hz != 40000000U) ||
        !valid(nominal, false) || !valid(data, true) ||
        nominal->divider != data->divider ||
        (tdc_enabled && nominal->divider > 2U)) return false;
    ntq = 1U + (uint32_t)nominal->tseg1 + nominal->tseg2;
    dtq = 1U + (uint32_t)data->tseg1 + data->tseg2;
    ndiv = ntq * nominal->divider;
    ddiv = dtq * data->divider;
    if (fcan_hz % ndiv != 0U || fcan_hz % ddiv != 0U) return false;
    output.nominal_bps = fcan_hz / ndiv;
    output.data_bps = fcan_hz / ddiv;
    if (output.nominal_bps > 1000000U || output.data_bps < output.nominal_bps ||
        output.data_bps > (fcan_hz == 16000000U ? 2000000U : 8000000U)) return false;
    output.nominal_sample_permyriad = (uint16_t)((1U + nominal->tseg1) * 10000U / ntq);
    output.data_sample_permyriad = (uint16_t)((1U + data->tseg1) * 10000U / dtq);
    /* R01UH0585EJ0120 pp.921-922,935-936: physical values encode as N-1. */
    output.ncfg = ((uint32_t)(nominal->tseg2 - 1U) << 24) |
                  ((uint32_t)(nominal->tseg1 - 1U) << 16) |
                  ((uint32_t)(nominal->sjw - 1U) << 11) |
                  (uint32_t)(nominal->divider - 1U);
    output.dcfg = ((uint32_t)(data->sjw - 1U) << 24) |
                  ((uint32_t)(data->tseg2 - 1U) << 20) |
                  ((uint32_t)(data->tseg1 - 1U) << 16) |
                  (uint32_t)(data->divider - 1U);
    *result = output;
    return true;
}
