// SOURCES: sim/restbus/RestBus.c
// [Educational Implementation] Host unit test of the rest-bus node logic: stimulus profiles of DESIGN 11.2 and the
// frame schedule produced by RestBus_Step (periods, counts, payloads, EMULATE_SPEED flag).
#include "test_support.h"
#include "RestBus.h"

static int  n301, n3f0, n101, nOther;
static uint32 first301, first3f0, first101;
static uint8  lastD[3][2];
static uint32 curMs;

static boolean send(uint32 id, uint8 dlc, const uint8 *d)
{
    if (id == 0x301u)      { if (n301 == 0) { first301 = curMs; } n301++; CHECK_EQ(dlc, 1); lastD[0][0] = d[0]; }
    else if (id == 0x3F0u) { if (n3f0 == 0) { first3f0 = curMs; } n3f0++; CHECK_EQ(dlc, 1); lastD[1][0] = d[0]; }
    else if (id == 0x101u) { if (n101 == 0) { first101 = curMs; } n101++; CHECK_EQ(dlc, 2); lastD[2][0] = d[0]; lastD[2][1] = d[1]; }
    else                   { nOther++; }
    return TRUE;
}

static void run(uint32 flags)
{
    n301 = n3f0 = n101 = nOther = 0;
    RestBus_Init(send, flags);
    for (curMs = 1u; curMs <= 4000u; curMs++) { RestBus_Step(curMs); }
}

int main(void)
{
    /* ---- pure profiles ---- */
    CHECK_EQ(RestBus_AmbientLux(0u), 200);     CHECK_EQ(RestBus_AmbientLux(499u), 200);
    CHECK_EQ(RestBus_AmbientLux(500u), 60);    CHECK_EQ(RestBus_AmbientLux(1499u), 60);
    CHECK_EQ(RestBus_AmbientLux(1500u), 20);   CHECK_EQ(RestBus_AmbientLux(3999u), 20);
    CHECK_EQ(RestBus_ModeRequest(2999u), 0);   CHECK_EQ(RestBus_ModeRequest(3000u), 1);
    CHECK_EQ(RestBus_ModeRequest(3499u), 1);   CHECK_EQ(RestBus_ModeRequest(3500u), 0);
    CHECK_EQ(RestBus_Speed01kmh(0u), 0);       CHECK_EQ(RestBus_Speed01kmh(200u), 0);
    CHECK_EQ(RestBus_Speed01kmh(1000u), (2400u * 2000u) / 4095u);
    CHECK_EQ(RestBus_Speed01kmh(1200u), (3000u * 2000u) / 4095u);   /* saturates at raw 3000 */
    CHECK_EQ(RestBus_Speed01kmh(4000u), (3000u * 2000u) / 4095u);

    /* ---- schedule without speed emulation ---- */
    run(0u);
    CHECK_EQ(n301, 41);                     /* t = 1, 100, 200 ... 4000 */
    CHECK_EQ(n3f0, 40);
    CHECK_EQ(n101, 0);
    CHECK_EQ(nOther, 0);
    CHECK_EQ(first301, 1);                      /* first Step call sends immediately (phase 0) */
    CHECK_EQ(first3f0, 3);
    CHECK_EQ(lastD[0][0], 20);                  /* last 0x301 payload: t >= 1500 -> 20 lux */
    CHECK_EQ(lastD[1][0], 0);                   /* last 0x3F0 payload: RUN */

    /* ---- with speed emulation ---- */
    run(RESTBUS_FLAG_EMULATE_SPEED);
    CHECK_EQ(n101, 200);
    CHECK_EQ(first101, 7);
    CHECK_EQ((lastD[2][0] | (lastD[2][1] << 8)), RestBus_Speed01kmh(3987u));   /* last frame at 7 + 199*20 */

    /* ---- no send function: Step is harmless ---- */
    RestBus_Init(NULL_PTR, 0u);
    RestBus_Step(10u);
    TEST_DONE();
}
