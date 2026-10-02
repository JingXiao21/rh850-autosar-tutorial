#include "Ostm.h"
#include "Can_BitTiming.h"
#include "Tick_Accumulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define CHECK(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
    exit(EXIT_FAILURE); } } while (0)

typedef struct {
    uintptr_t address;
    uint32_t value;
    unsigned width;
} Write;
typedef struct {
    uint8_t enabled[2], mode[2];
    uint32_t compare[2], counter[2];
    Write writes[32];
    unsigned count, te_reads;
    bool stuck;
} Bus;

static unsigned unit_for(uintptr_t address)
{
    if (address >= UINT32_C(0xFFDD8000) && address <= UINT32_C(0xFFDD8020)) return 0U;
    CHECK(address >= UINT32_C(0xFFDD9000) && address <= UINT32_C(0xFFDD9020));
    return 1U;
}
static void record(Bus *b, uintptr_t a, uint32_t v, unsigned width)
{
    CHECK(b->count < 32U);
    b->writes[b->count].address = a;
    b->writes[b->count].value = v;
    b->writes[b->count++].width = width;
}
static uint8_t read8(void *p, uintptr_t a)
{
    Bus *b = p;
    CHECK((a & 0xFFFU) == 0x10U);
    b->te_reads++;
    return b->enabled[unit_for(a)];
}
static uint32_t read32(void *p, uintptr_t a)
{
    Bus *b = p;
    CHECK((a & 0xFFFU) == 4U);
    return b->counter[unit_for(a)];
}
static void write8(void *p, uintptr_t a, uint8_t value)
{
    Bus *b = p;
    unsigned u = unit_for(a);
    record(b, a, value, 8U);
    switch (a & 0xFFFU) {
    case 0x0C: CHECK(value == 0U); break;
    case 0x14:
        CHECK(value == 1U);
        b->enabled[u] = 1U;
        b->counter[u] = b->mode[u] == 2U ? 0U : b->compare[u];
        break;
    case 0x18:
        CHECK(value == 1U);
        if (!b->stuck) b->enabled[u] = 0U;
        break;
    case 0x20:
        CHECK(b->enabled[u] == 0U);
        CHECK(value == 0U || value == 2U);
        b->mode[u] = value;
        break;
    default: CHECK(false);
    }
}
static void write16(void *p, uintptr_t a, uint16_t value)
{
    Bus *b = p;
    CHECK(a == UINT32_C(0xFFDD6000) || a == UINT32_C(0xFFDD6004));
    CHECK(b->enabled[a == UINT32_C(0xFFDD6000) ? 0U : 1U] == 0U);
    CHECK(value == 0U);
    record(b, a, value, 16U);
}
static void write32(void *p, uintptr_t a, uint32_t value)
{
    Bus *b = p;
    CHECK((a & 0xFFFU) == 0U);
    b->compare[unit_for(a)] = value;
    record(b, a, value, 32U);
}
static Rh850_Mmio bus_ops(Bus *b)
{
    Rh850_Mmio io = {b, read8, read32, write8, write16, write32};
    return io;
}

