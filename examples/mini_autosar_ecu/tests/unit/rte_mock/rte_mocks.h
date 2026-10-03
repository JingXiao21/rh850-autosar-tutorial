/*
 * rte_mocks.h
 *
 * [Educational Implementation]
 * Test double of everything the GENERATED RTE (Rte.c / Rte_Tasks.c) calls outside itself: OS services, Com, IoHwAb, the
 * BSW main functions and Trace. Every call is recorded (name + two arguments) so a test can assert the exact SEQUENCE of
 * calls, which is what RTE behaviour is: "after the runnable terminated, Com_SendSignal is called", "GetResource brackets
 * the server body", "the Com callback does SetEvent(Task_LightCtl, ...)".
 * Used only by tests/unit/test_rte_*.c (host).
 */
#ifndef RTE_MOCKS_H
#define RTE_MOCKS_H

#include "Std_Types.h"

typedef struct {
    const char *fn;
    uint32      a;
    uint32      b;
} MockCall;

void            mock_reset(void);
uint32          mock_ncalls(void);
const MockCall *mock_call(uint32 i);
uint32          mock_count(const char *fn);
int             mock_index_of(const char *fn, uint32 from);          /* -1 if not found */
int             mock_index_of_args(const char *fn, uint32 a, uint32 from);

/* Com: value store (size in bytes, little endian like the real packing) */
void            mock_com_set(uint16 sig, uint8 size, uint32 value);
uint32          mock_com_sent(uint16 sig);                           /* last value passed to Com_SendSignal */
void            mock_com_set_stopped(boolean stopped);               /* Com returns COM_SERVICE_NOT_AVAILABLE */

/* OS: the queue of event masks that WaitEvent hands out; when it is empty WaitEvent leaves the task via longjmp */
void            mock_set_events(const uint32 *evs, uint32 n);
int             mock_run_task(void (*task)(void));                   /* returns 1 if the task body was left through WaitEvent */
int             mock_irq_depth(void);                                /* nesting of Suspend/Resume OS interrupts (must end at 0) */
int             mock_res_depth(void);                                /* GetResource - ReleaseResource */

/* IoHwAb / trace */
extern uint16   mock_wheel_raw;
extern void   (*mock_iohwab_hook)(void);                             /* called inside IoHwAb_GetWheelSpeed */
int             mock_trace_has(const char *text);                    /* was a trace line containing text emitted? */

#endif
