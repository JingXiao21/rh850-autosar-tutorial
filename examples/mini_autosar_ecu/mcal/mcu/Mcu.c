/*
 * Mcu.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: MCU Driver (AUTOSAR_CP_SWS_MCUDriver, R25-11). Implemented: Mcu_Init, Mcu_InitClock, Mcu_DistributePllClock,
 * Mcu_GetPllStatus, Mcu_GetResetReason / Mcu_GetResetRawValue, Mcu_PerformReset, Mcu_SetMode (stub), Mcu_GetVersionInfo.
 * Not implemented: RAM section initialisation (Mcu_InitRamSection), power modes, clock monitoring, multiple clock settings.
 *
 * Hardware: STM32L552 RCC (target) - MSI 4 MHz -> PLL (M=1, N=sysClockHz/2MHz, R=/2) -> SYSCLK, FLASH_ACR wait states.
 * The same code runs on the host build against the SimMmio RCC model (sim/host/SimPeripherals.c), which mimics the PLLRDY / SWS handshake.
 * RH850 analogue: Clock Generator + PLL (CLKD/PLL0CTL...), the Mcu_InitClock / Mcu_DistributePllClock split exists for the same reason
 * (docs/01-rh850 clock chapters; docs/11-classic-autosar-primer/08 MCAL): PLL is started and locked first, SYSCLK switched afterwards.
 *
 * Register facts (RM0438 offsets; [V] = verified in Renode 1.17.0 STM32L5_RCC via target/renode/probe, [R] = reference manual only):
 *   RCC_CR    0x00  MSION b0, MSIRDY b1, PLLON b24, PLLRDY b25            [V] PLLRDY follows PLLON immediately in Renode
 *   RCC_CFGR  0x08  SW[1:0], SWS[3:2]  (3 = PLL)                          [V] SWS follows SW
 *   RCC_PLLCFGR 0x0C PLLSRC[1:0]=1 (MSI), PLLM[7:4], PLLN[14:8], PLLQEN b20, PLLQ[22:21], PLLREN b24, PLLR[26:25]   [V] read-back ok
 *   RCC_CSR   0x94  reset flags BORRSTF b27, SFTRSTF b28, IWDGRSTF b29, WWDGRSTF b30; RMVF b23   [R] (Renode reads 0 -> UNDEFINED)
 *   FLASH_ACR 0x40022000 LATENCY[2:0]                                      [V] read-back ok in Renode
 * Spec: AUTOSAR_CP_SWS_MCUDriver SWS_Mcu_00153 Mcu_Init, SWS_Mcu_00155 Mcu_InitClock, SWS_Mcu_00156 Mcu_DistributePllClock,
 *       SWS_Mcu_00157 Mcu_GetPllStatus, SWS_Mcu_00158 Mcu_GetResetReason, SWS_Mcu_00160 Mcu_PerformReset, SWS_Mcu_00161 Mcu_SetMode,
 *       SWS_Mcu_00162 Mcu_GetVersionInfo.
 */
#include "Mcu.h"
#include "Mmio.h"
#include "Det.h"
#include "Trace.h"
#include "Mini_ModuleIds.h"

#define MCU_E_PARAM_CONFIG    0x0Au
#define MCU_E_PARAM_CLOCK     0x0Bu
#define MCU_E_PARAM_MODE      0x0Cu
#define MCU_E_PLL_NOT_LOCKED  0x0Eu
#define MCU_E_UNINIT          0x0Fu
#define MCU_E_PARAM_POINTER   0x10u
#define MCU_E_INIT_FAILED     0x11u

#define MCU_SID_INIT          0x00u
#define MCU_SID_INITCLOCK     0x01u
#define MCU_SID_DISTRIBUTEPLL 0x03u
#define MCU_SID_GETPLLSTATUS  0x04u
#define MCU_SID_GETRESETREASON 0x05u
#define MCU_SID_GETRESETRAW   0x06u
#define MCU_SID_PERFORMRESET  0x07u
#define MCU_SID_SETMODE       0x08u
#define MCU_SID_GETVERSION    0x09u

#define RCC_BASE        0x40021000u
#define RCC_CR          (RCC_BASE + 0x00u)
#define RCC_CFGR        (RCC_BASE + 0x08u)
#define RCC_PLLCFGR     (RCC_BASE + 0x0Cu)
#define RCC_CSR         (RCC_BASE + 0x94u)
#define FLASH_ACR       0x40022000u
#define SCB_AIRCR       0xE000ED0Cu