static void test_ostm_sequence_and_ownership(void)
{
    Bus b = {0};
    Rh850_Mmio io = bus_ops(&b);
    uint32_t now;
    CHECK(Ostm_InitPclk(&io, 1U, OSTM_INTERVAL, 79999U) == OSTM_OK);
    CHECK(b.count == 4U);
    CHECK(b.writes[0].address == UINT32_C(0xFFDD6004) && b.writes[0].width == 16U);
    CHECK(b.writes[1].address == UINT32_C(0xFFDD900C) && b.writes[1].width == 8U);
    CHECK(b.writes[2].address == UINT32_C(0xFFDD9000) && b.writes[2].width == 32U);
    CHECK(b.writes[2].value == 79999U);
    CHECK(b.writes[3].address == UINT32_C(0xFFDD9020) && b.writes[3].width == 8U);
    CHECK(Ostm_Start(&io, 1U) == OSTM_OK);
    CHECK(Ostm_ReadCounter(&io, 1U, &now) == OSTM_OK && now == 79999U);
    CHECK(Ostm_Start(&io, 1U) == OSTM_BUSY);
    CHECK(Ostm_InitPclk(&io, 1U, OSTM_FREE_RUNNING, 123U) == OSTM_BUSY);
    CHECK(b.count == 5U); /* Busy calls must not disturb existing timer. */
    CHECK(Ostm_Stop(&io, 1U, 4U) == OSTM_OK);
    CHECK(Ostm_InitPclk(&io, 1U, OSTM_FREE_RUNNING, 900U) == OSTM_OK);
    CHECK(Ostm_Start(&io, 1U) == OSTM_OK);
    b.counter[1] = 901U;
    CHECK(Ostm_SetCompare(&io, 1U, 899U) == OSTM_OK);
    CHECK(b.enabled[1] == 1U && b.counter[1] == 901U);
    CHECK(Ostm_ReadCounter(&io, 1U, &now) == OSTM_OK && now == 901U);
    CHECK(Ostm_Stop(&io, 1U, 4U) == OSTM_OK);
    CHECK(Ostm_Start(&io, 1U) == OSTM_OK);
    CHECK(Ostm_ReadCounter(&io, 1U, &now) == OSTM_OK && now == 0U);
    CHECK(b.enabled[0] == 0U && b.compare[0] == 0U);
}

static void test_ostm_invalid_and_timeout(void)
{
    Bus b = {0};
    Rh850_Mmio io = bus_ops(&b);
    unsigned reads;
    CHECK(Ostm_InitPclk(&io, 2U, OSTM_INTERVAL, 1U) == OSTM_INVALID);
    CHECK(Ostm_InitPclk(&io, 0U, (Ostm_Mode)3, 1U) == OSTM_INVALID);
    CHECK(Ostm_Start(NULL, 0U) == OSTM_INVALID);
    CHECK(Ostm_ReadCounter(&io, 0U, NULL) == OSTM_INVALID);
    CHECK(Ostm_Stop(&io, 0U, 0U) == OSTM_INVALID);
    CHECK(b.count == 0U && b.te_reads == 0U);
    CHECK(Ostm_InitPclk(&io, 0U, OSTM_FREE_RUNNING, 1U) == OSTM_OK);
    CHECK(Ostm_Start(&io, 0U) == OSTM_OK);
    b.stuck = true;
    reads = b.te_reads;
    CHECK(Ostm_Stop(&io, 0U, 3U) == OSTM_TIMEOUT);
    CHECK(b.te_reads == reads + 3U && b.enabled[0] == 1U);
}

static void test_interval_conversion(void)
{
    uint32_t cmp = 77U;
    CHECK(Ostm_IntervalCompare(80000000U, 1000U, &cmp) && cmp == 79999U);
    CHECK(Ostm_IntervalCompare(20000000U, 1000U, &cmp) && cmp == 19999U);
    CHECK(Ostm_IntervalCompare(1000000U, 1U, &cmp) && cmp == 0U);
    CHECK(Ostm_IntervalCompare(2000000U, UINT32_C(2147483648), &cmp) && cmp == UINT32_MAX);
    CHECK(!Ostm_IntervalCompare(2000000U, UINT32_C(2147483649), &cmp));
    CHECK(!Ostm_IntervalCompare(32768U, 1000U, &cmp));
    CHECK(!Ostm_IntervalCompare(0U, 1000U, &cmp));
    CHECK(!Ostm_IntervalCompare(UINT32_MAX, UINT32_MAX, &cmp));
    CHECK(!Ostm_IntervalCompare(80000000U, 1000U, NULL));
}

