/*
 * Trace.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none. Closest real concepts: Det trace hooks / ARTI (Autosar Run-Time Interface,
 * AUTOSAR_CP_SWS_OS chapter 7.17 "ARTI" hooks) - here reduced to one text line per event so that host runs
 * and Renode runs can be compared with a text diff.
 * Implemented: the hardware-independent formatter (Trace_Log) with a tiny printf subset, timestamp
 * (Trace_GetTimeUs).  The character sink (Trace_Init/Trace_PutChar/Trace_PortLock) lives in the backends
 * os/port/cm33/Trace_Target.c (USART1) and os/port/host/Trace_Host.c (stdout + log file).
 * Format: see include/Trace.h and DESIGN section 12.  A full line is assembled in a local buffer first and
 * then sent under the backend's short lock, so lines from tasks and ISRs never interleave.
 */
#include <stdarg.h>
#include "Trace.h"
#include "Mini_Time.h"
#include "Trace_Port.h"

#define TRACE_LINE_MAX  200u

static const char *const s_catName[TRACE_CAT_COUNT] = {
    "OS   ", "RTE  ", "COM  ", "PDUR ", "CANIF", "CAN  ", "SWC  ", "BSW  ", "DET  ", "SIM  "
};

typedef struct {
    char   buf[TRACE_LINE_MAX];
    uint32 len;
} Trace_LineType;

static void line_putc(Trace_LineType *l, char c)
{
    if (l->len < (TRACE_LINE_MAX - 3u)) {          /* keep room for the line terminator */
        l->buf[l->len] = c;
        l->len++;
    }
}

static void line_puts(Trace_LineType *l, const char *s)
{
    while (*s != '\0') {
        line_putc(l, *s);
        s++;
    }
}

/* unsigned number in `base` (10 or 16), minimum width `width`, padded with '0' or ' ' */
static void line_putnum(Trace_LineType *l, uint32 v, uint32 base, boolean upper, uint32 width, char pad)
{
    char tmp[12];
    uint32 n = 0u;
    const char *digits = (upper != FALSE) ? "0123456789ABCDEF" : "0123456789abcdef";

    do {
        tmp[n] = digits[v % base];
        n++;
        v /= base;
    } while ((v != 0u) && (n < sizeof(tmp)));
    while (width > n) {
        line_putc(l, pad);
        width--;
    }
    while (n > 0u) {
        n--;
        line_putc(l, tmp[n]);
    }
}

static void line_putsigned(Trace_LineType *l, sint32 v, uint32 width, char pad)
{
    if (v < 0) {
        line_putc(l, '-');
        line_putnum(l, (uint32)(0u - (uint32)v), 10u, FALSE, (width > 0u) ? (width - 1u) : 0u, pad);
    } else {
        line_putnum(l, (uint32)v, 10u, FALSE, width, pad);
    }
}

/* the printf subset: %u %d %x %X (flag 0, width <= 8) %s %c %% and %B (two args: const uint8 *p, unsigned len) */
static void line_vformat(Trace_LineType *l, const char *fmt, va_list ap)
{
    while (*fmt != '\0') {
        char c = *fmt;
        fmt++;
        if (c != '%') {
            line_putc(l, c);
            continue;
        }
        {
            char pad = ' ';
            uint32 width = 0u;

            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }
            while ((*fmt >= '0') && (*fmt <= '9')) {
                width = (width * 10u) + (uint32)(*fmt - '0');
                fmt++;
            }
            if (width > 8u) {
                width = 8u;
            }
            while (*fmt == 'l') {                  /* tolerate %lu / %lx: all integers are 32 bit here */
                fmt++;
            }
            c = *fmt;
            if (c == '\0') {
                break;
            }
            fmt++;
            switch (c) {
            case 'u':
                line_putnum(l, (uint32)va_arg(ap, unsigned int), 10u, FALSE, width, pad);
                break;
            case 'd':
                line_putsigned(l, (sint32)va_arg(ap, int), width, pad);
                break;
            case 'x':
                line_putnum(l, (uint32)va_arg(ap, unsigned int), 16u, FALSE, width, pad);
                break;
            case 'X':
                line_putnum(l, (uint32)va_arg(ap, unsigned int), 16u, TRUE, width, pad);
                break;
            case 's': {
                const char *s = va_arg(ap, const char *);
                line_puts(l, (s != NULL_PTR) ? s : "(null)");
                break;
            }
            case 'c':
                line_putc(l, (char)va_arg(ap, int));
                break;
            case 'B': {
                const uint8 *p = va_arg(ap, const uint8 *);
                unsigned int n = va_arg(ap, unsigned int);
                unsigned int i;
                for (i = 0u; i < n; i++) {
                    if (i != 0u) {
                        line_putc(l, ' ');
                    }
                    line_putnum(l, (uint32)p[i], 16u, FALSE, 2u, '0');
                }
                break;
            }
            case '%':
                line_putc(l, '%');
                break;
            default:                               /* unknown conversion: print it literally so the bug is visible */
                line_putc(l, '%');
                line_putc(l, c);
                break;
            }
        }
    }
}

uint32 Trace_GetTimeUs(void)
{
    return Mini_Time_GetUs();
}

void Trace_Log(Trace_CategoryType cat, const char *fmt, ...)
{
    Trace_LineType line;
    va_list ap;
    uint32 i;
    uint32 lk;

    line.len = 0u;
    line_putc(&line, '[');
    line_putnum(&line, Trace_GetTimeUs(), 10u, FALSE, 9u, '0');
    line_puts(&line, "] " MINI_ECU_NAME " ");
    line_puts(&line, ((uint32)cat < (uint32)TRACE_CAT_COUNT) ? s_catName[cat] : "?????");
    line_putc(&line, ' ');
    va_start(ap, fmt);
    line_vformat(&line, fmt, ap);
    va_end(ap);
#if defined(MINI_PLATFORM_TARGET)
    line.buf[line.len] = '\r';                     /* serial terminals want CR LF */
    line.len++;
#endif
    line.buf[line.len] = '\n';
    line.len++;

    lk = Trace_PortLock();
    for (i = 0u; i < line.len; i++) {
        Trace_PutChar(line.buf[i]);
    }
    Trace_PortUnlock(lk);
}
