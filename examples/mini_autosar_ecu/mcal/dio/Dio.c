/*
 * Dio.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: DIO Driver (AUTOSAR_CP_SWS_DIODriver, R25-11). Implemented: Dio_ReadChannel, Dio_WriteChannel,
 * Dio_FlipChannel, Dio_ReadPort, Dio_WritePort, Dio_GetVersionInfo. Not implemented: channel groups, Dio_MaskedWritePort.
 * The channel id is the Port_PinType number (port_index*16 + pin), so the Dio_Cfg.h channel macros are just pin numbers.
 *
 * Hardware: STM32L552 GPIOx: BSRR 0x18 (atomic set bits 0..15 / reset bits 16..31), IDR 0x10, ODR 0x14, MODER 0x00. Reads of an OUTPUT
 * channel return ODR (what the driver commands), reads of an input return IDR (what the pad shows) - this is also what a real DIO driver
 * does for "level read-back of outputs". Writes always go through BSRR so no read-modify-write race with an ISR is possible.
 * RH850 analogue: Pn (port register) / PSRn (set/reset register) / PPRn (pin read), see docs/01-rh850 and docs/04-can-mcal/04-can-pin-transceiver.md.
 * Verified in Renode (STM32_GPIOPort): BSRR -> ODR, ODR -> IDR for output pins (target/renode/probe). The Dio driver never configures a pin:
 * direction/mode belong to Port (Port_Init).
 * Spec: AUTOSAR_CP_SWS_DIODriver SWS_Dio_00133 Dio_ReadChannel, SWS_Dio_00134 Dio_WriteChannel, SWS_Dio_00135 Dio_ReadPort,
 *       SWS_Dio_00136 Dio_WritePort, SWS_Dio_00190 Dio_FlipChannel; errors DIO_E_PARAM_INVALID_CHANNEL_ID 0x0A, _PORT_ID 0x14.
 */
#include "Dio.h"
#include "Mmio.h"
#include "Det.h"
#include "Mini_ModuleIds.h"

#define DIO_E_PARAM_INVALID_CHANNEL_ID  0x0Au
#define DIO_E_PARAM_INVALID_PORT_ID     0x14u
#define DIO_E_PARAM_POINTER             0x20u

#define DIO_SID_READCHANNEL   0x00u
#define DIO_SID_WRITECHANNEL  0x01u
#define DIO_SID_READPORT      0x02u
#define DIO_SID_WRITEPORT     0x03u
#define DIO_SID_GETVERSION    0x12u
#define DIO_SID_FLIPCHANNEL   0x11u

#define GPIO_BASE(port)  (0x42020000u + 0x400u * (uint32)(port))
#define GPIO_MODER       0x00u
#define GPIO_IDR         0x10u
#define GPIO_ODR         0x14u
#define GPIO_BSRR        0x18u
#define DIO_NUM_PORTS    8u

#define DIO_DET(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_DIO, 0u, (api), (err)); } } while (0)

static boolean dio_channel_valid(Dio_ChannelType ch, uint8 api)
{
    if ((ch >> 4) >= DIO_NUM_PORTS) {
        DIO_DET(api, DIO_E_PARAM_INVALID_CHANNEL_ID);
        return FALSE;
    }
    return TRUE;
}

static boolean dio_is_output(Dio_ChannelType ch)
{
    uint32 n = (uint32)(ch & 15u);
    return (((Mmio_Read32(GPIO_BASE(ch >> 4) + GPIO_MODER) >> (2u * n)) & 3u) == 1u) ? TRUE : FALSE;
}

Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId)
{
    uint32 base;
    uint32 reg;
    if (!dio_channel_valid(ChannelId, DIO_SID_READCHANNEL)) {
        return STD_LOW;
    }
    base = GPIO_BASE(ChannelId >> 4);
    reg  = dio_is_output(ChannelId) ? GPIO_ODR : GPIO_IDR;
    return (((Mmio_Read32(base + reg) >> (ChannelId & 15u)) & 1u) != 0u) ? STD_HIGH : STD_LOW;
}

void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level)
{
    uint32 n;
    if (!dio_channel_valid(ChannelId, DIO_SID_WRITECHANNEL)) {
        return;
    }
    n = (uint32)(ChannelId & 15u);
    Mmio_Write32(GPIO_BASE(ChannelId >> 4) + GPIO_BSRR, (Level == STD_HIGH) ? (1u << n) : (1u << (n + 16u)));
}

Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId)
{
    Dio_LevelType now;
    if (!dio_channel_valid(ChannelId, DIO_SID_FLIPCHANNEL)) {
        return STD_LOW;
    }
    now = Dio_ReadChannel(ChannelId);
    Dio_WriteChannel(ChannelId, (now == STD_HIGH) ? STD_LOW : STD_HIGH);
    return (now == STD_HIGH) ? STD_LOW : STD_HIGH;                   /* SWS_Dio_00191: the NEW level */
}

Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId)
{
    if (PortId >= DIO_NUM_PORTS) {
        DIO_DET(DIO_SID_READPORT, DIO_E_PARAM_INVALID_PORT_ID);
        return 0u;
    }
    return (Dio_PortLevelType)(Mmio_Read32(GPIO_BASE(PortId) + GPIO_IDR) & 0xFFFFu);
}

void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level)
{
    if (PortId >= DIO_NUM_PORTS) {
        DIO_DET(DIO_SID_WRITEPORT, DIO_E_PARAM_INVALID_PORT_ID);
        return;
    }
    /* All 16 pins in one atomic BSRR write: bits to set in [15:0], bits to clear in [31:16]. */
    Mmio_Write32(GPIO_BASE(PortId) + GPIO_BSRR, (uint32)Level | ((uint32)(uint16)(~Level) << 16));
}

void Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo)
{
    if (VersionInfo == NULL_PTR) {
        DIO_DET(DIO_SID_GETVERSION, DIO_E_PARAM_POINTER);
        return;
    }
    VersionInfo->vendorID         = MINI_VENDOR_ID;
    VersionInfo->moduleID         = MINI_MODULE_DIO;
    VersionInfo->sw_major_version = 1u;
    VersionInfo->sw_minor_version = 0u;
    VersionInfo->sw_patch_version = 0u;
}
