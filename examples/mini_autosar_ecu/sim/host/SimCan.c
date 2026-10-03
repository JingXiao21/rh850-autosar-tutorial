/*
 * SimCan.c   (HOST BUILD ONLY: located in sim/host/)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none. The "virtual CAN bus" of the host build (DESIGN 10.3): every host ECU executable owns ONE bus port;
 * buses are connected through FILES instead of IPC (deterministic, scriptable, diffable):
 *   - every frame the ECU transmits is appended to the TX log      "<time_us> <id_hex> <dlc> <byte0_hex> ..."
 *   - frames from the RX script (same format, e.g. the TX log of the other ECU) are injected into the ECU RX FIFO when the virtual time
 *     reaches their timestamp (SimCan_Step, called from the simulator step hook) via CanHwSim_PushRx() in Can_Hw_Sim.c, which raises
 *     the Cat2 RX interrupt (IRQ 39) when the driver enabled RX interrupts.
 * Lines starting with '#' and empty lines are ignored; the script is sorted by time (stable) after loading.
 * Implements sim/include/SimCan.h. Owner: agent B.
 */
#include "SimCan.h"
#include "Can_GeneralTypes.h"
#include <stdio.h>
#include <stdlib.h>

#define SIMCAN_MAX_SCRIPT   16384u

typedef struct {
    uint64 timeUs;
    uint32 id;
    uint8  dlc;
    uint8  data[8];
    uint32 seq;                                    /* original position: keeps the sort stable */
} SimCan_Frame;

static FILE         *s_txLog;
static SimCan_Frame  s_script[SIMCAN_MAX_SCRIPT];
static uint32        s_numFrames;
static uint32        s_nextFrame;
static uint32        s_txCount;
static uint32        s_injected;
static uint32        s_dropped;

static int simcan_cmp(const void *a, const void *b)
{
    const SimCan_Frame *x = (const SimCan_Frame *)a;
    const SimCan_Frame *y = (const SimCan_Frame *)b;
    if (x->timeUs != y->timeUs) { return (x->timeUs < y->timeUs) ? -1 : 1; }
    return (x->seq < y->seq) ? -1 : ((x->seq > y->seq) ? 1 : 0);
}

static void simcan_load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[160];
    if (f == NULL) {
        return;                                    /* a missing script = silent bus (e.g. ECU_A not run yet) */
    }
    while ((fgets(line, (int)sizeof(line), f) != NULL) && (s_numFrames < SIMCAN_MAX_SCRIPT)) {
        unsigned long long t;
        unsigned id, dlc, b[8];
        int n, i;
        char *p = line;
        while ((*p == ' ') || (*p == '\t')) { p++; }
        if ((*p == '#') || (*p == '\n') || (*p == '\r') || (*p == '\0')) {
            continue;
        }
        n = sscanf(p, "%llu %x %u %x %x %x %x %x %x %x %x", &t, &id, &dlc, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]);
        if ((n < 3) || (dlc > 8u) || (n < (int)(3u + dlc))) {
            continue;                              /* malformed line */
        }
        s_script[s_numFrames].timeUs = (uint64)t;
        s_script[s_numFrames].id     = (uint32)id & CAN_ID_VALUE_MASK_STD;
        s_script[s_numFrames].dlc    = (uint8)dlc;
        for (i = 0; i < 8; i++) {
            s_script[s_numFrames].data[i] = (i < (int)dlc) ? (uint8)b[i] : 0u;
        }
        s_script[s_numFrames].seq = s_numFrames;
        s_numFrames++;
    }
    fclose(f);
    qsort(s_script, s_numFrames, sizeof(s_script[0]), simcan_cmp);
}

void SimCan_Init(const char *txLogPath, const char *rxScriptPath)
{
    SimCan_Deinit();
    s_numFrames = s_nextFrame = s_txCount = s_injected = s_dropped = 0u;
    if (txLogPath != NULL_PTR) {
        s_txLog = fopen(txLogPath, "w");
        if (s_txLog != NULL) {
            (void)fprintf(s_txLog, "# time_us id dlc data...   (SimCan TX log = RX script format)\n");
            (void)fflush(s_txLog);
        }
    }
    if (rxScriptPath != NULL_PTR) {
        simcan_load_script(rxScriptPath);
    }
}

void SimCan_Deinit(void)
{
    if (s_txLog != NULL) {
        (void)fflush(s_txLog);
        (void)fclose(s_txLog);
        s_txLog = NULL;
    }
}

void SimCan_OnTx(uint64 nowUs, uint32 id, uint8 dlc, const uint8 *data)
{
    uint8 i;
    s_txCount++;
    if (s_txLog == NULL) {
        return;
    }
    (void)fprintf(s_txLog, "%llu %x %u", (unsigned long long)nowUs, (unsigned)id, (unsigned)dlc);
    for (i = 0u; i < dlc; i++) {
        (void)fprintf(s_txLog, " %02x", (unsigned)data[i]);
    }
    (void)fprintf(s_txLog, "\n");
    (void)fflush(s_txLog);                         /* keep the log complete even if the OS port exits the process abruptly */
}

boolean SimCan_Inject(uint32 id, uint8 dlc, const uint8 *data)
{
    boolean ok = CanHwSim_PushRx(id, dlc, data);
    if (ok) { s_injected++; } else { s_dropped++; }
    return ok;
}

void SimCan_Step(uint64 nowUs)
{
    while ((s_nextFrame < s_numFrames) && (s_script[s_nextFrame].timeUs <= nowUs)) {
        const SimCan_Frame *fr = &s_script[s_nextFrame];
        (void)SimCan_Inject(fr->id, fr->dlc, fr->data);
        s_nextFrame++;
    }
}

uint32 SimCan_GetTxCount(void)       { return s_txCount; }
uint32 SimCan_GetInjectedCount(void) { return s_injected; }
uint32 SimCan_GetDroppedCount(void)  { return s_dropped; }
uint32 SimCan_GetScriptFrames(void)  { return s_numFrames; }