#define CR_MSION        (1u << 0)
#define CR_MSIRDY       (1u << 1)
#define CR_PLLON        (1u << 24)
#define CR_PLLRDY       (1u << 25)
#define CSR_RMVF        (1u << 23)
#define CSR_BORRSTF     (1u << 27)
#define CSR_SFTRSTF     (1u << 28)
#define CSR_IWDGRSTF    (1u << 29)
#define CSR_WWDGRSTF    (1u << 30)

#define MCU_MSI_HZ          4000000u
#define MCU_WAIT_LOOPS      100000u         /* bounded polling (SWS_Mcu_00155 timeouts); the host model answers at once */
#define MCU_DET(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_MCU, 0u, (api), (err)); } } while (0)

static const Mcu_ConfigType *mcu_cfg;
static boolean               mcu_pllStarted;
static uint32                mcu_sysClockHz = MCU_MSI_HZ;     /* MSI is the reset clock */
static Mcu_RawResetType      mcu_rawReset;
static Mcu_ResetType         mcu_resetReason = MCU_RESET_UNDEFINED;

static boolean mcu_wait_bits(uint32 addr, uint32 mask, uint32 value)
{
    uint32 n;
    for (n = 0u; n < MCU_WAIT_LOOPS; n++) {
        if ((Mmio_Read32(addr) & mask) == value) {
            return TRUE;
        }
    }
    return FALSE;
}

void Mcu_Init(const Mcu_ConfigType *ConfigPtr)
{
    uint32 csr;
    if (ConfigPtr == NULL_PTR) {
        MCU_DET(MCU_SID_INIT, MCU_E_PARAM_CONFIG);
        return;
    }
    mcu_cfg        = ConfigPtr;
    mcu_pllStarted = FALSE;
    mcu_sysClockHz = MCU_MSI_HZ;

    csr = Mmio_Read32(RCC_CSR);                              /* latch the reset cause, then clear the flags (RMVF) */
    mcu_rawReset = (Mcu_RawResetType)(csr & 0xFF800000u);
    if ((csr & CSR_BORRSTF) != 0u)                    { mcu_resetReason = MCU_POWER_ON_RESET; }
    else if ((csr & (CSR_IWDGRSTF | CSR_WWDGRSTF)) != 0u) { mcu_resetReason = MCU_WATCHDOG_RESET; }
    else if ((csr & CSR_SFTRSTF) != 0u)               { mcu_resetReason = MCU_SW_RESET; }
    else                                              { mcu_resetReason = MCU_RESET_UNDEFINED; }
    Mmio_Modify32(RCC_CSR, 0u, CSR_RMVF);
}

Std_ReturnType Mcu_InitClock(Mcu_ClockType ClockSetting)
{
    uint32 hz;
    uint32 n;
    uint32 ws;

    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_INITCLOCK, MCU_E_UNINIT);
        return E_NOT_OK;
    }
    if (ClockSetting >= mcu_cfg->clockSettings) {
        MCU_DET(MCU_SID_INITCLOCK, MCU_E_PARAM_CLOCK);
        return E_NOT_OK;
    }
    hz = mcu_cfg->sysClockHz;
    if (((hz % 2000000u) != 0u) || (hz < 16000000u) || (hz > 80000000u)) {   /* VCO = 4 MHz * N, SYSCLK = VCO / 2, range 1: <= 80 MHz */
        MCU_DET(MCU_SID_INITCLOCK, MCU_E_PARAM_CLOCK);
        return E_NOT_OK;
    }
    n  = hz / 2000000u;                                      /* PLLN: 4 MHz * N / 2 = hz */
    ws = (hz - 1u) / 16000000u;                              /* flash wait states for voltage range 1: 0 WS up to 16 MHz ... 4 WS at 80 MHz [R] */

    Mmio_Modify32(RCC_CR, 0u, CR_MSION);                     /* reference clock: MSI is on after reset on silicon ... */
    if (!mcu_wait_bits(RCC_CR, CR_MSIRDY, CR_MSIRDY)) {
        /* ... but Renode's STM32L5_RCC resets CR to 0 and may not implement MSIRDY: do not fail, the PLL ready flag below decides. */
    }
    Mmio_Modify32(FLASH_ACR, 0x7u, ws);                      /* wait states BEFORE the faster clock is selected */
    Mmio_Modify32(RCC_CR, CR_PLLON, 0u);                     /* PLLCFGR is writable only while the PLL is off */
    (void)mcu_wait_bits(RCC_CR, CR_PLLRDY, 0u);
    Mmio_Write32(RCC_PLLCFGR, 0x1u                           /* PLLSRC = MSI */
                              | (0u << 4)                    /* PLLM = /1 */
                              | (n << 8)                     /* PLLN */
                              | (1u << 20) | (0u << 21)      /* PLLQ /2 enabled: FDCAN kernel clock */
                              | (1u << 24) | (0u << 25));    /* PLLR /2 enabled: SYSCLK */
    Mmio_Modify32(RCC_CR, 0u, CR_PLLON);
    if (!mcu_wait_bits(RCC_CR, CR_PLLRDY, CR_PLLRDY)) {
        MCU_DET(MCU_SID_INITCLOCK, MCU_E_PLL_NOT_LOCKED);
        return E_NOT_OK;
    }
    mcu_pllStarted = TRUE;
    return E_OK;
}

