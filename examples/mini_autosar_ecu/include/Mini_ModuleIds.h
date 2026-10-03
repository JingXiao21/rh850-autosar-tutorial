/*
 * Mini_ModuleIds.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: <Module>_MODULE_ID in each module header (values from the AUTOSAR BSW
 * module list). IoHwAb has no standard ID (project specific): 0xFE.
 * Used as ModuleId of Det_ReportError() / Det_ReportRuntimeError().
 */
#ifndef MINI_MODULEIDS_H
#define MINI_MODULEIDS_H

#define MINI_MODULE_OS       1u
#define MINI_MODULE_RTE      2u
#define MINI_MODULE_ECUM     10u
#define MINI_MODULE_DET      15u
#define MINI_MODULE_BSWM     42u
#define MINI_MODULE_COM      50u
#define MINI_MODULE_PDUR     51u
#define MINI_MODULE_CANIF    60u
#define MINI_MODULE_CAN      80u
#define MINI_MODULE_MCU      101u
#define MINI_MODULE_DIO      120u
#define MINI_MODULE_ADC      123u
#define MINI_MODULE_PORT     124u
#define MINI_MODULE_SCHM     130u
#define MINI_MODULE_IOHWAB   0xFEu
#define MINI_VENDOR_ID       0xFFFFu    /* educational: no registered vendor id */

/* Det error ids shared by all modules (real ones are per module in each SWS) */
#define MINI_E_UNINIT              0x01u
#define MINI_E_PARAM_POINTER       0x02u
#define MINI_E_PARAM_INVALID       0x03u
#define MINI_E_ALREADY_INITIALIZED 0x04u

#endif /* MINI_MODULEIDS_H */
