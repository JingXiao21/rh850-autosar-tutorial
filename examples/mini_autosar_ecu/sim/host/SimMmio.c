/*
 * SimMmio.c   (HOST BUILD ONLY: located in sim/host/)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none. On the target the MCAL touches registers through volatile pointers (Mmio.h); on the host build
 * Mmio_Read32/Mmio_Write32 are ordinary functions that land here. This file is the REGISTER FILE:
 *   - a small table of address windows, each owned by a behavioural peripheral model (SimPeripherals.c: GPIOx, ADC1, RCC), and
 *   - a fallback "plain RAM" for every other address (read back what was written, 0 initially), so drivers that touch registers
 *     the models do not care about (FLASH_ACR, SCB_AIRCR, ...) just work.
 * Addresses are the real 32-bit STM32L552 addresses; the host never dereferences them.
 * Implements sim/include/SimMmio.h (SimMmio_Register, SimMmio_Reset) and the host half of mcal/mmio/Mmio.h.
 * Owner: agent B.
 */
#include "SimMmio.h"
#include "Mmio.h"

#define SIMMMIO_MAX_REGIONS   24u
#define SIMMMIO_RAM_WORDS     512u       /* fallback RAM: open addressing table, plenty for the handful of unmodelled registers */

typedef struct {
    uint32          base;
    uint32          size;
    SimMmio_ReadFn  rd;
    SimMmio_WriteFn wr;
    void           *ctx;
} SimMmio_Region;

typedef struct {
    uint32  addr;
    uint32  value;
    boolean used;
} SimMmio_RamWord;

static SimMmio_Region  s_regions[SIMMMIO_MAX_REGIONS];
static uint32          s_numRegions;
static SimMmio_RamWord s_ram[SIMMMIO_RAM_WORDS];

void SimMmio_Reset(void)
{
    uint32 i;
    s_numRegions = 0u;
    for (i = 0u; i < SIMMMIO_RAM_WORDS; i++) {
        s_ram[i].used  = FALSE;
        s_ram[i].addr  = 0u;
        s_ram[i].value = 0u;
    }
}

void SimMmio_Register(uint32 base, uint32 size, SimMmio_ReadFn rd, SimMmio_WriteFn wr, void *ctx)
{
    if (s_numRegions < SIMMMIO_MAX_REGIONS) {
        s_regions[s_numRegions].base = base;
        s_regions[s_numRegions].size = size;
        s_regions[s_numRegions].rd   = rd;
        s_regions[s_numRegions].wr   = wr;
        s_regions[s_numRegions].ctx  = ctx;
        s_numRegions++;
    }
}

/* later registrations win on overlap: search from the newest */
static const SimMmio_Region *simmmio_find(uint32 addr)
{
    uint32 i;
    for (i = s_numRegions; i > 0u; i--) {
        const SimMmio_Region *r = &s_regions[i - 1u];
        if ((addr >= r->base) && ((addr - r->base) < r->size)) {
            return r;
        }
    }
    return NULL_PTR;
}

static SimMmio_RamWord *simmmio_ram(uint32 addr, boolean create)
{
    uint32 h = (addr >> 2) % SIMMMIO_RAM_WORDS;
    uint32 n;
    for (n = 0u; n < SIMMMIO_RAM_WORDS; n++) {
        SimMmio_RamWord *w = &s_ram[(h + n) % SIMMMIO_RAM_WORDS];
        if (w->used && (w->addr == addr)) {
            return w;
        }
        if (!w->used) {
            if (create) {
                w->used = TRUE;
                w->addr = addr;
                w->value = 0u;
                return w;
            }
            return NULL_PTR;
        }
    }
    return NULL_PTR;                          /* table full: writes are dropped, reads give 0 */
}

uint32 Mmio_Read32(uint32 addr)
{
    const SimMmio_Region *r = simmmio_find(addr);
    if ((r != NULL_PTR) && (r->rd != NULL_PTR)) {
        return r->rd(addr - r->base, r->ctx);
    } else {
        SimMmio_RamWord *w = simmmio_ram(addr & ~3u, FALSE);
        return (w != NULL_PTR) ? w->value : 0u;
    }
}

void Mmio_Write32(uint32 addr, uint32 val)
{
    const SimMmio_Region *r = simmmio_find(addr);
    if ((r != NULL_PTR) && (r->wr != NULL_PTR)) {
        r->wr(addr - r->base, val, r->ctx);
    } else {
        SimMmio_RamWord *w = simmmio_ram(addr & ~3u, TRUE);
        if (w != NULL_PTR) {
            w->value = val;
        }
    }
}
