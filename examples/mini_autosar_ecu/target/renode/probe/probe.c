/*
 * probe.c  (target/renode/probe)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none. Register-behaviour probe used to VERIFY, against Renode 1.17.0's STM32L552 peripheral
 * models, the assumptions the MCAL drivers (Mcu/Port/Dio/Adc/Can target backends) make about RCC / GPIO / ADC1 / FDCAN1.
 * It prints "name=0xVALUE" lines on USART1; the verified/unverified findings are recorded in the header of
 * mcal/can/Can_Hw_Stm32.c and in the final hand-over notes. Run with target/renode/probe/probe.resc.
 */
#include <stdint.h>
#include "../common/mini_uart.h"

#define R32(a) (*(volatile uint32_t *)(a))
#define RCC       0x40021000u
#define GPIOA     0x42020000u
#define GPIOC     0x42020800u
#define ADC1      0x42028000u
#define FDCAN1    0x4000A400u
#define SRAMCAN   0x4000AC00u
#define FLASHC    0x40022000u

static volatile uint32_t s_ms;
void SysTick_Handler(void) { s_ms++; }
static void delay_ms(uint32_t ms) { uint32_t t = s_ms; while ((s_ms - t) < ms) { } }

static void probe_rcc(void)
{
    mu_puts("--- RCC\r\n");
    mu_reg("CR.reset", R32(RCC + 0x00u));
    mu_reg("ICSCR", R32(RCC + 0x04u));
    mu_reg("CFGR.reset", R32(RCC + 0x08u));
    mu_reg("PLLCFGR.reset", R32(RCC + 0x0Cu));
    mu_reg("AHB2ENR.reset", R32(RCC + 0x4Cu));
    mu_reg("APB1ENR1.reset", R32(RCC + 0x58u));
    mu_reg("CCIPR1.reset", R32(RCC + 0x88u));
    mu_reg("CSR.reset", R32(RCC + 0x94u));
    /* PLL: MSI source, M=1, N=40, R=/2 -> 80 MHz ; Q enabled */
    R32(RCC + 0x0Cu) = 0x1u | (0u << 4) | (40u << 8) | (1u << 20) | (0u << 21) | (1u << 24) | (0u << 25);
    mu_reg("PLLCFGR.set", R32(RCC + 0x0Cu));
    R32(RCC + 0x00u) |= (1u << 24);                         /* PLLON */
    delay_ms(2);
    mu_reg("CR.pllon", R32(RCC + 0x00u));
    R32(RCC + 0x08u) = (R32(RCC + 0x08u) & ~3u) | 3u;       /* SW = PLL */
    delay_ms(2);
    mu_reg("CFGR.sw=pll", R32(RCC + 0x08u));
    R32(RCC + 0x00u) |= (1u << 16);                         /* HSEON */
    delay_ms(2);
    mu_reg("CR.hseon", R32(RCC + 0x00u));
    R32(RCC + 0x4Cu) |= (1u << 0) | (1u << 1) | (1u << 2) | (1u << 13);    /* GPIOA/B/C + ADC clocks */
    mu_reg("AHB2ENR.set", R32(RCC + 0x4Cu));
    R32(RCC + 0x58u) |= (1u << 25);                         /* FDCANEN */
    mu_reg("APB1ENR1.set", R32(RCC + 0x58u));
    R32(RCC + 0x88u) = (R32(RCC + 0x88u) & ~(3u << 24)) | (1u << 24);
    mu_reg("CCIPR1.set", R32(RCC + 0x88u));
    R32(FLASHC + 0x00u) = 4u;
    mu_reg("FLASH_ACR", R32(FLASHC + 0x00u));
}

static void probe_gpio(void)
{
    mu_puts("--- GPIO\r\n");
    mu_reg("GPIOC.MODER.reset", R32(GPIOC + 0x00u));
    R32(GPIOC + 0x00u) = (R32(GPIOC + 0x00u) & ~(3u << 14)) | (1u << 14);   /* PC7 output */
    mu_reg("GPIOC.MODER.pc7out", R32(GPIOC + 0x00u));
    R32(GPIOC + 0x18u) = (1u << 7);                                         /* BSRR set */
    mu_reg("GPIOC.ODR.afterset", R32(GPIOC + 0x14u));
    mu_reg("GPIOC.IDR.afterset", R32(GPIOC + 0x10u));
    R32(GPIOC + 0x18u) = (1u << (7 + 16));
    mu_reg("GPIOC.ODR.afterclr", R32(GPIOC + 0x14u));
    mu_reg("GPIOA.MODER.reset", R32(GPIOA + 0x00u));
    R32(GPIOA + 0x00u) |= (3u << 2);                                        /* PA1 analog */
    mu_reg("GPIOA.MODER.pa1analog", R32(GPIOA + 0x00u));
}

