/*
 * SimCan.h  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart (test infrastructure): the "virtual CAN bus" of the host build. Each host
 * ECU executable has ONE bus port. Instead of IPC between processes, buses are connected through
 * FILES: the TX log of one run is the RX script of the other (see DESIGN 10.3), and a scripted
 * rest-bus (RestBus.h) injects the tester frames. Deterministic, scriptable, diffable.
 *
 * FILE FORMAT (text, one frame per line, '#' starts a comment):
 *     <time_us> <id_hex> <dlc> <byte0_hex> <byte1_hex> ...
 *     e.g.   20000 101 2 e8 03
 * TX log written by SimCan_OnTx() == RX script format read by SimCan_Init().
 *
 * Owner: agent B (sim/host/SimCan.c, sim/host/Can_Hw_Sim.c).
 */
#ifndef SIMCAN_H
#define SIMCAN_H

#include "Std_Types.h"

/* IRQ number used for "FDCAN1 interrupt line 0" on both builds (STM32L5 NVIC IRQ 39). */
#define SIMCAN_IRQ_FDCAN1_IT0   39u

/* txLogPath: file receiving every frame the ECU transmits (NULL_PTR = none).
 * rxScriptPath: frames to inject into the ECU at their timestamps (NULL_PTR = none). */
void    SimCan_Init(const char *txLogPath, const char *rxScriptPath);
void    SimCan_Deinit(void);                                   /* flush/close files */

/* Called by the host Can_Hw backend when the driver transmits a frame at time nowUs. */
void    SimCan_OnTx(uint64 nowUs, uint32 id, uint8 dlc, const uint8 *data);

/* Called by the simulator step hook (HostMain registers it with SimTime_RegisterStepHook): injects every
 * script frame whose timestamp <= nowUs through SimCan_Inject(). */
void    SimCan_Step(uint64 nowUs);

/* Inject a frame from "the bus" into the ECU RX FIFO (rest-bus, script). Raises IRQ 39 when the driver has
 * RX interrupts enabled. Returns FALSE if the host RX FIFO (depth 3, as FDCAN FIFO0) is full (frame lost). */
boolean SimCan_Inject(uint32 id, uint8 dlc, const uint8 *data);

/* Backend side (implemented in Can_Hw_Sim.c, called by SimCan.c): */
boolean CanHwSim_PushRx(uint32 id, uint8 dlc, const uint8 *data);

/* ---- additions by agent B (backward compatible) ---- */
uint32  SimCan_GetTxCount(void);                       /* frames passed to SimCan_OnTx since SimCan_Init */
uint32  SimCan_GetInjectedCount(void);                 /* frames accepted by SimCan_Inject */
uint32  SimCan_GetDroppedCount(void);                  /* frames refused by the backend (RX FIFO full / controller stopped) */
uint32  SimCan_GetScriptFrames(void);                  /* frames loaded from the RX script file */

#endif /* SIMCAN_H */
