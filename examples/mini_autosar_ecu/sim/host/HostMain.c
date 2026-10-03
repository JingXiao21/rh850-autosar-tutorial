/*
 * HostMain.c  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart: replaces the reset handler + main() of the target image. One host executable = one ECU
 * (DESIGN 10.2). Parses the command line, brings up the simulator (trace file, register file + peripherals,
 * virtual CAN files, rest-bus, virtual time limit) and then runs the very same startup as the target:
 * EcuM_Init() -> ... -> StartOS (the host OS port then runs the scheduler on fibers and never returns until
 * ShutdownOS: the process exits from there, see Os_Hooks.c ShutdownHook for the end-of-run trace line).
 *
 * Command line (DESIGN 10.3):
 *   --run-ms N            end of virtual time (default 4000)
 *   --log FILE            mirror the trace into FILE
 *   --can-tx-log FILE     write every transmitted CAN frame (becomes the rx script of the other ECU)
 *   --can-rx-script FILE  inject the frames of FILE at their timestamps
 *   --restbus             enable the scripted tester (0x301 AmbientLight, 0x3F0 EcuModeReq)
 *   --emulate-speed       (with --restbus) the rest-bus also sends 0x101 VehicleSpeed on behalf of SensorEcu
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "EcuM.h"
#include "Trace.h"
#include "SimTime.h"
#include "SimMmio.h"
#include "SimCan.h"
#include "RestBus.h"

static boolean host_restbus;

/* one step hook for everything the simulator does at a 1 ms boundary (the hook list of SimTime may be a single slot) */
static void host_step(uint64 nowUs)
{
    SimCan_Step(nowUs);                              /* script frames whose time has come -> RX FIFO + IRQ 39 */
    if (host_restbus) {
        RestBus_Step((uint32)(nowUs / 1000u));       /* tester frames, injected through SimCan_Inject */
    }
}

static int host_usage(const char *prog)
{
    (void)fprintf(stderr,
        "usage: %s [--run-ms N] [--log FILE] [--can-tx-log FILE] [--can-rx-script FILE] [--restbus] [--emulate-speed]\n",
        prog);
    return 2;
}

int main(int argc, char **argv)
{
    unsigned long runMs = 4000ul;
    const char *logFile = NULL_PTR;
    const char *txLog = NULL_PTR;
    const char *rxScript = NULL_PTR;
    uint32 restFlags = 0u;
    boolean emulate = FALSE;
    int i;

    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--run-ms") == 0) && (i + 1 < argc)) {
            runMs = strtoul(argv[++i], NULL, 10);
        } else if ((strcmp(argv[i], "--log") == 0) && (i + 1 < argc)) {
            logFile = argv[++i];
        } else if ((strcmp(argv[i], "--can-tx-log") == 0) && (i + 1 < argc)) {
            txLog = argv[++i];
        } else if ((strcmp(argv[i], "--can-rx-script") == 0) && (i + 1 < argc)) {
            rxScript = argv[++i];
        } else if (strcmp(argv[i], "--restbus") == 0) {
            host_restbus = TRUE;
        } else if (strcmp(argv[i], "--emulate-speed") == 0) {
            emulate = TRUE;
        } else {
            return host_usage(argv[0]);
        }
    }
    if (emulate) { restFlags |= RESTBUS_FLAG_EMULATE_SPEED; }

    Trace_Init(logFile);                             /* first: everything below may trace */
    TRACE(TRACE_CAT_SIM, "HOST run_ms=%u restbus=%u emulate_speed=%u", (unsigned)runMs,
          (unsigned)host_restbus, (unsigned)emulate);

    SimPeripherals_Init();                           /* GPIO/ADC/RCC models, register file */
    SimCan_Init(txLog, rxScript);
    if (host_restbus) {
        RestBus_Init(SimCan_Inject, restFlags);
    }
    SimTime_SetEndUs((uint64)runMs * 1000u);
    SimTime_RegisterStepHook(host_step);

    EcuM_Init();                                     /* ends in StartOS(): returns only when the simulation ended */
    SimCan_Deinit();
    return 0;
}
