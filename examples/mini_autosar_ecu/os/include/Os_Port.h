/*
 * Os_Port.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the processor-specific part of an OS ("Os_Hal" / "port layer", not
 * standardised). Interface between the portable kernel (os/src/Os_Kernel.c, owner agent A) and the
 * two ports: os/port/cm33 (Cortex-M33, PendSV) and os/port/host (Windows fibers + virtual time).
 * Agent A may extend this header (it is private to the OS); other agents must not include it.
 */
#ifndef OS_PORT_H
#define OS_PORT_H

#include "Os.h"

/* ---- called by the kernel ---- */
void   Os_Port_Init(void);                                   /* StartOS: NVIC priorities, SysTick; host: fiber env */
void   Os_Port_StartTick(void);                              /* start the 1 ms tick source */
void   Os_Port_InitTaskContext(TaskType t);                  /* (re)build initial context so that the task starts at its entry */
void   Os_Port_RequestDispatch(void);                        /* ask for a context switch at the earliest legal point
                                                                (target: pend PendSV; host: switch now if in task context,
                                                                 or at ISR exit) */
void   Os_Port_StartFirstTask(void);                         /* leave StartOS: enter dispatcher/idle; never returns */
void   Os_Port_Idle(void);                                   /* idle loop body (target: WFI; host: advance virtual time) */
void   Os_Port_Halt(void);                                   /* after ShutdownOS */

/* ---- interrupt locking (nesting counters are in the kernel) ---- */
uint32 Os_Port_DisableAll(void);                             /* returns previous state */
void   Os_Port_RestoreAll(uint32 prev);
void   Os_Port_SetOsMask(boolean masked);                    /* target: BASEPRI = OS_CM33_MAX_SYSCALL_PRIO or 0; host: flag */
boolean Os_Port_InIsr(void);                                 /* handler mode / host ISR emulation flag */

/* ---- called BY the ports into the kernel ---- */
void   Os_Kernel_TickHandler(void);                          /* system counter + alarms; may request dispatch */
void   Os_Kernel_IsrEnter(ISRType id);                       /* Cat2 wrapper begin */
void   Os_Kernel_IsrExit(void);                              /* Cat2 wrapper end: reschedule if needed */
TaskType Os_Kernel_SelectNext(TaskType *prevOut);            /* dispatcher: running task -> next; called with interrupts locked */

/* ---- additions by agent A (kernel <-> port, still private to the OS) ---- */
void   Os_Port_InterruptPoint(void);                         /* kernel service-return / unmask point. target: no-op (the NVIC delivers
                                                                interrupts by itself); host: deliver pending simulated tick/ISRs now */
boolean Os_Port_StackCheck(TaskType t);                      /* FALSE if the stack canary of task t was overwritten (host: TRUE) */
uint32 Os_Port_StackUsage(TaskType t);                       /* high-water mark in bytes from the 0xDEADBEEF fill (host: 0) */
void   Os_Kernel_TaskReturn(void);                           /* port: a task entry function returned -> E_OS_MISSINGEND path; never returns */
StatusType Os_Kernel_GetShutdownStatus(void);                /* status passed to ShutdownOS (host Os_Port_Halt uses it as exit code) */

/* Cortex-M33 constants (documented in DESIGN 6.4) */
#define OS_CM33_PRIO_BITS            4u                      /* STM32L5: 16 levels in bits [7:4] */
#define OS_CM33_MAX_SYSCALL_PRIO     0x40u                   /* BASEPRI value; Cat2 ISRs and SysTick >= this */
#define OS_CM33_PENDSV_PRIO          0xF0u                   /* lowest urgency: switches run when all ISRs are done */

#endif /* OS_PORT_H */
