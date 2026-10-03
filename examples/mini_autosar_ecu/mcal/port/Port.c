/*
 * Port.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Port Driver (AUTOSAR_CP_SWS_PortDriver, R25-11). Implemented: Port_Init, Port_SetPinDirection,
 * Port_RefreshPortDirection, Port_SetPinMode, Port_GetVersionInfo. Not implemented: pin groups/ports as a unit, PortPinLevelValue
 * beyond the initial level, slew-rate/drive "extensions" other than a fixed speed per mode.
 *
 * Hardware: STM32L552 GPIOA..GPIOH @ 0x42020000 + 0x400*port (AHB2). Registers per pin (2 bit fields in MODER/OSPEEDR/PUPDR, 4 bit AF
 * nibbles in AFRL/AFRH, 1 bit OTYPER/ODR/ASCR):
 *   MODER 0x00 (00 in, 01 out, 10 alternate function, 11 analog)   OTYPER 0x04   OSPEEDR 0x08   PUPDR 0x0C
 *   IDR 0x10   ODR 0x14   BSRR 0x18   AFRL 0x20   AFRH 0x24   ASCR 0x2C (analog switch control, needed to connect a pin to the ADC)
 * Clock gate: RCC_AHB2ENR (0x4C) bit n enables GPIOn.
 * RH850 analogue: port groups P0..P44 with PM (direction), PMC (mode control), PFC/PFCE/PFCAE (alternate function select),
 * PU/PD, PODC (open drain): one STM32 "MODER+AFR" pair corresponds to PMC + PFCx there (docs/04-can-mcal/04-can-pin-transceiver.md).
 * Verified in Renode (STM32_GPIOPort): MODER/ODR/IDR/BSRR behave as on silicon (target/renode/probe); the clock gate, OTYPER, OSPEEDR,
 * PUPDR, AFRx and ASCR accesses are accepted but have no observable effect there ([R]: reference manual only).
 * Spec: AUTOSAR_CP_SWS_PortDriver SWS_Port_00140 Port_Init, SWS_Port_00141 Port_SetPinDirection, SWS_Port_00142
 *       Port_RefreshPortDirection, SWS_Port_00145 Port_SetPinMode.
 */
#include "Port.h"
#include "Mmio.h"
#include "Det.h"
#include "Mini_ModuleIds.h"

#define PORT_E_PARAM_PIN               0x0Au
#define PORT_E_DIRECTION_UNCHANGEABLE  0x0Bu
#define PORT_E_PARAM_INVALID_MODE      0x0Du
#define PORT_E_MODE_UNCHANGEABLE       0x0Eu
#define PORT_E_UNINIT                  0x0Fu
#define PORT_E_PARAM_POINTER           0x10u
#define PORT_E_PARAM_INVALID_DIRECTION 0x11u

#define PORT_SID_INIT                  0x00u
#define PORT_SID_SETPINDIRECTION       0x01u
#define PORT_SID_REFRESHPORTDIRECTION  0x02u
#define PORT_SID_GETVERSIONINFO        0x03u
#define PORT_SID_SETPINMODE            0x04u

#define GPIO_BASE(port)   (0x42020000u + 0x400u * (uint32)(port))
#define GPIO_MODER        0x00u
#define GPIO_OTYPER       0x04u
#define GPIO_OSPEEDR      0x08u
#define GPIO_PUPDR        0x0Cu
#define GPIO_BSRR         0x18u
#define GPIO_AFRL         0x20u
#define GPIO_AFRH         0x24u
#define GPIO_ASCR         0x2Cu
#define RCC_AHB2ENR       0x4002104Cu
#define PORT_NUM_PORTS    8u

#define PORT_DET(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_PORT, 0u, (api), (err)); } } while (0)

static const Port_ConfigType *port_cfg;

static const Port_PinConfigType *port_find(Port_PinType pin)
{
    uint16 i;
    for (i = 0u; i < port_cfg->numPins; i++) {
        if (port_cfg->pins[i].pin == pin) {
            return &port_cfg->pins[i];
        }
    }
    return NULL_PTR;
}

static void port_set_moder(Port_PinType pin, uint32 moder)
{
    uint32 n = (uint32)(pin & 15u);
    Mmio_Modify32(GPIO_BASE(pin >> 4) + GPIO_MODER, 3u << (2u * n), moder << (2u * n));
}

