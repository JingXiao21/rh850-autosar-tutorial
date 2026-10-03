# GNU binutils cheat sheet for `analyze_image.py`

**[Educational Implementation]** Every number in `artifacts/mini-autosar/analysis/<ecu>.md` can be reproduced by hand with
the commands below (all from `tools/toolchains/xpack-arm-none-eabi-gcc-*/bin/`, prefix `arm-none-eabi-`).
`$E` = the ELF (`artifacts/mini-autosar/target/<Ecu>/<Ecu>.elf`), `$M` = its `.map`.

| Command | What it shows | Used for |
|---|---|---|
| `nm -S -n $E` | every symbol: address, **size** (`-S`), type letter, name, sorted by address (`-n`). Types: `T/t` code, `R/r` rodata, `D/d` initialised data, `B/b` bss, `A` absolute (linker symbols such as `_stack_size`), `W` weak; upper case = global, lower = file-local (`static`) | stack arrays `Os_Stack_*`, linker symbols (`_estack`, `__os_stack_start`), address -> handler name |
| `nm -S --size-sort -C $E` | same, sorted by size ascending (`-C` demangle, no-op for C); read from the bottom for the biggest symbols | top-N list |
| `size $E` | Berkeley format: `text` (code + rodata + `.data` load image), `data`, `bss` totals | quick headline numbers |
| `size -A $E` | System-V format: one line per section with size and address, including debug sections | output-section sizes |
| `readelf -S -W $E` | section headers: type (`PROGBITS` has bytes in the file, `NOBITS` = `.bss`-like), flags (`A` alloc, `W` write, `X` exec), address, offset, alignment | find orphan/odd sections |
| `readelf -l -W $E` | program headers (segments) and the section-to-segment mapping. **`VirtAddr` is where the data runs, `PhysAddr` is where it is stored**: for `.data` they differ (RAM vs FLASH) - that is exactly what `Reset_Handler` copies | LMA != VMA check |
| `readelf -h $E` | ELF header, `Entry point` (must be Reset_Handler, with Thumb bit) | entry check |
| `objdump -h $E` | section table with **VMA and LMA columns** and flags (`ALLOC`, `LOAD`, `CONTENTS`, `READONLY`); `ALLOC` without `LOAD` = NOLOAD section (no flash image, LMA column meaningless) | VMA/LMA/size table |
| `objdump -s -j .isr_vector $E` | hex dump of one section (`-j`/`--section`), here the 125 vector words | decode vector table words |
| `objdump -d --disassemble=Reset_Handler $E` | disassemble one function (`-d` code, `--disassemble=SYM` limits it); `-d -j .text.fast` limits by section instead | look at the startup code |
| `-Wl,-Map=X.map` (linker option) | GNU ld map: `Memory Configuration`, `Linker script and memory map` (output section -> input sections -> object files, `load address` for LMA), discarded sections (`--gc-sections`) | per-module contribution |
| `-Wl,--cref` (linker option) | appends the **Cross Reference Table**: per symbol the defining object first, then every object that references it | "who uses symbol X" |
| `-Wl,--print-memory-usage` (linker option) | the FLASH/RAM/RAM2 "Used Size / Region Size / %age" table in the link log | cross-check of section 1 |

Handy by hand:

```
arm-none-eabi-nm -S --size-sort $E | tail -20            # 20 biggest symbols
arm-none-eabi-objdump -h $E | grep -A1 -E "\.data|\.bss" # VMA/LMA of data
arm-none-eabi-readelf -l -W $E                           # segments, PhysAddr vs VirtAddr
arm-none-eabi-objdump -d -j .text.fast $E                # only the MINI_CODE_FAST functions
```
