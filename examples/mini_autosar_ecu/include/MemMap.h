/*
 * MemMap.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: MemMap.h (AUTOSAR_CP_SWS_MemoryMapping). The real mechanism is
 *     #define <MIP>_START_SEC_<TYPE>   #include "<MIP>_MemMap.h"  ...  #define <MIP>_STOP_SEC_<TYPE>
 * which expands to compiler pragmas (#pragma section ...). GCC has no push/pop-able section pragmas,
 * so this project maps the same *section classes* onto attribute macros placed in front of a
 * definition:
 *     MINI_VAR_NOINIT uint32 Mod_Counter;      // like MOD_START_SEC_VAR_NO_INIT_32
 * On the host build every macro expands to nothing (default host linker script).
 * Section names are collected by target/stm32l552/linker/stm32l552_autosar.ld.
 */
#ifndef MEMMAP_H
#define MEMMAP_H

#include "Compiler.h"
#include "Mini_Cfg.h"

#if defined(MINI_PLATFORM_TARGET)
  #define MINI_VAR_NOINIT     __attribute__((section(".bss.noinit")))             /* survives warm reset (RAM2) */
  #define MINI_VAR_OS_STACK   __attribute__((section(".os_stack"), aligned(8)))   /* OS task stacks             */
  #define MINI_VAR_RTE_BUF    __attribute__((section(".bss.rte")))                /* RTE buffers (zeroed)       */
  #define MINI_VAR_COM_BUF    __attribute__((section(".bss.com")))                /* Com I-PDU buffers (zeroed) */
  #define MINI_CODE_FAST      __attribute__((section(".text.fast")))              /* ISR / dispatcher code      */
  #define MINI_CONST_CFG      __attribute__((section(".rodata.cfg")))             /* generated config tables    */
#else
  #define MINI_VAR_NOINIT
  #define MINI_VAR_OS_STACK
  #define MINI_VAR_RTE_BUF
  #define MINI_VAR_COM_BUF
  #define MINI_CODE_FAST
  #define MINI_CONST_CFG
#endif

#endif /* MEMMAP_H */
