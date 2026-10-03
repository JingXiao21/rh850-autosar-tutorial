// SOURCES: mcal/mcu/Mcu.c sim/host/SimMmio.c sim/host/SimPeripherals.c
/*
 * test_mcal_mcu.c
 *
 * [Educational Implementation]
 * Unit test of the Mcu MCAL driver on the HOST build against the simulated RCC (ready flags follow the ON bits, SWS follows SW).
 * Real AUTOSAR counterpart: MCAL module test of SWS_Mcu_00153 Mcu_Init, 00155 Mcu_InitClock, 00156 Mcu_DistributePllClock,
 * 00157 Mcu_GetPllStatus, 00158 Mcu_GetResetReason, 00161 Mcu_SetMode. Checks: Det errors (uninit, bad clock setting, PLL not locked,
 * bad mode), PLL programming for 80 MHz and 48 MHz, flash wait states, the PLL-lock / switch-over sequence, reset reason + RMVF.
 */
#include "mcal_test_support.h"
#include "Mcu.h"
#include "Mmio.h"
#include "SimMmio.h"

#define RCC_CR      0x40021000u
#define RCC_CFGR    0x40021008u
#define RCC_PLLCFGR 0x4002100Cu
#define RCC_CSR     0x40021094u
#define FLASH_ACR   0x40022000u

static const Mcu_ConfigType cfg80 = { 80000000u, 1u };
static const Mcu_ConfigType cfg48 = { 48000000u, 1u };
static const Mcu_ConfigType cfgBad = { 81000000u, 1u };

int main(void)
{
    SimPeripherals_Init();

    /* not initialised */
    T_CHECK(Mcu_GetPllStatus() == MCU_PLL_STATUS_UNDEFINED);
    T_CHECK((ts_det_count == 1) && (ts_det_last_module == MINI_MODULE_MCU) && (ts_det_last_error == 0x0Fu));
    T_CHECK(Mcu_InitClock(0u) == E_NOT_OK);
    T_CHECK(ts_det_count == 2);
    Mcu_Init(NULL_PTR);
    T_CHECK((ts_det_count == 3) && (ts_det_last_error == 0x0Au));    /* MCU_E_PARAM_CONFIG */

    Mcu_Init(&cfg80);
    T_CHECK(ts_det_count == 3);
    T_CHECK(Mcu_GetResetReason() == MCU_POWER_ON_RESET);             /* sim RCC_CSR: BORRSTF */
    T_CHECK((Mcu_GetResetRawValue() & (1u << 27)) != 0u);
    T_CHECK((Mmio_Read32(RCC_CSR) & 0xFF000000u) == 0u);             /* flags removed with RMVF */
    T_CHECK(Mcu_GetPllStatus() == MCU_PLL_STATUS_UNDEFINED);         /* PLL not started yet */
    T_CHECK(Mcu_GetSystemClockHz() == 4000000u);                     /* MSI */

    /* DistributePllClock before InitClock: PLL not locked */
    T_CHECK(Mcu_DistributePllClock() == E_NOT_OK);
    T_CHECK((ts_det_count == 4) && (ts_det_last_error == 0x0Eu));
    T_CHECK((Mmio_Read32(RCC_CFGR) & 0xCu) == 0u);                   /* still on MSI */

    T_CHECK(Mcu_InitClock(1u) == E_NOT_OK);                          /* only clock setting 0 exists */
    T_CHECK((ts_det_count == 5) && (ts_det_last_error == 0x0Bu));    /* MCU_E_PARAM_CLOCK */

    T_CHECK(Mcu_InitClock(McuConf_McuClockSettingConfig_0) == E_OK);
    T_CHECK((Mmio_Read32(FLASH_ACR) & 7u) == 4u);                    /* 4 wait states at 80 MHz */
    T_CHECK(((Mmio_Read32(RCC_PLLCFGR) >> 8) & 0x7Fu) == 40u);       /* PLLN: 4 MHz * 40 / 2 = 80 MHz */
    T_CHECK((Mmio_Read32(RCC_PLLCFGR) & 3u) == 1u);                  /* PLLSRC = MSI */
    T_CHECK((Mmio_Read32(RCC_PLLCFGR) & (1u << 24)) != 0u);          /* PLLREN */
    T_CHECK((Mmio_Read32(RCC_CR) & (1u << 25)) != 0u);               /* PLLRDY */
    T_CHECK(Mcu_GetPllStatus() == MCU_PLL_LOCKED);
    T_CHECK((Mmio_Read32(RCC_CFGR) & 0xCu) == 0u);                   /* PLL started but SYSCLK not switched yet */
    T_CHECK(Mcu_GetSystemClockHz() == 4000000u);

    T_CHECK(Mcu_DistributePllClock() == E_OK);
    T_CHECK((Mmio_Read32(RCC_CFGR) & 3u) == 3u);                     /* SW = PLL */
    T_CHECK((Mmio_Read32(RCC_CFGR) & 0xCu) == 0xCu);                 /* SWS = PLL */
    T_CHECK(Mcu_GetSystemClockHz() == 80000000u);
    T_CHECK(ts_trace_has("MCU clock=80000000"));

    /* re-init with another frequency */
    Mcu_Init(&cfg48);
    T_CHECK(Mcu_GetPllStatus() == MCU_PLL_STATUS_UNDEFINED);
    T_CHECK(Mcu_InitClock(0u) == E_OK);
    T_CHECK((Mmio_Read32(FLASH_ACR) & 7u) == 2u);                    /* 2 wait states at 48 MHz */
    T_CHECK(((Mmio_Read32(RCC_PLLCFGR) >> 8) & 0x7Fu) == 24u);
    T_CHECK(Mcu_DistributePllClock() == E_OK);
    T_CHECK(Mcu_GetSystemClockHz() == 48000000u);

    /* unsupported frequency */
    Mcu_Init(&cfgBad);
    {
        int c = ts_det_count;
        T_CHECK(Mcu_InitClock(0u) == E_NOT_OK);
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x0Bu));
    }

    /* modes, reset, version */
    {
        int c = ts_det_count;
        Mcu_SetMode(0u);
        T_CHECK(ts_det_count == c);
        Mcu_SetMode(3u);
        T_CHECK((ts_det_count == c + 1) && (ts_det_last_error == 0x0Cu));
        Mcu_PerformReset();
        T_CHECK(Mmio_Read32(0xE000ED0Cu) == 0x05FA0004u);            /* SCB_AIRCR: VECTKEY | SYSRESETREQ */
        {
            Std_VersionInfoType vi;
            Mcu_GetVersionInfo(&vi);
            T_CHECK((vi.moduleID == MINI_MODULE_MCU) && (vi.sw_major_version == 1u));
            Mcu_GetVersionInfo(NULL_PTR);
            T_CHECK((ts_det_count == c + 2) && (ts_det_last_error == 0x10u));
        }
    }
    T_DONE();
}
