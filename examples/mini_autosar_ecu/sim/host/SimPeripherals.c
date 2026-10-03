/*
 * SimPeripherals.c   (HOST BUILD ONLY: located in sim/host/)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (simulation of the microcontroller below the MCAL, comparable to a "virtual ECU" / MiL-SiL
 * peripheral model). Behavioural models registered in the SimMmio register file at the REAL STM32L552 addresses:
 *   GPIOA..GPIOH @ 0x42020000 + 0x400*n : MODER/OTYPER/OSPEEDR/PUPDR/ODR/AFR/ASCR registers, IDR (= ODR for output pins, injected
 *                  level for input pins), BSRR/BRR atomic set/reset. Every ODR bit change is traced ("SIM GPIO PC7=1") so a log
 *                  shows the headlight outputs switching.
 *   ADC1         @ 0x42028000 : power-up / calibration / ADEN handshake and instant software-triggered single conversions. The result for
 *                  the wheel-speed channel 6 is the scenario profile SimAdc_ProfileRaw(t) (DESIGN 11.2; the same function the Renode
 *                  script evaluates), unless a fixed value was set by SimAdc_SetRaw (tests).
 *   RCC          @ 0x40021000 : plain registers + the ready/switch-status handshake of CR (MSI/HSI/HSE/PLL ready follows ON bit) and
 *                  CFGR (SWS follows SW), CSR reset flags with RMVF. Without it Mcu_InitClock would wait for ever.
 * Everything else (FLASH_ACR, SCB, ...) is plain RAM in SimMmio.c. The register layout and handshake mirror what Renode's models do
 * (verified with target/renode/probe) so the SAME MCAL source behaves the same on both builds.
 * Spec context: none (below the MCAL). Owner: agent B.
 */
#include "SimMmio.h"
#include "SimTime.h"
#include "Trace.h"
#include <string.h>

#define NUM_GPIO        8u
#define GPIO_BASE0      0x42020000u
#define GPIO_STRIDE     0x400u
#define ADC1_BASE       0x42028000u
#define RCC_BASE        0x40021000u

/* ------------------------------------------------------------------------------------------------- GPIO */
typedef struct {
    uint32 moder, otyper, ospeedr, pupdr, odr, afrl, afrh, ascr, lckr;
    uint32 input;                                  /* levels injected for input pins by SimDio_SetInput */
} Sim_Gpio;

static Sim_Gpio s_gpio[NUM_GPIO];

static void gpio_reset(void)
{
    uint32 i;
    for (i = 0u; i < NUM_GPIO; i++) {
        s_gpio[i].moder = 0xFFFFFFFFu;
        s_gpio[i].otyper = s_gpio[i].ospeedr = s_gpio[i].pupdr = s_gpio[i].odr = 0u;
        s_gpio[i].afrl = s_gpio[i].afrh = s_gpio[i].ascr = s_gpio[i].lckr = s_gpio[i].input = 0u;
    }
    s_gpio[0].moder = 0xABFFFFFFu;                 /* same reset values as Renode ramn.repl (SWD pins on port A/B) */
    s_gpio[1].moder = 0xFFFFFEBFu;
    s_gpio[7].moder = 0x0000000Fu;
}

static void gpio_set_odr(uint32 port, uint32 newOdr)
{
    uint32 changed = (s_gpio[port].odr ^ newOdr) & 0xFFFFu;
    uint32 pin;
    s_gpio[port].odr = newOdr & 0xFFFFu;
    for (pin = 0u; pin < 16u; pin++) {
        if ((changed & (1u << pin)) != 0u) {
            TRACE(TRACE_CAT_SIM, "GPIO P%c%u=%u", (int)('A' + port), (unsigned)pin, (unsigned)((newOdr >> pin) & 1u));
        }
    }
}

