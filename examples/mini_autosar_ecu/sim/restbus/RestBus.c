/*
 * RestBus.c
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart. "Rest-bus simulation" = emulating all bus participants that are not under test; here
 * the stimulus node of the DESIGN 11 scenario. ONE source for two builds:
 *   host   : HostMain registers RestBus_Step as (part of) a SimTime step hook, send = SimCan_Inject
 *   target : third Renode machine "REST" (target/restbus/main_restbus.c), send = Can_Write on the real MCAL driver
 * Frames (classic CAN, standard id), phase offsets avoid bursts that would overflow the 3-deep RX FIFO of the ECU:
 *   0x301 AmbientLight dlc 1  every 100 ms, phase 0   : 200 (t<500), 60 (500<=t<1500), 20 (t>=1500)
 *   0x3F0 EcuModeReq   dlc 1  every 100 ms, phase 3   : 1 for 3000<=t<3500 else 0  (0 = RUN, 1 = POST_RUN)
 *   0x101 VehicleSpeed dlc 2  every  20 ms, phase 7   : only with RESTBUS_FLAG_EMULATE_SPEED (host runs of ECU_B
 *                                                       without a SensorEcu log). LE uint16, 0.1 km/h
 * The profiles are pure functions of time so that tests and both builds agree.
 */
#include "RestBus.h"
#include "Trace.h"

#define RB_PERIOD_AMBIENT_MS  100u
#define RB_PERIOD_MODE_MS     100u
#define RB_PERIOD_SPEED_MS    20u
#define RB_PHASE_AMBIENT_MS   0u
#define RB_PHASE_MODE_MS      3u
#define RB_PHASE_SPEED_MS     7u

static RestBus_SendFn rb_send;
static uint32         rb_flags;
static uint32         rb_nextAmbient;
static uint32         rb_nextMode;
static uint32         rb_nextSpeed;

uint8 RestBus_AmbientLux(uint32 timeMs)
{
    if (timeMs < 500u)  { return 200u; }
    if (timeMs < 1500u) { return 60u; }
    return 20u;
}

uint8 RestBus_ModeRequest(uint32 timeMs)
{
    return (uint8)(((timeMs >= 3000u) && (timeMs < 3500u)) ? 1u : 0u);
}

/* wheel ADC profile (DESIGN 11.2) converted with the SpeedSensorSWC formula speed = raw*2000/4095 */
uint16 RestBus_Speed01kmh(uint32 timeMs)
{
    uint32 raw = 0u;
    if (timeMs > 200u) {
        raw = 3u * (timeMs - 200u);
        if (raw > 3000u) { raw = 3000u; }
    }
    return (uint16)((raw * 2000u) / 4095u);
}

void RestBus_Init(RestBus_SendFn send, uint32 flags)
{
    rb_send = send;
    rb_flags = flags;
    rb_nextAmbient = RB_PHASE_AMBIENT_MS;
    rb_nextMode = RB_PHASE_MODE_MS;
    rb_nextSpeed = RB_PHASE_SPEED_MS;
}

static void rb_tx(uint32 id, uint8 dlc, const uint8 *data)
{
    boolean ok = (rb_send != NULL_PTR) ? rb_send(id, dlc, data) : FALSE;
    TRACE(TRACE_CAT_SIM, "RESTBUS TX id=0x%03x dlc=%u data=%B%s", (unsigned)id, (unsigned)dlc, data, (unsigned)dlc,
          ok ? "" : " (not accepted)");
}

void RestBus_Step(uint32 nowMs)
{
    uint8 d[2];
    if (rb_send == NULL_PTR) { return; }
    if (nowMs >= rb_nextAmbient) {
        d[0] = RestBus_AmbientLux(nowMs);
        rb_tx(0x301u, 1u, d);
        rb_nextAmbient += RB_PERIOD_AMBIENT_MS;
    }
    if (nowMs >= rb_nextMode) {
        d[0] = RestBus_ModeRequest(nowMs);
        rb_tx(0x3F0u, 1u, d);
        rb_nextMode += RB_PERIOD_MODE_MS;
    }
    if (((rb_flags & RESTBUS_FLAG_EMULATE_SPEED) != 0u) && (nowMs >= rb_nextSpeed)) {
        uint16 v = RestBus_Speed01kmh(nowMs);
        d[0] = (uint8)(v & 0xFFu);
        d[1] = (uint8)(v >> 8);
        rb_tx(0x101u, 2u, d);
        rb_nextSpeed += RB_PERIOD_SPEED_MS;
    }
}

/* frame seen on the bus (target REST node; the host run has no receive path for ECU frames: they go to the TX log) */
void RestBus_OnRx(uint32 id, uint8 dlc, const uint8 *data)
{
    TRACE(TRACE_CAT_SIM, "RESTBUS RX id=0x%03x dlc=%u data=%B", (unsigned)id, (unsigned)dlc, data, (unsigned)dlc);
}
