/*
 * HostFiber.h  (HOST BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: none (context primitive of the host OS port).  Declares the four Windows-fiber
 * wrappers of Os_PortFiber_Host.c.  Deliberately free of any #include: <windows.h> (used by the .c file)
 * must not meet AUTOSAR headers (SetEvent, CONST ... clash), the interface therefore uses plain C types only.
 */
#ifndef HOSTFIBER_H
#define HOSTFIBER_H

void *HostFiber_ConvertThread(void);                                            /* main thread -> fiber */
void *HostFiber_Create(unsigned stackBytes, void (*entry)(unsigned arg), unsigned arg);
void  HostFiber_Switch(void *fiber);                                            /* returns when switched back */
void  HostFiber_Delete(void *fiber);

#endif /* HOSTFIBER_H */
