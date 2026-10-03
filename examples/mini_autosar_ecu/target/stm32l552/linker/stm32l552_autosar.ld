/* [Educational Implementation] stm32l552_autosar.ld
 * Extension of bringup/stage0_hello/stm32l552.ld with the sections used by the MemMap macros of
 * include/MemMap.h (real AUTOSAR counterpart: the linker-side of MemMap.h / section placement in the
 * ECU integration; see AUTOSAR_CP_SWS_MemoryMapping).
 *
 * Memory (Renode platforms/cpus/stm32l552.repl): FLASH 0x08000000 512K, SRAM1 0x20000000 192K,
 * SRAM2 0x20030000 64K. Verify against RM0438 for real hardware.
 *
 * Output sections (in placement order) and who feeds them:
 *   .isr_vector   FLASH  target/stm32l552/startup/startup.c    (vector table, KEEP)
 *   .text         FLASH  all code; MINI_CODE_FAST (.text.fast) grouped first (ISR + dispatcher), then .rodata
 *                        and MINI_CONST_CFG (.rodata.cfg): generated configuration tables
 *   .data         RAM    initialised variables, LMA in FLASH (copied by Reset_Handler)
 *   .os_stack     RAM    MINI_VAR_OS_STACK: all OS task stacks, NOLOAD, NOT zeroed (kernel paints them with
 *                        0xDEADBEEF for Os_GetTaskStackUsage); symbols __os_stack_start/__os_stack_end
 *   .bss          RAM    zero-initialised: .bss.rte (RTE buffers, MINI_VAR_RTE_BUF) and .bss.com (Com I-PDU
 *                        buffers, MINI_VAR_COM_BUF) grouped first with start/end symbols for the map file,
 *                        then everything else; cleared by Reset_Handler between _sbss and _ebss
 *   .noinit       RAM2   MINI_VAR_NOINIT (.bss.noinit): not touched by startup, survives warm reset (e.g.
 *                        reset reason / Det last error); must be listed BEFORE .bss so its pattern wins
 *   .stack        RAM    main stack (MSP): used by the OS idle loop and by all ISR/PendSV handlers
 *                        (tasks run on PSP with their own .os_stack areas)
 * The startup file must provide: _sidata,_sdata,_edata,_sbss,_ebss,_estack (as in stage0).
 */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
  RAM2  (rw)  : ORIGIN = 0x20030000, LENGTH = 64K
}

_stack_size = 0x1000;   /* 4 KB main stack (MSP): idle loop + nested ISR frames */

SECTIONS
{
  .isr_vector : ALIGN(4)
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text : ALIGN(4)
  {
    *(.text.fast*)
    *(.text*)
    *(.rodata.cfg*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  _sidata = LOADADDR(.data);

  .data : ALIGN(4)
  {
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .os_stack (NOLOAD) : ALIGN(8)
  {
    __os_stack_start = .;
    *(.os_stack*)
    . = ALIGN(8);
    __os_stack_end = .;
  } > RAM

  .noinit (NOLOAD) : ALIGN(4)
  {
    __noinit_start = .;
    *(.bss.noinit*)
    *(.noinit*)
    . = ALIGN(4);
    __noinit_end = .;
  } > RAM2

  .bss (NOLOAD) : ALIGN(4)
  {
    _sbss = .;
    __rte_buf_start = .;
    *(.bss.rte*)
    __rte_buf_end = .;
    __com_buf_start = .;
    *(.bss.com*)
    __com_buf_end = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  .stack (NOLOAD) : ALIGN(8)
  {
    __stack_start = .;
    . = . + _stack_size;
    _estack = .;
  } > RAM

  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }

  ASSERT(_estack <= ORIGIN(RAM) + LENGTH(RAM), "RAM overflow: .data/.bss/.os_stack/.stack do not fit SRAM1")
}