static void test_can_timing(void)
{
    Can_Timing n = {2U, 31U, 8U, 4U};
    Can_Timing d = {2U, 15U, 4U, 3U};
    Can_FdTimingResult r = {0};
    CHECK(Can_ComputeFdTiming(40000000U, &n, &d, true, &r));
    CHECK(r.nominal_bps == 500000U && r.data_bps == 1000000U);
    CHECK(r.nominal_sample_permyriad == 8000U && r.data_sample_permyriad == 8000U);
    CHECK(r.ncfg == UINT32_C(0x071E1801));
    CHECK(r.dcfg == UINT32_C(0x023E0001));
    printf("40MHz candidate: NCFG=0x%08" PRIX32 " DCFG=0x%08" PRIX32 "\n", r.ncfg, r.dcfg);
    CHECK(!Can_ComputeFdTiming(80000000U, &n, &d, false, &r));
    CHECK(!Can_ComputeFdTiming(40000000U, NULL, &d, false, &r));
    CHECK(!Can_ComputeFdTiming(40000000U, &n, &d, false, NULL));
    d.sjw = 4U; /* Explicitly rejected by conservative strict timing policy. */
    CHECK(!Can_ComputeFdTiming(40000000U, &n, &d, false, &r));
    d.sjw = 3U; d.divider = 1U;
    CHECK(!Can_ComputeFdTiming(40000000U, &n, &d, false, &r));
    n.divider = d.divider = 4U;
    CHECK(!Can_ComputeFdTiming(40000000U, &n, &d, true, &r));
    CHECK(Can_ComputeFdTiming(40000000U, &n, &d, false, &r));
    n.divider = d.divider = 2U;
    n.tseg1 = 12U; n.tseg2 = 3U; n.sjw = 2U;
    d.tseg1 = 5U; d.tseg2 = 2U; d.sjw = 1U;
    CHECK(Can_ComputeFdTiming(16000000U, &n, &d, false, &r));
    CHECK(r.nominal_bps == 500000U && r.data_bps == 1000000U);
    CHECK(r.nominal_sample_permyriad == 8125U && r.data_sample_permyriad == 7500U);
    n.tseg1 = 13U; /* Would require rounding the requested bitrate. */
    CHECK(!Can_ComputeFdTiming(16000000U, &n, &d, false, &r));
    n.tseg1 = UINT16_MAX;
    CHECK(!Can_ComputeFdTiming(40000000U, &n, &d, false, &r));
}

static void test_tick_rollovers(void)
{
    Tick_Accumulator state = {0};
    uint16_t tick;
    uint32_t raw = UINT32_MAX - 40000U;
    uint64_t total = 0U;
    uint32_t random = 12345U;
    unsigned i;
    CHECK(!Tick_Update(&state, 0U, &tick));
    CHECK(!Tick_Init(&state, 32768U, 0U));
    CHECK(Tick_Init(&state, 80000000U, raw));
    for (i = 0U; i < 100000U; ++i) {
        uint32_t delta;
        random = random * 1664525U + 1013904223U;
        delta = random % 16000001U;
        raw += delta;
        total += delta;
        CHECK(Tick_Update(&state, raw, &tick));
        CHECK(tick == (uint16_t)(total / 80000U));
        CHECK(state.remainder == (uint32_t)(total % 80000U));
    }
    CHECK(Tick_Update(&state, raw, &tick)); /* Repeated read does not advance. */
    CHECK(tick == (uint16_t)(total / 80000U));
    CHECK(Tick_Init(&state, 20000000U, UINT32_MAX - 9999U));
    CHECK(Tick_Update(&state, 0U, &tick) && tick == 0U);
    CHECK(Tick_Update(&state, 10000U, &tick) && tick == 1U);
    state.os_tick = UINT16_MAX;
    CHECK(Tick_Update(&state, 30000U, &tick) && tick == 0U);
    CHECK(!Tick_Update(&state, 30000U, NULL));
}

int main(void)
{
    test_ostm_sequence_and_ownership();
    test_ostm_invalid_and_timeout();
    test_interval_conversion();
    test_can_timing();
    test_tick_rollovers();
    puts("PASS: 5 test groups; 100000 tick samples across hardware/OS rollovers.");
    return EXIT_SUCCESS;
}
