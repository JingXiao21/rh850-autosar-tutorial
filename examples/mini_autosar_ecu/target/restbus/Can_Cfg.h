/*
 * Can_Cfg.h (REST node, hand written)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: generated Can_Cfg.h of the CAN driver. The rest-bus node is NOT an AUTOSAR ECU, so
 * its MCAL configuration is written by hand instead of being generated. It plays the role of the generated Can_Cfg.h
 * because target/restbus is first on the include path of the RestBus image (tools/run_mini_autosar.py).
 * HOH numbers: HRH and HTH share one number space (see Can.h).
 */
#ifndef CAN_CFG_H
#define CAN_CFG_H
#define CAN_NUM_HOH                                  3u
#define CanConf_CanHardwareObject_Hrh_VehicleSpeed   0u    /* 0x101, sent by SensorEcu */
#define CanConf_CanHardwareObject_Hrh_LightStatus    1u    /* 0x201, sent by LightEcu  */
#define CanConf_CanHardwareObject_Hth_RestBus        2u    /* transmits 0x301 / 0x3F0 (id is given per frame) */
#endif