static uint32 gpio_read(uint32 off, void *ctx)
{
    uint32 port = (uint32)(uintptr_t)ctx;
    Sim_Gpio *g = &s_gpio[port];
    uint32 pin, idr = 0u;
    switch (off) {
    case 0x00u: return g->moder;
    case 0x04u: return g->otyper;
    case 0x08u: return g->ospeedr;
    case 0x0Cu: return g->pupdr;
    case 0x10u:                                    /* IDR: output pins show what they drive, inputs the injected level */
        for (pin = 0u; pin < 16u; pin++) {
            uint32 mode = (g->moder >> (2u * pin)) & 3u;
            uint32 bit  = (mode == 1u) ? ((g->odr >> pin) & 1u) : ((mode == 0u) ? ((g->input >> pin) & 1u) : 0u);
            idr |= bit << pin;
        }
        return idr;
    case 0x14u: return g->odr;
    case 0x1Cu: return g->lckr;
    case 0x20u: return g->afrl;
    case 0x24u: return g->afrh;
    case 0x2Cu: return g->ascr;
    default:    return 0u;                         /* BSRR / BRR read as 0 */
    }
}

static void gpio_write(uint32 off, uint32 v, void *ctx)
{
    uint32 port = (uint32)(uintptr_t)ctx;
    Sim_Gpio *g = &s_gpio[port];
    switch (off) {
    case 0x00u: g->moder = v;   break;
    case 0x04u: g->otyper = v & 0xFFFFu; break;
    case 0x08u: g->ospeedr = v; break;
    case 0x0Cu: g->pupdr = v;   break;
    case 0x14u: gpio_set_odr(port, v); break;
    case 0x18u: {                                  /* BSRR: BR (reset, high half) wins over BS (set, low half) */
        uint32 n = g->odr | (v & 0xFFFFu);
        n &= ~((v >> 16) & 0xFFFFu);
        gpio_set_odr(port, n);
        break; }
    case 0x1Cu: g->lckr = v;    break;
    case 0x20u: g->afrl = v;    break;
    case 0x24u: g->afrh = v;    break;
    case 0x28u: gpio_set_odr(port, g->odr & ~(v & 0xFFFFu)); break;     /* BRR */
    case 0x2Cu: g->ascr = v & 0xFFFFu; break;
    default: break;
    }
}

boolean SimDio_GetOutput(uint16 channel)
{
    uint32 port = (uint32)(channel >> 4);
    return (port < NUM_GPIO) ? (((s_gpio[port].odr >> (channel & 15u)) & 1u) != 0u) : FALSE;
}

void SimDio_SetInput(uint16 channel, boolean level)
{
    uint32 port = (uint32)(channel >> 4);
    if (port < NUM_GPIO) {
        if (level) { s_gpio[port].input |=  (1u << (channel & 15u)); }
        else       { s_gpio[port].input &= ~(1u << (channel & 15u)); }
    }
}

/* ------------------------------------------------------------------------------------------------- ADC1 */
#define ADC_ISR_ADRDY   (1u << 0)
#define ADC_ISR_EOSMP   (1u << 1)
#define ADC_ISR_EOC     (1u << 2)
#define ADC_ISR_EOS     (1u << 3)
#define ADC_CR_ADEN     (1u << 0)
#define ADC_CR_ADDIS    (1u << 1)
#define ADC_CR_ADSTART  (1u << 2)
#define ADC_CR_ADSTP    (1u << 4)
#define ADC_CR_ADVREGEN (1u << 28)
#define ADC_CR_DEEPPWD  (1u << 29)
#define ADC_CR_ADCAL    (1u << 31)

typedef struct {
    uint32  isr, ier, cr, cfgr, smpr1, smpr2, sqr1, dr, ccr;
    uint16  chRaw[19];
    boolean chFixed[19];                          /* TRUE: value set by test, FALSE: wheel channel follows the profile */
    boolean wheelOverride;
    uint32  conversions;
} Sim_Adc;

#define SIM_ADC_WHEEL_CHANNEL  6u
static Sim_Adc s_adc;

