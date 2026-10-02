/*
 * EcuM.c
 *
 * [Educational Implementation] EcuM-like startup. See EcuM.h.
 * Init order matters and mirrors a real ECU: lower layers before upper
 * layers, NvM data before the users of that data, communication is started
 * (controller STARTED) only after every receiver of frames is initialized.
 */
#include "EcuM.h"
#include "Det.h"
#include "Can.h"
#include "CanIf.h"
#include "CanTp.h"
#include "PduR.h"
#include "NvM.h"
#include "Dem.h"
#include "Dcm.h"
#include "Rte.h"
#include "UdsTrace.h"

static boolean EcuM_ResetRequested;
static uint32  EcuM_ResetCount;

void EcuM_Init(void)
{
    UDS_TRACE("EcuM", "---- EcuM_Init: DriverInitZero/One (MCAL) ----");
    Det_Init(NULL_PTR);
    Can_Init(&Can_Config);                 /* MCAL                         */
    UDS_TRACE("EcuM", "---- BSW init (normally BswM action list) ----");
    CanIf_Init(&CanIf_Config);             /* ECU abstraction              */
    CanTp_Init(&CanTp_Config);             /* services: communication      */
    PduR_Init(&PduR_Config);
    NvM_Init(NULL_PTR);
    NvM_ReadAll();                         /* NV data before its users     */
    Dem_Init(NULL_PTR);
    Dcm_Init(&Dcm_Config);
    Rte_Start();                           /* SW-C init runnables          */
    /* Real stack: ComM_RequestComMode(FULL) -> CanSM -> CanIf_SetControllerMode. */
    UDS_TRACE("EcuM", "---- start communication (normally ComM/CanSM) ----");
    (void)CanIf_SetControllerMode(CanConf_CanController_CAN0, CAN_CS_STARTED);
    EcuM_ResetRequested = FALSE;
}

void EcuM_SimRequestReset(void)
{
    EcuM_ResetRequested = TRUE;
}

boolean EcuM_SimIsResetRequested(void)
{
    return EcuM_ResetRequested;
}

void EcuM_SimPerformReset(void)
{
    UDS_TRACE("EcuM", "==== simulated ECU RESET (Mcu_PerformReset) - RAM state lost, NV data kept ====");
    EcuM_ResetCount++;
    Can_DeInit();
    EcuM_Init();
}

uint32 EcuM_SimGetResetCount(void)
{
    return EcuM_ResetCount;
}