Std_ReturnType Mcu_DistributePllClock(void)
{
    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_DISTRIBUTEPLL, MCU_E_UNINIT);
        return E_NOT_OK;
    }
    if (!mcu_pllStarted || ((Mmio_Read32(RCC_CR) & CR_PLLRDY) == 0u)) {
        MCU_DET(MCU_SID_DISTRIBUTEPLL, MCU_E_PLL_NOT_LOCKED);   /* SWS_Mcu_00156: only call with a locked PLL */
        return E_NOT_OK;
    }
    Mmio_Modify32(RCC_CFGR, 0x3u, 0x3u);                     /* SW = PLL */
    if (!mcu_wait_bits(RCC_CFGR, 0xCu, 0xCu)) {              /* SWS = PLL */
        return E_NOT_OK;
    }
    mcu_sysClockHz = mcu_cfg->sysClockHz;
    TRACE(TRACE_CAT_BSW, "MCU clock=%u", (unsigned)mcu_sysClockHz);
    return E_OK;
}

Mcu_PllStatusType Mcu_GetPllStatus(void)
{
    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_GETPLLSTATUS, MCU_E_UNINIT);
        return MCU_PLL_STATUS_UNDEFINED;
    }
    if (!mcu_pllStarted) {
        return MCU_PLL_STATUS_UNDEFINED;
    }
    return ((Mmio_Read32(RCC_CR) & CR_PLLRDY) != 0u) ? MCU_PLL_LOCKED : MCU_PLL_UNLOCKED;
}

Mcu_ResetType Mcu_GetResetReason(void)
{
    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_GETRESETREASON, MCU_E_UNINIT);
        return MCU_RESET_UNDEFINED;
    }
    return mcu_resetReason;
}

Mcu_RawResetType Mcu_GetResetRawValue(void)
{
    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_GETRESETRAW, MCU_E_UNINIT);
        return 0u;
    }
    return mcu_rawReset;
}

void Mcu_PerformReset(void)
{
    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_PERFORMRESET, MCU_E_UNINIT);
        return;
    }
    Mmio_Write32(SCB_AIRCR, 0x05FA0004u);                    /* VECTKEY | SYSRESETREQ; on the host model this is a no-op and returns */
}

void Mcu_SetMode(Mcu_ModeType McuMode)
{
    if (mcu_cfg == NULL_PTR) {
        MCU_DET(MCU_SID_SETMODE, MCU_E_UNINIT);
        return;
    }
    if (McuMode != 0u) {                                     /* only the RUN mode (0) exists */
        MCU_DET(MCU_SID_SETMODE, MCU_E_PARAM_MODE);
    }
}

void Mcu_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo == NULL_PTR) {
        MCU_DET(MCU_SID_GETVERSION, MCU_E_PARAM_POINTER);
        return;
    }
    versioninfo->vendorID         = MINI_VENDOR_ID;
    versioninfo->moduleID         = MINI_MODULE_MCU;
    versioninfo->sw_major_version = 1u;
    versioninfo->sw_minor_version = 0u;
    versioninfo->sw_patch_version = 0u;
}

uint32 Mcu_GetSystemClockHz(void)
{
    return mcu_sysClockHz;
}