uint16 SimAdc_ProfileRaw(uint32 timeMs)           /* DESIGN 11.2: raw = 0 (t <= 200 ms), else min(3000, 3*(t-200)) */
{
    uint32 raw;
    if (timeMs <= 200u) {
        return 0u;
    }
    raw = 3u * (timeMs - 200u);
    return (uint16)((raw > 3000u) ? 3000u : raw);
}

void SimAdc_SetRaw(uint16 raw)                    { s_adc.chRaw[SIM_ADC_WHEEL_CHANNEL] = (uint16)(raw & 0xFFFu); s_adc.wheelOverride = TRUE; }
void SimAdc_ClearOverride(void)                   { s_adc.wheelOverride = FALSE; }
void SimAdc_SetChannelRaw(uint8 channel, uint16 raw)
{
    if (channel < 19u) {
        s_adc.chRaw[channel]   = (uint16)(raw & 0xFFFu);
        s_adc.chFixed[channel] = TRUE;
    }
}
uint32 SimAdc_GetConversionCount(void)            { return s_adc.conversions; }

static uint32 adc_sample(uint32 ch)
{
    if (ch >= 19u) {
        return 0u;
    }
    if ((ch == SIM_ADC_WHEEL_CHANNEL) && !s_adc.wheelOverride) {
        return SimAdc_ProfileRaw((uint32)(SimTime_GetUs() / 1000u));      /* virtual time (ms since boot) */
    }
    return s_adc.chRaw[ch];
}

static uint32 adc_read(uint32 off, void *ctx)
{
    (void)ctx;
    switch (off) {
    case 0x00u: return s_adc.isr;
    case 0x04u: return s_adc.ier;
    case 0x08u: return s_adc.cr;
    case 0x0Cu: return s_adc.cfgr;
    case 0x14u: return s_adc.smpr1;
    case 0x18u: return s_adc.smpr2;
    case 0x30u: return s_adc.sqr1;
    case 0x40u: s_adc.isr &= ~ADC_ISR_EOC; return s_adc.dr;               /* reading DR clears EOC (RM0438) */
    case 0x308u: return s_adc.ccr;
    default:    return 0u;
    }
}

static void adc_write(uint32 off, uint32 v, void *ctx)
{
    (void)ctx;
    switch (off) {
    case 0x00u: s_adc.isr &= ~v; break;                                  /* write 1 to clear */
    case 0x04u: s_adc.ier = v; break;
    case 0x08u: {
        uint32 cr = s_adc.cr;
        cr = (cr & ~(ADC_CR_DEEPPWD | ADC_CR_ADVREGEN)) | (v & (ADC_CR_DEEPPWD | ADC_CR_ADVREGEN));
        if (((v & ADC_CR_ADEN) != 0u) && ((cr & ADC_CR_DEEPPWD) == 0u) && ((cr & ADC_CR_ADVREGEN) != 0u)) {
            cr |= ADC_CR_ADEN;
            s_adc.isr |= ADC_ISR_ADRDY;
        }
        if (((v & ADC_CR_ADDIS) != 0u) && ((cr & ADC_CR_ADEN) != 0u)) {
            cr &= ~ADC_CR_ADEN;
        }
        if (((v & ADC_CR_ADSTART) != 0u) && ((cr & ADC_CR_ADEN) != 0u)) {     /* instant conversion of SQ1 */
            s_adc.dr = adc_sample((s_adc.sqr1 >> 6) & 0x1Fu) & 0xFFFu;
            s_adc.isr |= ADC_ISR_EOSMP | ADC_ISR_EOC | ADC_ISR_EOS;
            s_adc.conversions++;
        }
        s_adc.cr = cr & ~(ADC_CR_ADSTART | ADC_CR_ADSTP | ADC_CR_ADCAL | ADC_CR_ADDIS);
        break; }
    case 0x0Cu: s_adc.cfgr = v; break;
    case 0x14u: s_adc.smpr1 = v; break;
    case 0x18u: s_adc.smpr2 = v; break;
    case 0x30u: s_adc.sqr1 = v; break;
    case 0x308u: s_adc.ccr = v; break;
    default: break;
    }
}

