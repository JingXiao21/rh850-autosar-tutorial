/*
 * mini_uart.h  (target/renode/common)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (debug console; the real stack would use Dlt/Det). Polling USART1 output used by the
 * Renode bring-up programs (same registers as bringup/stage0_hello/main.c: CR1 0x00, BRR 0x0C, ISR 0x1C, TDR 0x28).
 */
#ifndef MINI_UART_H
#define MINI_UART_H
#include <stdint.h>

#define MU_REG32(a)   (*(volatile uint32_t *)(a))
#define MU_USART1     0x40013800u

static inline void mu_init(void)
{
    MU_REG32(MU_USART1 + 0x0Cu) = 80000000u / 115200u;
    MU_REG32(MU_USART1 + 0x00u) = (1u << 3) | (1u << 0);      /* TE | UE */
}
static inline void mu_putc(char c)
{
    while ((MU_REG32(MU_USART1 + 0x1Cu) & (1u << 7)) == 0u) { }   /* TXE */
    MU_REG32(MU_USART1 + 0x28u) = (uint32_t)(unsigned char)c;
}
static inline void mu_puts(const char *s) { while (*s != '\0') { mu_putc(*s++); } }
static inline void mu_hex(uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    mu_puts("0x");
    for (int i = 28; i >= 0; i -= 4) { mu_putc(h[(v >> i) & 0xFu]); }
}
/* "name=0x........\r\n" */
static inline void mu_reg(const char *name, uint32_t v) { mu_puts(name); mu_putc('='); mu_hex(v); mu_puts("\r\n"); }
#endif
