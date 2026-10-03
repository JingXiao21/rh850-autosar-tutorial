/*
 * Dio.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: DIO Driver (AUTOSAR_CP_SWS_DIODriver). Implemented: Dio_ReadChannel,
 * Dio_WriteChannel, Dio_FlipChannel, Dio_ReadPort, Dio_WritePort, Dio_GetVersionInfo (no channel
 * groups, no masked writes). Writes use GPIOx->BSRR (atomic set/reset); reads use IDR (input) or
 * ODR (output channel). Channel id = port_index*16 + pin (same numbering as Port_PinType).
 * Owner: agent B.
 */
#ifndef DIO_H
#define DIO_H

#include "Std_Types.h"
#include "Dio_Cfg.h"          /* generated: DioConf_DioChannel_* ids */

typedef uint16 Dio_ChannelType;
typedef uint8  Dio_PortType;
typedef uint8  Dio_LevelType;         /* STD_HIGH / STD_LOW */
typedef uint16 Dio_PortLevelType;

Dio_LevelType     Dio_ReadChannel(Dio_ChannelType ChannelId);
void              Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);
Dio_LevelType     Dio_FlipChannel(Dio_ChannelType ChannelId);
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);
void              Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);
void              Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo);

#endif /* DIO_H */
