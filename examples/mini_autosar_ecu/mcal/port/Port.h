/*
 * Port.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Port Driver (AUTOSAR_CP_SWS_PortDriver). Implemented subset: Port_Init,
 * Port_SetPinDirection, Port_RefreshPortDirection, Port_SetPinMode. Hardware: STM32L552 GPIOx
 * (MODER/OTYPER/OSPEEDR/PUPDR/ODR/AFRx) + RCC AHB2ENR clock gate. Owner: agent B.
 * Pin numbering: Port_PinType = port_index*16 + pin (PA0 = 0, PB7 = 23, PC7 = 39).
 */
#ifndef PORT_H
#define PORT_H

#include "Std_Types.h"
#include "Port_Cfg.h"         /* generated: PortConf_PortPin_* ids */

typedef uint16 Port_PinType;
typedef uint8  Port_PinModeType;      /* 0 = GPIO; 1..15 = alternate function number (AFx) */

typedef enum { PORT_PIN_IN = 0, PORT_PIN_OUT = 1, PORT_PIN_ANALOG = 2 } Port_PinDirectionType;
typedef enum { PORT_PULL_NONE = 0, PORT_PULL_UP = 1, PORT_PULL_DOWN = 2 } Port_PinPullType;

typedef struct {
    Port_PinType          pin;
    Port_PinDirectionType direction;
    Port_PinModeType      mode;
    boolean               directionChangeable;   /* PortPinDirectionChangeable */
    uint8                 initialLevel;          /* STD_HIGH / STD_LOW (output pins) */
    Port_PinPullType      pull;
    boolean               openDrain;
} Port_PinConfigType;

typedef struct {
    uint16                    numPins;
    const Port_PinConfigType *pins;
} Port_ConfigType;
extern const Port_ConfigType Port_Config;        /* generated Port_Cfg.c */

void Port_Init(const Port_ConfigType *ConfigPtr);
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);
void Port_RefreshPortDirection(void);
void Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode);
void Port_GetVersionInfo(Std_VersionInfoType *versioninfo);

#endif /* PORT_H */
