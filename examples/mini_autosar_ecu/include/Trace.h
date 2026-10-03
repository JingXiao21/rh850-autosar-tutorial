/*
 * Trace.h
 *
 * [Educational Implementation]
 * No AUTOSAR counterpart (nearest real concepts: Det trace hooks, ARTI/ORTI). One uniform, text,
 * line-oriented trace used by OS / RTE / Com / CAN / SWC code. Identical format on host (stdout and
 * optional log file) and on target (USART1, 115200 8N1).
 *
 * LINE FORMAT (exactly one line per event; "\r\n" on target, "\n" on host):
 *     [<t_us:09u>] <ECU:4> <CAT:5> <free text, key=value pairs>
 * Examples:
 *     [000020000] ECUB OS    TASK_START Task_LightCtl prio=3
 *     [000020120] ECUB RTE   WRITE HeadlightCmd=1
 *     [000020300] ECUB COM   TX pdu=0x201 len=6
 *     [000020310] ECUB CAN   TX id=0x201 dlc=6 data=01 00 00 00 00 00
 * ECU = MINI_ECU_NAME. t_us = time since boot (target: from OS system counter + SysTick->VAL,
 * host: virtual simulated time). CAT is padded to 5 chars by Trace.c. See DESIGN.md section 12 for
 * the catalogue of event names.
 *
 * FORMAT STRINGS: minimal printf subset implemented by Trace.c: %u %d %x %X (flag 0, width <= 8; %x is
 * lower case), %s %c %% and the extension %B = hex dump taking TWO arguments (const uint8 *p, unsigned len)
 * printed as "01 a0 ff". All integer arguments MUST be cast to (unsigned)/(int) (arm uint32_t is unsigned long).
 */
#ifndef TRACE_H
#define TRACE_H

#include "Std_Types.h"
#include "Mini_Cfg.h"

typedef enum {
    TRACE_CAT_OS    = 0,   /* "OS   " */
    TRACE_CAT_RTE   = 1,   /* "RTE  " */
    TRACE_CAT_COM   = 2,   /* "COM  " */
    TRACE_CAT_PDUR  = 3,   /* "PDUR " */
    TRACE_CAT_CANIF = 4,   /* "CANIF" */
    TRACE_CAT_CAN   = 5,   /* "CAN  " */
    TRACE_CAT_SWC   = 6,   /* "SWC  " */
    TRACE_CAT_BSW   = 7,   /* "BSW  " (EcuM/BswM/SchM/IoHwAb/MCAL) */
    TRACE_CAT_DET   = 8,   /* "DET  " */
    TRACE_CAT_SIM   = 9,   /* "SIM  " (host simulator / rest-bus / test harness) */
    TRACE_CAT_COUNT
} Trace_CategoryType;

/* Select backend. Target: configures USART1 directly (debug console, NOT an AUTOSAR module).
 * Host: stdout, optionally mirrored to logFile. Idempotent: Trace_Init(NULL_PTR) keeps an already selected log
 * file (HostMain opens the file first; EcuM_AL_DriverInitZero later calls Trace_Init(NULL_PTR)). Callable before OS start. */
void   Trace_Init(const char *logFile);

/* Emit one event line. Safe from task and ISR context (short interrupt lock; never blocks). */
void   Trace_Log(Trace_CategoryType cat, const char *fmt, ...);

/* Timestamp source = Mini_Time_GetUs() (include/Mini_Time.h), microseconds since boot, wraps at 2^32 us = 71 min. */
uint32 Trace_GetTimeUs(void);

/* Backend hook: one character out. Target: USART1 TDR (polling). Host: fputc. */
void   Trace_PutChar(char c);

#define TRACE(cat, ...)  do { if (((MINI_TRACE_MASK) >> (cat)) & 1u) { Trace_Log((cat), __VA_ARGS__); } } while (0)

#endif /* TRACE_H */
