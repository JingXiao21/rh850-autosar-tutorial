# Image analysis: LightEcu

ELF `LightEcu.elf`, map `LightEcu.map`, linker script `stm32l552_autosar.ld`. Toolchain: arm-none-eabi binutils from `tools/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin`.

```text
text	   data	    bss	    dec	    hex	filename
  16764	     12	  10216	  26992	   6970	D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
```


## 1. Memory regions (linker script MEMORY{} vs usage)

| Region | Attr | Origin | Length | Used B | Free B | Used | 0..100% |
|---|---|---|---:|---:|---:|---:|---|
| FLASH | rx | 0x08000000 | 524288 (512 KiB) | 16776 | 507512 | 3.20% | [#...................] |
| RAM | rwx | 0x20000000 | 196608 (192 KiB) | 10232 | 186376 | 5.20% | [#...................] |
| RAM2 | rw | 0x20030000 | 65536 (64 KiB) | 0 | 65536 | 0.00% | [....................] |

Used = highest end address of any section placed in the region minus its origin (the number `--print-memory-usage` prints). FLASH also holds the load image of `.data` (LMA); RAM2 is only used by `.noinit` (MINI_VAR_NOINIT).


## 2. Output sections (VMA / LMA / size / region)

| Section | VMA | LMA | Size B | VMA in | LMA in | Align | Notes |
|---|---|---|---:|---|---|---:|---|
| .isr_vector | 0x08000000 | 0x08000000 | 500 | FLASH | FLASH | 512 |  |
| .text | 0x080001f4 | 0x080001f4 | 16264 | FLASH | FLASH | 4 |  |
| **.data** | 0x20000000 | 0x0800417c | 12 | RAM | FLASH | 4 | LMA != VMA: load image in FLASH, copied to RAM by Reset_Handler |
| .os_stack | 0x20000010 | - | 4864 | RAM | - | 8 | NOLOAD/NOBITS: no flash image |
| .noinit | 0x20030000 | - | 0 | RAM2 | - | 4 | NOLOAD/NOBITS: no flash image |
| .bss | 0x20001310 | - | 1256 | RAM | - | 8 | NOLOAD/NOBITS: no flash image |
| .stack | 0x200017f8 | - | 4096 | RAM | - | 8 | NOLOAD/NOBITS: no flash image |


## 3. Program headers (readelf -l): what is loaded where

| Type | Offset | VirtAddr | PhysAddr | FileSiz | MemSiz | Flg | Note |
|---|---|---|---|---:|---:|---|---|
| LOAD | 0x00001000 | 0x08000000 | 0x08000000 | 16764 | 16764 | R E |  |
| LOAD | 0x00006000 | 0x20000000 | 0x0800417c | 12 | 12 | RW | PhysAddr != VirtAddr |
| LOAD | 0x00000010 | 0x20000010 | 0x08004188 | 0 | 4864 | RW | PhysAddr != VirtAddr |
| LOAD | 0x00000310 | 0x20001310 | 0x20001310 | 0 | 5352 | RW |  |


## 4. Per-module contribution (from the map, per input object)

| Module | Code | RO data | .data | .bss | Stacks | Flash B | RAM B |
|---|---:|---:|---:|---:|---:|---:|---:|
| Os_Cfg (generated) | 0 | 232 | 0 | 0 | 4608 | 232 | 4608 |
| OS | 3368 | 0 | 1 | 806 | 0 | 3369 | 807 |
| Main stack (.stack, linker) | 0 | 0 | 0 | 0 | 4096 | 0 | 4096 |
| BswM | 332 | 1526 | 0 | 7 | 0 | 1858 | 7 |
| Com | 1686 | 0 | 0 | 18 | 0 | 1686 | 18 |
| Can | 1604 | 0 | 0 | 37 | 0 | 1604 | 37 |
| OS port (cm33) | 770 | 0 | 0 | 157 | 256 | 770 | 413 |
| libc/libgcc | 892 | 0 | 0 | 0 | 0 | 892 | 0 |
| RTE (generated) | 742 | 0 | 0 | 33 | 0 | 742 | 33 |
| Trace | 636 | 40 | 0 | 0 | 0 | 676 | 0 |
| Startup/vectors | 82 | 500 | 0 | 0 | 0 | 582 | 0 |
| CanIf | 540 | 0 | 0 | 5 | 0 | 540 | 5 |
| Mcu | 448 | 0 | 5 | 9 | 0 | 453 | 14 |
| Port | 396 | 0 | 0 | 4 | 0 | 396 | 4 |
| Det | 226 | 0 | 0 | 98 | 0 | 226 | 98 |
| BswM_Cfg (generated) | 122 | 200 | 0 | 0 | 0 | 322 | 0 |
| EcuM | 306 | 0 | 1 | 1 | 0 | 307 | 2 |
| Adc | 284 | 0 | 0 | 24 | 0 | 284 | 24 |
| PduR | 292 | 0 | 0 | 5 | 0 | 292 | 5 |
| Trace/Time port | 258 | 0 | 0 | 6 | 0 | 258 | 6 |
| SWC LightControlSWC | 220 | 0 | 0 | 0 | 0 | 220 | 0 |
| Com_Cfg (generated) | 0 | 200 | 0 | 10 | 0 | 200 | 10 |
| IoHwAb | 116 | 6 | 1 | 7 | 0 | 123 | 8 |
| SWC OdometerSWC | 112 | 0 | 0 | 0 | 0 | 112 | 0 |
| SWC LightActuatorSWC | 92 | 0 | 0 | 0 | 0 | 92 | 0 |
| EcuM_Cfg (generated) | 80 | 1 | 0 | 0 | 0 | 81 | 0 |
| Can_Cfg (generated) | 0 | 76 | 0 | 0 | 0 | 76 | 0 |
| Integration (main/hooks) | 70 | 0 | 0 | 0 | 0 | 70 | 0 |
| Dio | 70 | 0 | 0 | 0 | 0 | 70 | 0 |
| CanIf_Cfg (generated) | 0 | 64 | 0 | 0 | 0 | 64 | 0 |
| Port_Cfg (generated) | 0 | 64 | 0 | 0 | 0 | 64 | 0 |
| (alignment fill) | 23 | 0 | 4 | 29 | 0 | 27 | 33 |
| SchM | 44 | 0 | 0 | 0 | 0 | 44 | 0 |
| PduR_Cfg (generated) | 0 | 26 | 0 | 0 | 0 | 26 | 0 |
| Adc_Cfg (generated) | 0 | 10 | 0 | 0 | 0 | 10 | 0 |
| Mcu_Cfg (generated) | 0 | 8 | 0 | 0 | 0 | 8 | 0 |
| **TOTAL** | 13811 | 2953 | 12 | 1256 | 8960 | 16776 | 10228 |

Flash = code + RO data + `.data` load image; RAM = `.data` + `.bss` + OS stacks + main stack. Object files are mapped to modules by name (os/src -> OS, gen/*Rte* -> RTE, swc/* -> SWC, bsw/com -> Com ...); generated configuration tables are listed separately as `<Module>_Cfg (generated)`. `(alignment fill)` is padding between input sections.


## 5. Top 15 largest symbols (nm -S --size-sort)

| Symbol | Size B | Type | Address | Region |
|---|---:|---|---|---|
| __stack_start | 4096 | B | 0x200017f8 | RAM |
| Os_Stack_Task_LightCtl | 1536 | b | 0x20000410 | RAM |
| Os_Stack_Task_LightAct | 1024 | b | 0x20000010 | RAM |
| Os_Stack_Task_Init | 1024 | b | 0x20000e10 | RAM |
| Os_Stack_Task_BswMain | 1024 | b | 0x20000a10 | RAM |
| __udivmoddi4 | 822 | T | 0x08003494 | FLASH |
| s_q | 512 | b | 0x2000153c | RAM |
| g_vectors | 500 | R | 0x08000000 | FLASH |
| Trace_Log | 496 | T | 0x08003078 | FLASH |
| StartOS | 440 | T | 0x08002818 | FLASH |
| Port_Init | 360 | T | 0x08001e3c | FLASH |
| CanHw_Init | 308 | T | 0x080018e0 | FLASH |
| Os_Kernel_SelectNext | 304 | T | 0x080026e8 | FLASH |
| Com_ReceiveSignal | 292 | T | 0x080007bc | FLASH |
| s_idleStack | 256 | b | 0x20001210 | RAM |

Type: T/t code, R/r read-only data, D/d initialised data, B/b zero-initialised data (upper case = global, lower = static).


## 6. MemMap-style sections: where did they end up?

| Input section | MemMap macro | Purpose | Output / region | Size B | Range | Contributors |
|---|---|---|---|---:|---|---|
| .text.fast | MINI_CODE_FAST | ISR + dispatcher code | .text / FLASH | 236 | 0x080001f4..0x080002e0 | OS port (cm33), RTE (generated), Trace/Time port |
| .rodata.cfg | MINI_CONST_CFG | generated config tables | .text / FLASH | 744 | 0x080037e0..0x08003aca | Adc_Cfg (generated), BswM_Cfg (generated), CanIf_Cfg (generated), Can_Cfg (generated), Com |
| .bss.rte | MINI_VAR_RTE_BUF | RTE buffers | .bss / RAM | 33 | 0x20001310..0x20001331 | RTE (generated) |
| .bss.com | MINI_VAR_COM_BUF | Com I-PDU buffers | .bss / RAM | 10 | 0x20001344..0x2000134e | Com_Cfg (generated) |
| .os_stack | MINI_VAR_OS_STACK | OS task stacks | .os_stack / RAM | 4864 | 0x20000010..0x20001310 | OS port (cm33), Os_Cfg (generated) |
| .bss.noinit | MINI_VAR_NOINIT | survives warm reset | - | 0 | - | - |
| .isr_vector | (startup.c attribute) | vector table | .isr_vector / FLASH | 500 | 0x08000000..0x080001f4 | Startup/vectors |

| Linker symbol | Value | Region |
|---|---|---|
| __rte_buf_start | 0x20001310 | RAM |
| __rte_buf_end | 0x20001331 | RAM |
| __com_buf_start | 0x20001331 | RAM |
| __com_buf_end | 0x2000134e | RAM |
| __os_stack_start | 0x20000010 | RAM |
| __os_stack_end | 0x20001310 | RAM |
| __noinit_start | 0x20030000 | RAM2 |
| __noinit_end | 0x20030000 | RAM2 |
| _sdata | 0x20000000 | RAM |
| _edata | 0x2000000c | RAM |
| _sidata | 0x0800417c | FLASH |
| _sbss | 0x20001310 | RAM |
| _ebss | 0x200017f8 | RAM |
| __stack_start | 0x200017f8 | RAM |
| _estack | 0x200027f8 | RAM |


## 7. Stacks

| Task stack (Os_Cfg.c / kernel) | Words | Bytes | Address |
|---|---:|---:|---|
| Task_LightAct | 256 | 1024 | 0x20000010 |
| Task_LightCtl | 384 | 1536 | 0x20000410 |
| Task_BswMain | 256 | 1024 | 0x20000a10 |
| Task_Init | 256 | 1024 | 0x20000e10 |
| s_idleStack | 64 | 256 | 0x20001210 |
| s_bootStack | 32 | 128 | 0x20001428 |

.os_stack spans 4864 B (sum of the stack arrays inside it 4864 B; s_bootStack lives in .bss, the boot stack of StartOS).

Main stack (MSP, `.stack`): 4096 B from 0x200017f8 to 0x200027f8 (idle phase before the first task, all ISRs, PendSV). Tasks run on PSP inside `.os_stack`; stack watermarks are measured at runtime by Os_GetTaskStackUsage() (0xDEADBEEF paint).


## 8. Vector table, Reset_Handler, _estack

`g_vectors` at 0x08000000, 500 B = 125 entries, alignment check for VTOR: OK (512-byte aligned).

| Entry | Word | Expected | Check |
|---|---|---|---|
| [0] initial MSP | 0x200027f8 | _estack = 0x200027f8 | OK |
| [1] Reset vector | 0x08003415 | Reset_Handler = 0x08003414 (+1 Thumb bit) | OK |

| Handler | Vector entries |
|---|---:|
| Os_Cm33_IrqEntry | 109 |
| Default_Handler | 8 |
| (reserved, 0) | 4 |
| Reset_Handler | 1 |
| PendSV_Handler | 1 |
| SysTick_Handler | 1 |

Start of `Reset_Handler` (objdump -d --disassemble=Reset_Handler):

```text
08003414 <Reset_Handler>:
 8003414:	b508      	push	{r3, lr}
 8003416:	f04f 23e0 	mov.w	r3, #3758153728	@ 0xe000e000
 800341a:	4a0c      	ldr	r2, [pc, #48]	@ (800344c <Reset_Handler+0x38>)
 800341c:	490c      	ldr	r1, [pc, #48]	@ (8003450 <Reset_Handler+0x3c>)
 800341e:	f8c3 2d08 	str.w	r2, [r3, #3336]	@ 0xd08
 8003422:	4b0c      	ldr	r3, [pc, #48]	@ (8003454 <Reset_Handler+0x40>)
 8003424:	4a0c      	ldr	r2, [pc, #48]	@ (8003458 <Reset_Handler+0x44>)
 8003426:	428b      	cmp	r3, r1
 8003428:	d307      	bcc.n	800343a <Reset_Handler+0x26>
 800342a:	2100      	movs	r1, #0
 800342c:	4b0b      	ldr	r3, [pc, #44]	@ (800345c <Reset_Handler+0x48>)
 800342e:	4a0c      	ldr	r2, [pc, #48]	@ (8003460 <Reset_Handler+0x4c>)
 8003430:	4293      	cmp	r3, r2
 8003432:	d307      	bcc.n	8003444 <Reset_Handler+0x30>
 8003434:	f7fe f819 	bl	800146a <main>
 8003438:	e7fe      	b.n	8003438 <Reset_Handler+0x24>
 800343a:	f852 0b04 	ldr.w	r0, [r2], #4
 800343e:	f843 0b04 	str.w	r0, [r3], #4
 8003442:	e7f0      	b.n	8003426 <Reset_Handler+0x12>
 8003444:	f843 1b04 	str.w	r1, [r3], #4
 8003448:	e7f2      	b.n	8003430 <Reset_Handler+0x1c>
 800344a:	bf00      	nop
 800344c:	08000000 	.word	0x08000000
 8003450:	2000000c 	.word	0x2000000c
 8003454:	20000000 	.word	0x20000000
 8003458:	0800417c 	.word	0x0800417c
 800345c:	20001310 	.word	0x20001310
```


## 9. Cross reference (-Wl,--cref table of the map)

| Symbol | Defined in | Referenced by |
|---|---|---|
| Os_Config | gen__LightEcu__Os_Cfg.c.o | os__src__Os_Task.c.o, os__src__Os_Resource.c.o, os__src__Os_Event.c.o, os__src__Os_Core.c.o, os__src__Os_Alarm.c.o, os__port__cm33__Os_Port_Cm33.c.o |
| Com_SendSignal | bsw__com__Com.c.o | gen__LightEcu__Rte.c.o |
| Can_Write | mcal__can__Can.c.o | ecual__canif__CanIf.c.o |
| EcuM_Init | bsw__ecum__EcuM.c.o | integration__Main_Target.c.o |
| Rte_Start | gen__LightEcu__Rte.c.o | gen__LightEcu__BswM_Cfg.c.o |

The first object after the symbol in the cref table defines it; the following lines are the objects that reference it. The table has 272 symbols. Use `--xref NAME` (repeatable) to look up others.


## 10. Warnings and notes

No warnings: no orphan sections with content, all regions <= 80% full, .data has an LMA in FLASH, vector table sane.

Notes:
- .bss.noinit (MINI_VAR_NOINIT) is not used by any module of this image; .noinit stays empty


## Appendix: binutils output excerpts and commands run

`size -A` (section sizes as the linker placed them):

```text
D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf  :
section             size        addr
.isr_vector          500   134217728
.text              16264   134218228
.data                 12   536870912
.os_stack           4864   536870928
.noinit                0   537067520
.bss                1256   536875792
.stack              4096   536877048
.debug_info        77003           0
.debug_abbrev      20655           0
.debug_loclists    27167           0
.debug_aranges      3232           0
.debug_rnglists     4295           0
.debug_line        57013           0
.debug_str         17635           0
.comment              57           0
.ARM.attributes       52           0
.debug_frame        7496           0
.debug_line_str      403           0
Total             242000
```

Commands executed by analyze_image.py (see tools/BINUTILS_CHEATSHEET.md for what each shows):

```text
arm-none-eabi-objdump -h D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-readelf -l -W D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-nm -S -n D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-nm -S --size-sort -C D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-size D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-size -A D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-objdump -s -j .isr_vector D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-readelf -h D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
arm-none-eabi-objdump -d --disassemble=Reset_Handler D:\side_project\rh850\artifacts\mini-autosar\target\LightEcu\LightEcu.elf
```

