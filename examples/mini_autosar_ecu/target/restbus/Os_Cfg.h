/*
 * Os_Cfg.h (hand-written placeholder for the REST-BUS node only)
 *
 * [Educational Implementation]
 * The rest-bus firmware (target/restbus) has no OS and no generated configuration, but it links the MCAL Can driver whose
 * critical sections go through SchM.h -> Os.h -> Os_Cfg.h. This empty configuration satisfies that include chain.
 * The ECU images use the REAL generated gen/<Ecu>/Os_Cfg.h instead (generator/emit_os.py).
 */
#ifndef OS_CFG_H
#define OS_CFG_H
#define OS_NUM_TASKS        0u
#define OS_NUM_COUNTERS     0u
#define OS_NUM_ALARMS       0u
#define OS_NUM_RESOURCES    0u
#define OS_NUM_ISRS         0u
#define OS_USE_STARTUPHOOK  0
#define OS_USE_ERRORHOOK    0
#define OS_USE_SHUTDOWNHOOK 0
#define OS_USE_PRETASKHOOK  0
#define OS_USE_POSTTASKHOOK 0
#endif
