/* [Educational Implementation] Stage 0 smoke test: USART1 print + SysTick interrupt.
 * Register offsets are those of the STM32 "USARTv2" block (CR1 0x00, BRR 0x0C, ISR 0x1C, TDR 0x28)
 * and the Armv8-M SysTick (0xE000E010); verify with RM0438 for real hardware. */
#include <stdint.h>

#define REG32(a)        (*(volatile uint32_t *)(a))
#define USART1_BASE     0x40013800u
#define USART_CR1       REG32(USART1_BASE + 0x00u)
#define USART_BRR       REG32(USART1_BASE + 0x0Cu)
#define USART_ISR       REG32(USART1_BASE + 0x1Cu)
#define USART_TDR       REG32(USART1_BASE + 0x28u)
#define SYST_CSR        REG32(0xE000E010u)
#define SYST_RVR        REG32(0xE000E014u)
#define SYST_CVR        REG32(0xE000E018u)

static volatile uint32_t s_ticks;              /* .bss  -> must read 0 after startup */
static volatile uint32_t s_magic = 0xC0FFEEu;           /* .data -> must read 0xC0FFEE after startup */

void SysTick_Handler(void) { s_ticks++; }

static void uart_putc(char c)
{
    while ((USART_ISR & (1u << 7)) == 0u) { }  /* TXE */
    USART_TDR = (uint32_t)c;
}
static void uart_puts(const char *s) { while (*s != '\0') { uart_putc(*s++); } }
static void uart_puthex(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) { uart_putc(hex[(v >> i) & 0xFu]); }
}

int main(void)
{
    USART_BRR = 80000000u / 115200u;
    USART_CR1 = (1u << 3) | (1u << 0);         /* TE | UE */

    uart_puts("stage0: reset ok, .data=");
    uart_puthex(s_magic);
    uart_puts(" .bss=");
    uart_puthex(s_ticks);
    uart_puts("\r\n");

    SYST_RVR = 80000000u / 1000u - 1u;         /* 1 ms at 80 MHz (Renode ramn.repl systickFrequency) */
    SYST_CVR = 0u;
    SYST_CSR = 0x7u;                           /* CLKSOURCE | TICKINT | ENABLE */

    for (uint32_t n = 1u; n <= 3u; n++) {
        while (s_ticks < n * 100u) { }
        uart_puts("stage0: systick ");
        uart_puthex(s_ticks);
        uart_puts(" ms\r\n");
    }
    uart_puts("stage0: done\r\n");
    for (;;) { }
}
