/*
 * RestBus_Cfg.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: generated Mcu_Cfg.c / Port_Cfg.c / Can_Cfg.c. Hand written for the rest-bus node
 * (same pin map and CAN parameters as the ECU configurations in config/ecuc/<Ecu>.ecuc.json: USART1 on PA9/PA10, FDCAN1 on
 * PD1/PD0, 500 kbit/s). RX is polled (rxInterrupt = FALSE), so the node needs no NVIC setup and no OS.
 */
#include "Mcu.h"
#include "Port.h"
#include "Can.h"

const Mcu_ConfigType Mcu_Config = { 80000000u, 1u };

static const Port_PinConfigType restbus_pins[PORT_NUM_PINS] = {
    /* pin                            direction     mode dirChg level     pull            openDrain */
    { PortConf_PortPin_Pin_Usart1Tx, PORT_PIN_OUT, 7u, FALSE, STD_HIGH, PORT_PULL_NONE, FALSE },
    { PortConf_PortPin_Pin_Usart1Rx, PORT_PIN_IN,  7u, FALSE, STD_LOW,  PORT_PULL_UP,   FALSE },
    { PortConf_PortPin_Pin_CanTx,    PORT_PIN_OUT, 9u, FALSE, STD_HIGH, PORT_PULL_NONE, FALSE },
    { PortConf_PortPin_Pin_CanRx,    PORT_PIN_IN,  9u, FALSE, STD_LOW,  PORT_PULL_UP,   FALSE }
};
const Port_ConfigType Port_Config = { PORT_NUM_PINS, restbus_pins };

static const Can_HohConfigType restbus_hoh[CAN_NUM_HOH] = {
    { CanConf_CanHardwareObject_Hrh_VehicleSpeed, CAN_HOH_RECEIVE,  0x101u, 0x7FFu, 0u },
    { CanConf_CanHardwareObject_Hrh_LightStatus,  CAN_HOH_RECEIVE,  0x201u, 0x7FFu, 0u },
    { CanConf_CanHardwareObject_Hth_RestBus,      CAN_HOH_TRANSMIT, 0x000u, 0x7FFu, 0u }
};
const Can_ConfigType Can_Config = { 500000u, FALSE, CAN_NUM_HOH, restbus_hoh };
