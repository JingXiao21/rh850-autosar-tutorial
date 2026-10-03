/*
 * Trace_Target.c  (TARGET BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (debug console, deliberately NOT an AUTOSAR module: tracing must work before
 * any driver is initialised and from every context).  Character sink of the trace on the STM32L552: USART1
 * (115200 8N1, polling).  Register layout "USARTv2": CR1 +0x00, BRR +0x0C, ISR +0x1C, TDR +0x28 (RM0438).
 * Renode's STM32 USART model accepts bytes at any time and writes them to the file backend, so no pin
 * muxing is needed there; on real hardware PA9 (AF7) must be configured by the Port driver.
 * Independent of the OS: also linked into the bare-metal rest-bus image.
 */
#include "Trace.h"
#include "../../src/Trace_Port.h"

#define REG32(a)         (*(volatile uint32 *)(a))
#define USART1_BASE      0x40013800u
#define USART_CR1        REG32(USART1_BASE + 0x00u)   /* bit0 UE, bit3 TE */
#define USART_BRR        REG32(USART1_BASE + 0x0Cu)   /* baud = fck / BRR (oversampling by 16) */
#define USART_ISR        REG32(USART1_BASE + 0x1Cu)   /* bit7 TXE: transmit data register empty */
#define USART_TDR        REG32(USART1_BASE + 0x28u)
#define RCC_APB2ENR      REG32(0x40021060u)           /* bit14 USART1EN (clock gate; irrelevant in Renode) */

static boolean s_ready;

void Trace_Init(const char *logFile)
{
    (void)logFile;                                    /* target: no log file */
    if (s_ready != FALSE) {
        return;
    }
    RCC_APB2ENR |= (1uL << 14);
    USART_BRR = MINI_CPU_CLOCK_HZ / 115200u;
    USART_CR1 = (1uL << 3) | (1uL << 0);              /* TE | UE */
    s_ready = TRUE;
}

void Trace_PutChar(char c)
{
    if (s_ready == FALSE) {
        Trace_Init(NULL_PTR);                         /* tracing before Trace_Init still works */
    }
    while ((USART_ISR & (1uL << 7)) == 0u) {
        /* wait until the transmit register is empty */
    }
    USART_TDR = (uint32)(uint8)c;
}

/* PRIMASK based: safe from tasks and from any interrupt, nestable by returning the previous state */
uint32 Trace_PortLock(void)
{
    uint32 primask;

    __asm volatile ("mrs %0, primask \n cpsid i" : "=r" (primask) : : "memory");
    return primask;
}

void Trace_PortUnlock(uint32 prev)
{
    __asm volatile ("msr primask, %0" : : "r" (prev) : "memory");
}