static void port_apply(const Port_PinConfigType *p)
{
    uint32 port = (uint32)(p->pin >> 4);
    uint32 n    = (uint32)(p->pin & 15u);
    uint32 base = GPIO_BASE(port);
    uint32 moder;

    Mmio_Modify32(RCC_AHB2ENR, 0u, 1u << port);                       /* clock gate of the port */

    if ((p->direction == PORT_PIN_OUT) && (p->mode == 0u)) {          /* initial level BEFORE the pin becomes an output: no glitch */
        Mmio_Write32(base + GPIO_BSRR, (p->initialLevel == STD_HIGH) ? (1u << n) : (1u << (n + 16u)));
    }
    Mmio_Modify32(base + GPIO_OTYPER, 1u << n, p->openDrain ? (1u << n) : 0u);
    Mmio_Modify32(base + GPIO_OSPEEDR, 3u << (2u * n), ((p->mode != 0u) ? 2u : 0u) << (2u * n));   /* alternate functions: high speed */
    Mmio_Modify32(base + GPIO_PUPDR, 3u << (2u * n), ((uint32)p->pull) << (2u * n));
    if (p->mode != 0u) {
        uint32 reg = (n < 8u) ? GPIO_AFRL : GPIO_AFRH;
        uint32 sh  = 4u * (n & 7u);
        Mmio_Modify32(base + reg, 0xFu << sh, ((uint32)p->mode & 0xFu) << sh);
    }
    Mmio_Modify32(base + GPIO_ASCR, 1u << n, (p->direction == PORT_PIN_ANALOG) ? (1u << n) : 0u);

    if (p->direction == PORT_PIN_ANALOG)      { moder = 3u; }
    else if (p->mode != 0u)                   { moder = 2u; }
    else if (p->direction == PORT_PIN_OUT)    { moder = 1u; }
    else                                      { moder = 0u; }
    port_set_moder(p->pin, moder);
}

void Port_Init(const Port_ConfigType *ConfigPtr)
{
    uint16 i;
    if (ConfigPtr == NULL_PTR) {
        PORT_DET(PORT_SID_INIT, PORT_E_PARAM_POINTER);
        return;
    }
    for (i = 0u; i < ConfigPtr->numPins; i++) {                      /* validate first: a bad table must not half-configure the pins */
        if ((ConfigPtr->pins[i].pin >> 4) >= PORT_NUM_PORTS) {
            PORT_DET(PORT_SID_INIT, PORT_E_PARAM_PIN);
            return;
        }
    }
    port_cfg = ConfigPtr;
    for (i = 0u; i < ConfigPtr->numPins; i++) {
        port_apply(&ConfigPtr->pins[i]);
    }
}

void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
    const Port_PinConfigType *p;
    if (port_cfg == NULL_PTR) {
        PORT_DET(PORT_SID_SETPINDIRECTION, PORT_E_UNINIT);
        return;
    }
    p = port_find(Pin);
    if (p == NULL_PTR) {
        PORT_DET(PORT_SID_SETPINDIRECTION, PORT_E_PARAM_PIN);
        return;
    }
    if (!p->directionChangeable) {
        PORT_DET(PORT_SID_SETPINDIRECTION, PORT_E_DIRECTION_UNCHANGEABLE);
        return;
    }
    if ((Direction != PORT_PIN_IN) && (Direction != PORT_PIN_OUT)) {
        PORT_DET(PORT_SID_SETPINDIRECTION, PORT_E_PARAM_INVALID_DIRECTION);
        return;
    }
    port_set_moder(Pin, (Direction == PORT_PIN_OUT) ? 1u : 0u);
}

void Port_RefreshPortDirection(void)
{
    uint16 i;
    if (port_cfg == NULL_PTR) {
        PORT_DET(PORT_SID_REFRESHPORTDIRECTION, PORT_E_UNINIT);
        return;
    }
    for (i = 0u; i < port_cfg->numPins; i++) {                       /* pins whose direction may not change are re-written */
        const Port_PinConfigType *p = &port_cfg->pins[i];
        if ((!p->directionChangeable) && (p->mode == 0u) && (p->direction != PORT_PIN_ANALOG)) {
            port_set_moder(p->pin, (p->direction == PORT_PIN_OUT) ? 1u : 0u);
        }
    }
}

void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode)
{
    const Port_PinConfigType *p;
    uint32 n;
    uint32 base;
    if (port_cfg == NULL_PTR) {
        PORT_DET(PORT_SID_SETPINMODE, PORT_E_UNINIT);
        return;
    }
    p = port_find(Pin);
    if (p == NULL_PTR) {
        PORT_DET(PORT_SID_SETPINMODE, PORT_E_PARAM_PIN);
        return;
    }
    if (Mode > 15u) {
        PORT_DET(PORT_SID_SETPINMODE, PORT_E_PARAM_INVALID_MODE);
        return;
    }
    if (!p->directionChangeable) {                                   /* header has no PortPinModeChangeable: same flag used */
        PORT_DET(PORT_SID_SETPINMODE, PORT_E_MODE_UNCHANGEABLE);
        return;
    }
    n    = (uint32)(Pin & 15u);
    base = GPIO_BASE(Pin >> 4);
    if (Mode == 0u) {
        port_set_moder(Pin, (p->direction == PORT_PIN_OUT) ? 1u : 0u);
    } else {
        uint32 reg = (n < 8u) ? GPIO_AFRL : GPIO_AFRH;
        uint32 sh  = 4u * (n & 7u);
        Mmio_Modify32(base + reg, 0xFu << sh, ((uint32)Mode) << sh);
        port_set_moder(Pin, 2u);
    }
}

void Port_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo == NULL_PTR) {
        PORT_DET(PORT_SID_GETVERSIONINFO, PORT_E_PARAM_POINTER);
        return;
    }
    versioninfo->vendorID         = MINI_VENDOR_ID;
    versioninfo->moduleID         = MINI_MODULE_PORT;
    versioninfo->sw_major_version = 1u;
    versioninfo->sw_minor_version = 0u;
    versioninfo->sw_patch_version = 0u;
}
