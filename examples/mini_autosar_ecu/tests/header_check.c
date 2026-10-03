/* [Educational Implementation] header_check.c: compiles every public header together (host + target,
 * -std=c99 -Wall -Wextra -Werror -pedantic). Build via tools/run_mini_autosar.py --step headers. */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Platform_Types.h"
#include "Compiler.h"
#include "MemMap.h"
#include "Mini_Cfg.h"
#include "Mini_ModuleIds.h"
#include "Trace.h"
#include "Mini_Time.h"
#include "Os.h"
#include "Os_CfgTypes.h"
#include "Os_Port.h"
#include "Mmio.h"
#include "Mcu.h"
#include "Port.h"
#include "Dio.h"
#include "Adc.h"
#include "Can.h"
#include "CanIf.h"
#include "CanIf_Cbk.h"
#include "IoHwAb.h"
#include "Det.h"
#include "SchM.h"
#include "EcuM.h"
#include "BswM.h"
#include "Com.h"
#include "PduR.h"
#include "Rte_Common.h"
#ifdef MINI_PLATFORM_HOST
#include "SimTime.h"
#include "SimMmio.h"
#include "SimCan.h"
#endif
#include "RestBus.h"

int header_check_dummy(void);
int header_check_dummy(void)
{
    PduInfoType p = { NULL_PTR, NULL_PTR, 0u };
    Com_SignalConfigType s;
    (void)p; (void)s;
    return (int)sizeof(Os_ConfigType) + (int)RTE_E_OK + (int)E_OS_ID;
}
