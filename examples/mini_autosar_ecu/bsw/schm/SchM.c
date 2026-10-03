/*
 * SchM.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: BSW Scheduler (generated together with the RTE; AUTOSAR_CP_SWS_RTE chapter
 * "BSW Scheduler" 5.8.x, AUTOSAR_CP_SWS_BSWGeneral). Implemented: SchM_Start / SchM_Init / SchM_StartTiming /
 * SchM_Deinit as trace-only lifecycle calls (the StartPostOS sequence of EcuM SWS 7.3.3: SchM_Start ->
 * BswM_Init -> SchM_Init -> SchM_StartTiming). Exclusive areas are macros in SchM.h (mapped onto the OS
 * interrupt-lock services); the MainFunction task bodies are generated into Rte_Tasks.c.
 * Not implemented: SchM_ConfigType, mode-switch notification ports, trigger APIs.
 */
#include "SchM.h"
#include "Trace.h"

static boolean schm_started;

Std_ReturnType SchM_Start(void)
{
    schm_started = TRUE;
    TRACE(TRACE_CAT_BSW, "SCHM START");
    return E_OK;
}

void SchM_Init(void)
{
    TRACE(TRACE_CAT_BSW, "SCHM INIT");
}

void SchM_StartTiming(void)
{
    /* Real SchM starts its own timing sources (alarms/schedule tables) here. In this project the OS alarms
     * Alarm_BswMain/... have autostart in the OS configuration, so only a trace line remains. */
    TRACE(TRACE_CAT_BSW, "SCHM TIMING");
}

void SchM_Deinit(void)
{
    schm_started = FALSE;
    TRACE(TRACE_CAT_BSW, "SCHM DEINIT");
}
