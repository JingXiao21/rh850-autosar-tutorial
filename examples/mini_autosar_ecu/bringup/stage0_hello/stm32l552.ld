/* [Educational Implementation] Minimal GNU ld script for STM32L552 (RAMN / Renode).
 * Memory sizes match Renode platforms/cpus/stm32l552.repl:
 *   flash 0x08000000 size 0x80000 (512 KB), sram1 0x20000000 size 0x30000 (192 KB),
 *   sram2 0x20030000 size 0x10000 (64 KB). Verify against the STM32L552 reference manual
 *   before using on real hardware. */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
  RAM2  (rw)  : ORIGIN = 0x20030000, LENGTH = 64K
}

_stack_size = 0x2000;   /* 8 KB main stack (MSP) */

SECTIONS
{
  .isr_vector : ALIGN(4)          /* vector table must be at the start of flash */
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text : ALIGN(4)
  {
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  _sidata = LOADADDR(.data);      /* LMA of .data initial values (in FLASH) */

  .data : ALIGN(4)                /* VMA in RAM, LMA in FLASH */
  {
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) : ALIGN(4)
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  .noinit (NOLOAD) : ALIGN(4)     /* survives warm reset: startup does not touch it */
  {
    *(.noinit*)
  } > RAM2

  .stack (NOLOAD) : ALIGN(8)
  {
    . = . + _stack_size;
    _estack = .;                  /* initial MSP = top of stack */
  } > RAM
}