/* ------------------------------------------------------------------------------------------------- RCC */
#define RCC_CR     0x00u
#define RCC_CFGR   0x08u
#define RCC_CSR    0x94u
typedef struct { uint32 cr, cfgr, pllcfgr, csr; } Sim_Rcc;
static Sim_Rcc s_rcc;

static uint32 rcc_read(uint32 off, void *ctx)
{
    (void)ctx;
    switch (off) {
    case RCC_CR: {                                 /* *RDY bit follows *ON bit: MSI b0->b1, HSI b8->b10, HSE b16->b17, PLL b24->b25 */
        uint32 rdy = ((s_rcc.cr & (1u << 0)) << 1) | ((s_rcc.cr & (1u << 8)) << 2) | ((s_rcc.cr & (1u << 16)) << 1) |
                     ((s_rcc.cr & (1u << 24)) << 1);
        return (s_rcc.cr & ~((1u << 1) | (1u << 10) | (1u << 17) | (1u << 25))) | rdy;
    }
    case RCC_CFGR: return (s_rcc.cfgr & ~0xCu) | ((s_rcc.cfgr & 3u) << 2);       /* SWS = SW */
    case 0x0Cu:    return s_rcc.pllcfgr;
    case RCC_CSR:  return s_rcc.csr;
    default:       return 0u;
    }
}

static uint32 s_rccPlain[0x400u / 4u];            /* all other RCC registers (AHB2ENR, APB1ENR1, CCIPR1 ...) are plain read/write */

static uint32 rcc_read_all(uint32 off, void *ctx)
{
    if ((off == RCC_CR) || (off == RCC_CFGR) || (off == 0x0Cu) || (off == RCC_CSR)) {
        return rcc_read(off, ctx);
    }
    return s_rccPlain[(off >> 2) % (0x400u / 4u)];
}

static void rcc_write(uint32 off, uint32 v, void *ctx)
{
    (void)ctx;
    switch (off) {
    case RCC_CR:   s_rcc.cr = v; break;
    case RCC_CFGR: s_rcc.cfgr = v; break;
    case 0x0Cu:    s_rcc.pllcfgr = v; break;
    case RCC_CSR:
        s_rcc.csr = (s_rcc.csr & 0xFF000000u) | (v & 0x007FFFFFu);          /* low bits plain; flags 24..31 are read-only ... */
        if ((v & (1u << 23)) != 0u) {                                       /* ... and cleared by RMVF */
            s_rcc.csr &= 0x00FFFFFFu;
        }
        break;
    default:       s_rccPlain[(off >> 2) % (0x400u / 4u)] = v; break;
    }
}

/* ------------------------------------------------------------------------------------------------- init */
void SimPeripherals_Init(void)
{
    uint32 i;
    SimMmio_Reset();
    gpio_reset();
    (void)memset(&s_adc, 0, sizeof(s_adc));        /* calibration (ADCAL) completes at once: the bit is never kept */
    (void)memset(s_rccPlain, 0, sizeof(s_rccPlain));
    s_rcc.cr      = 0x00000001u;                   /* MSION (MSIRDY derived); the real reset value is 0x63 */
    s_rcc.cfgr    = 0u;
    s_rcc.pllcfgr = 0x00001000u;
    s_rcc.csr     = 0x08000000u;                   /* BORRSTF: power-on reset */
    for (i = 0u; i < NUM_GPIO; i++) {
        SimMmio_Register(GPIO_BASE0 + i * GPIO_STRIDE, GPIO_STRIDE, gpio_read, gpio_write, (void *)(uintptr_t)i);
    }
    SimMmio_Register(ADC1_BASE, 0x400u, adc_read, adc_write, NULL_PTR);
    SimMmio_Register(RCC_BASE, 0x400u, rcc_read_all, rcc_write, NULL_PTR);
}
