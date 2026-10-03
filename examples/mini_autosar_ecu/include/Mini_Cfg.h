/*
 * Mini_Cfg.h
 *
 * [Educational Implementation]
 * Project-wide switches. No real AUTOSAR counterpart (closest: per-module <Mod>_Cfg.h pre-compile
 * switches such as <Mod>DevErrorDetect).
 *
 * The build scripts define EXACTLY ONE platform and ONE ECU identity on the command line:
 *     -DMINI_PLATFORM_TARGET | -DMINI_PLATFORM_HOST
 *     -DMINI_ECU_A (SensorEcu) | -DMINI_ECU_B (LightEcu) | -DMINI_ECU_REST (rest-bus node, target only)
 */
#ifndef MINI_CFG_H
#define MINI_CFG_H

#include "Std_Types.h"

#if defined(MINI_PLATFORM_TARGET) && defined(MINI_PLATFORM_HOST)
#error "define exactly one of MINI_PLATFORM_TARGET / MINI_PLATFORM_HOST"
#endif
#if !defined(MINI_PLATFORM_TARGET) && !defined(MINI_PLATFORM_HOST)
#error "define exactly one of MINI_PLATFORM_TARGET / MINI_PLATFORM_HOST"
#endif

#if defined(MINI_ECU_A)
  #define MINI_ECU_NAME   "ECUA"
#elif defined(MINI_ECU_B)
  #define MINI_ECU_NAME   "ECUB"
#elif defined(MINI_ECU_REST)
  #define MINI_ECU_NAME   "REST"
#else
  #define MINI_ECU_NAME   "TEST"        /* unit-test executables may omit the ECU define */
#endif

/* ---- development error detection (Det) for all BSW modules ---- */
#define MINI_DEV_ERROR_DETECT   STD_ON

/* ---- OS tick ---- */
#define MINI_OS_TICK_US         1000u       /* 1 ms system counter tick (SysTick 1 kHz / host virtual tick) */
#define MINI_CPU_CLOCK_HZ       80000000u

/* ---- trace (see Trace.h) ---- */
#define MINI_TRACE_MASK         0x0000FFFFu /* bit n = Trace_CategoryType n; reduce to cut UART volume */
#define MINI_TRACE_OS_SWITCH    STD_ON      /* one line per task start/terminate/preempt */

#endif /* MINI_CFG_H */