static void probe_adc(void)
{
    uint32_t i;
    mu_puts("--- ADC1\r\n");
    mu_reg("ADC.ISR.reset", R32(ADC1 + 0x00u));
    mu_reg("ADC.CR.reset", R32(ADC1 + 0x08u));
    mu_reg("ADC.CFGR.reset", R32(ADC1 + 0x0Cu));
    R32(ADC1 + 0x08u) &= ~(1u << 29);                    /* DEEPPWD = 0 */
    R32(ADC1 + 0x08u) |= (1u << 28);                     /* ADVREGEN */
    delay_ms(1);
    mu_reg("ADC.CR.vreg", R32(ADC1 + 0x08u));
    R32(ADC1 + 0x08u) |= (1u << 31);                     /* ADCAL */
    for (i = 0; i < 100000u && (R32(ADC1 + 0x08u) & (1u << 31)); i++) { }
    mu_reg("ADC.CR.aftercal", R32(ADC1 + 0x08u));
    R32(ADC1 + 0x00u) = 1u;                              /* clear ADRDY */
    R32(ADC1 + 0x08u) |= 1u;                             /* ADEN */
    for (i = 0; i < 100000u && !(R32(ADC1 + 0x00u) & 1u); i++) { }
    mu_reg("ADC.ISR.afteraden", R32(ADC1 + 0x00u));
    mu_reg("ADC.CR.afteraden", R32(ADC1 + 0x08u));
    R32(ADC1 + 0x14u) = 4u << 18;                        /* SMPR1: SMP6 (bits 20:18) = 4 */
    R32(ADC1 + 0x30u) = (6u << 6);                       /* SQR1: L=0, SQ1 = channel 6 */
    mu_reg("ADC.SQR1", R32(ADC1 + 0x30u));
    for (uint32_t n = 0; n < 6; n++) {
        R32(ADC1 + 0x00u) = 0xFFFFFFFFu;                 /* clear flags (w1c) */
        mu_reg("ADC.ISR.cleared", R32(ADC1 + 0x00u));
        R32(ADC1 + 0x08u) |= (1u << 2);                  /* ADSTART */
        for (i = 0; i < 100000u && !(R32(ADC1 + 0x00u) & (1u << 2)); i++) { }
        mu_reg("ADC.ISR.eoc", R32(ADC1 + 0x00u));
        mu_reg("ADC.DR", R32(ADC1 + 0x40u));
        mu_reg("ADC.CR.afterconv", R32(ADC1 + 0x08u));
        delay_ms(60);
    }
}

static void probe_fdcan(void)
{
    mu_puts("--- FDCAN1\r\n");
    mu_reg("CREL", R32(FDCAN1 + 0x00u));
    mu_reg("ENDN", R32(FDCAN1 + 0x04u));
    mu_reg("CCCR.reset", R32(FDCAN1 + 0x18u));
    mu_reg("NBTP.reset", R32(FDCAN1 + 0x1Cu));
    mu_reg("TXBC.reset", R32(FDCAN1 + 0xC0u));
    mu_reg("TXFQS.reset", R32(FDCAN1 + 0xC4u));
    mu_reg("RXGFC.reset", R32(FDCAN1 + 0x80u));
    mu_reg("RXF0S.reset", R32(FDCAN1 + 0x90u));
    mu_reg("CKDIV.reset", R32(FDCAN1 + 0x100u));
    R32(FDCAN1 + 0x18u) |= 1u;                           /* INIT */
    mu_reg("CCCR.init", R32(FDCAN1 + 0x18u));
    R32(FDCAN1 + 0x18u) |= 2u;                           /* CCE */
    mu_reg("CCCR.init+cce", R32(FDCAN1 + 0x18u));
    R32(FDCAN1 + 0x1Cu) = (1u << 25) | (9u << 16) | (12u << 8) | 1u;
    mu_reg("NBTP.set", R32(FDCAN1 + 0x1Cu));
    R32(FDCAN1 + 0x80u) = (2u << 4) | (2u << 2) | (1u << 24);          /* ANFS/ANFE reject, LSS=1 */
    mu_reg("RXGFC.set", R32(FDCAN1 + 0x80u));
    R32(SRAMCAN + 0x00u) = (2u << 30) | (1u << 27) | (0x123u << 16) | 0x7FFu;
    mu_reg("SRAMCAN.word0", R32(SRAMCAN + 0x00u));
    R32(FDCAN1 + 0x54u) = 1u;                            /* IE.RF0NE */
    mu_reg("IE", R32(FDCAN1 + 0x54u));
    R32(FDCAN1 + 0x5Cu) = 1u;                            /* ILE.EINT0 */
    mu_reg("ILE", R32(FDCAN1 + 0x5Cu));
    R32(FDCAN1 + 0x18u) &= ~1u;                          /* leave INIT */
    mu_reg("CCCR.run", R32(FDCAN1 + 0x18u));
    mu_reg("PSR.run", R32(FDCAN1 + 0x44u));
    mu_reg("TXFQS.run", R32(FDCAN1 + 0xC4u));
}

int main(void)
{
    mu_init();
    mu_puts("probe: start\r\n");
    R32(0xE000E014u) = 80000000u / 1000u - 1u;
    R32(0xE000E018u) = 0u;
    R32(0xE000E010u) = 7u;
    probe_rcc();
    probe_gpio();
    probe_adc();
    probe_fdcan();
    mu_puts("probe: done\r\n");
    for (;;) { }
}
