/*
 * UdsTrace.c
 *
 * [Educational Implementation] Text trace of layer hops. See UdsTrace.h.
 */
#include "UdsTrace.h"
#include "SimClock.h"
#include <stdarg.h>

static boolean UdsTrace_Enabled = TRUE;
static FILE *UdsTrace_Sink = NULL;

void UdsTrace_SetEnabled(boolean enabled) { UdsTrace_Enabled = enabled; }
boolean UdsTrace_IsEnabled(void) { return UdsTrace_Enabled; }
void UdsTrace_SetSink(FILE *sink) { UdsTrace_Sink = sink; }

void UdsTrace_Log(const char *module, const char *fmt, ...)
{
    va_list args;
    FILE *out = (UdsTrace_Sink != NULL) ? UdsTrace_Sink : stdout;

    if (!UdsTrace_Enabled) {
        return;
    }
    fprintf(out, "[%6lu ms] [%-8s] ", (unsigned long)SimClock_NowMs(), module);
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fputc('\n', out);
}

const char *UdsTrace_Hex(const uint8 *data, uint16 length)
{
    static char ring[4][64u * 3u + 8u];
    static uint8 slot;
    static const char digits[] = "0123456789ABCDEF";
    char *buf = ring[slot];
    uint16 i;
    uint16 shown = (length > 64u) ? 64u : length;
    size_t pos = 0u;

    slot = (uint8)((slot + 1u) & 3u);
    for (i = 0u; i < shown; i++) {
        if (i != 0u) {
            buf[pos++] = ' ';
        }
        buf[pos++] = digits[(data[i] >> 4) & 0x0Fu];
        buf[pos++] = digits[data[i] & 0x0Fu];
    }
    if (shown < length) {
        buf[pos++] = ' ';
        buf[pos++] = '.';
        buf[pos++] = '.';
        buf[pos++] = '.';
    }
    buf[pos] = '\0';
    return buf;
}
