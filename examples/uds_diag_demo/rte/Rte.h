/*
 * Rte.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Rte_Main.h (Rte_Start / Rte_Stop) plus the OS task
 * bodies that the RTE generator produces for runnables mapped to tasks.
 */
#ifndef RTE_H
#define RTE_H

#include "Std_Types.h"

void Rte_Start(void);
void Rte_Task_10ms(void);

#endif /* RTE_H */
