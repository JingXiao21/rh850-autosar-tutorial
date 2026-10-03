#!/usr/bin/env python3
"""[Educational Implementation] analyze_image.py - map / ELF / linker-script analysis of a target firmware image.

Real counterpart: the integrator's "memory report" step (Vector: vLinkGen + map file inspection, Lauterbach/TRACE32
symbol views, Green Hills/IAR map viewers).  Here it is done with plain GNU binutils + a map-file parser, so every
number can be re-checked by hand with the commands listed in tools/BINUTILS_CHEATSHEET.md.

    python tools/analyze_image.py --ecu LightEcu                       # uses artifacts/mini-autosar/target/<ecu>/<ecu>.{elf,map}
    python tools/analyze_image.py --elf X.elf --map X.map --ld X.ld --ecu Name --xref Os_Config --xref Can_Write --top 20

Inputs : target ELF (arm-none-eabi-gcc -g), GNU ld map (-Wl,-Map, with -Wl,--cref), the linker script.
Outputs: text report on stdout (+ <out>/<ecu>.txt) and Markdown report <out>/<ecu>.md
         (default <out> = artifacts/mini-autosar/analysis).  Exit code 0 even with warnings (they are reported), 1 if a
         tool run / parse failed.

Report contents
  1  memory regions: MEMORY{} of the linker script vs. usage (FLASH / RAM / RAM2)
  2  output sections: VMA, LMA, size, region of VMA/LMA, .data (LMA != VMA) highlighted
  3  program headers (segments) from readelf -l
  4  per-module (OS, RTE, SWC, Com, PduR, CanIf, Can, Mcu, ...) flash / RAM contribution, parsed from the .map
  5  top-N largest symbols (nm -S --size-sort)
  6  MemMap-style sections (.text.fast .rodata.cfg .bss.rte .bss.com .bss.noinit .os_stack) and where they ended up
  7  OS task stacks (Os_Stack_* symbols of the generated Os_Cfg.c) + main stack
  8  vector table, Reset_Handler, _estack, IRQ entry histogram
  9  cross reference of selected symbols (-Wl,--cref table of the map)
  10 warnings: orphan sections, regions > 80 % full, .data without LMA, vector table sanity, empty MemMap sections
"""
import argparse
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
TOOLCHAIN = REPO / "tools/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin"
OUT_DEFAULT = REPO / "artifacts/mini-autosar/analysis"
LD_DEFAULT = ROOT / "target/stm32l552/linker/stm32l552_autosar.ld"
FULL_WARN_PCT = 80.0

COMMANDS = []      # every binutils command line that was run (appendix of the report)


class ToolError(Exception):
    pass


def tool(name):
    exe = TOOLCHAIN / f"arm-none-eabi-{name}.exe"
    if not exe.exists():
        exe = TOOLCHAIN / f"arm-none-eabi-{name}"
    return str(exe)


def run_tool(name, *args):
    cmd = [tool(name), *[str(a) for a in args]]
    COMMANDS.append("arm-none-eabi-" + name + " " + " ".join(f'"{a}"' if " " in str(a) else str(a) for a in args))
    p = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    if p.returncode != 0:
        raise ToolError(f"{name} failed: {p.stderr.strip()[:200]}")
    return p.stdout


# ------------------------------------------------------------------------------------------------ report model
class Report:
    """Collects blocks once, renders them as aligned text or Markdown."""

    def __init__(self, title):
        self.title = title
        self.blocks = []

    def h(self, text):
        self.blocks.append(("h", text))

    def p(self, text):
        self.blocks.append(("p", text))

    def code(self, text):
        self.blocks.append(("code", text))

    def table(self, header, rows, align=None):
        self.blocks.append(("table", header, [[str(c) for c in r] for r in rows], align))

    def text(self):
        out = [self.title, "=" * len(self.title), ""]
        for b in self.blocks:
            if b[0] == "h":
                out += ["", b[1], "-" * len(b[1])]
            elif b[0] == "p":
                out += [b[1]]
            elif b[0] == "code":
                out += ["    " + l for l in b[1].splitlines()]
            else:
                _, header, rows, align = b
                w = [max(len(str(x)) for x in col) for col in zip(header, *rows)] if rows else [len(x) for x in header]
                al = align or ["l"] * len(header)
                fmt = lambda r: "  ".join((c.rjust(w[i]) if al[i] == "r" else c.ljust(w[i])) for i, c in enumerate(r)).rstrip()
                out += [fmt(header), "  ".join("-" * x for x in w)] + [fmt(r) for r in rows]
                out[-len(rows):] = [x.replace("**", "") for x in out[-len(rows):]] if rows else []
        return "\n".join(out) + "\n"

    def markdown(self):
        out = [f"# {self.title}", ""]
        for b in self.blocks:
            if b[0] == "h":
                out += ["", f"## {b[1]}", ""]
            elif b[0] == "p":
                out += [b[1], ""]
            elif b[0] == "code":
                out += ["```text", b[1].rstrip("\n"), "```", ""]
            else:
                _, header, rows, align = b
                al = align or ["l"] * len(header)
                out.append("| " + " | ".join(header) + " |")
                out.append("|" + "|".join(("---:" if a == "r" else "---") for a in al) + "|")
                out += ["| " + " | ".join(c.replace("|", "\\|") for c in r) + " |" for r in rows]
                out.append("")
        return "\n".join(out) + "\n"


# ------------------------------------------------------------------------------------------------ parsers
def parse_ld(path):
    """MEMORY regions and output-section names of the linker script."""
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    text_nc = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    regions = {}
    m = re.search(r"MEMORY\s*\{(.*?)\}", text_nc, flags=re.S)
    if m:
        for rm in re.finditer(r"(\w+)\s*(\(\w+\))?\s*:\s*ORIGIN\s*=\s*(\S+)\s*,\s*LENGTH\s*=\s*(\w+)", m.group(1)):
            regions[rm.group(1)] = (int(rm.group(3), 0), parse_size(rm.group(4)), (rm.group(2) or "").strip("()"))
    sections = set(re.findall(r"^\s*(\.[\w.]+)\s*(?:\(\w+\))?\s*(?:ALIGN\([^)]*\))?\s*:", text_nc, flags=re.M))
    return regions, sections


def parse_size(s):
    s = s.strip()
    mult = {"K": 1024, "M": 1024 * 1024}.get(s[-1:].upper(), 1)
    return int(s[:-1] if mult != 1 else s, 0) * mult


def parse_map_regions(map_text):
    regions = {}
    m = re.search(r"^Memory Configuration\s*\n\s*\nName\s+Origin\s+Length\s+Attributes\s*\n(.*?)\n\s*\n", map_text, flags=re.M | re.S)
    if m:
        for l in m.group(1).splitlines():
            t = l.split()
            if len(t) >= 3 and t[0] != "*default*":
                regions[t[0]] = (int(t[1], 16), int(t[2], 16), t[3] if len(t) > 3 else "")
    return regions


def base(path):
    """object file name from a map path; archive members become 'lib.a(member.o)'."""
    path = path.strip()
    m = re.match(r".*[\\/]([^\\/]+\.a)\(([^)]+)\)$", path)
    if m:
        return f"{m.group(1)}({m.group(2)})"
    return re.split(r"[\\/]", path)[-1]


def parse_map_sections(map_text):
    """Returns output sections [{name, vma, size, lma, inputs:[{name, addr, size, obj}]}] from 'Linker script and memory map'."""
    start = map_text.index("Linker script and memory map")
    end = map_text.find("\nCross Reference Table", start)
    lines = map_text[start:end if end >= 0 else None].splitlines()
    outs, cur, pending = [], None, None
    out_re = re.compile(r"^(\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)(?:\s+load address 0x([0-9a-f]+))?\s*$")
    in_re = re.compile(r"^ (\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s*(.*)$")
    cont_re = re.compile(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s*(.*)$")
    for l in lines:
        m = out_re.match(l)
        if m:
            cur = dict(name=m.group(1), vma=int(m.group(2), 16), size=int(m.group(3), 16),
                       lma=int(m.group(4), 16) if m.group(4) else None, inputs=[])
            outs.append(cur)
            pending = None
            continue
        if re.match(r"^\.\S+\s*$", l):                         # long output section name, numbers on the next line
            pending = ("out", l.strip())
            continue
        if pending and pending[0] == "out":
            m = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)(?:\s+load address 0x([0-9a-f]+))?\s*$", l)
            if m:
                cur = dict(name=pending[1], vma=int(m.group(1), 16), size=int(m.group(2), 16),
                           lma=int(m.group(3), 16) if m.group(3) else None, inputs=[])
                outs.append(cur)
            pending = None
            continue
        if cur is None:
            continue
        if pending and pending[0] == "in":
            m = cont_re.match(l)
            if m:
                cur["inputs"].append(dict(name=pending[1], addr=int(m.group(1), 16), size=int(m.group(2), 16), obj=base(m.group(3))))
                pending = None
                continue
            pending = None
        m = in_re.match(l)
        if m:
            name, addr, size, rest = m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)
            obj = "(fill)" if name == "*fill*" else base(rest) if rest.strip() else "(none)"
            cur["inputs"].append(dict(name=name, addr=addr, size=size, obj=obj))
            continue
        m = re.match(r"^ (\S+)\s*$", l)                        # long input section name on its own line
        if m and not l.strip().startswith("*(") and not l.strip().startswith("["):
            pending = ("in", m.group(1))
    return outs


def parse_cref(map_text):
    """symbol -> [files], the first file is the one that defines the symbol."""
    i = map_text.find("Cross Reference Table")
    if i < 0:
        return {}
    table, cur = {}, None
    for l in map_text[i:].splitlines()[3:]:
        if not l.strip():
            continue
        if l[0] not in " \t":
            parts = l.split(None, 1)
            cur = parts[0]
            table[cur] = [base(parts[1])] if len(parts) > 1 else []
        elif cur:
            table[cur].append(base(l))
    return table


def parse_objdump_h(text):
    secs = []
    lines = text.splitlines()
    for i, l in enumerate(lines):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+2\*\*(\d+)", l)
        if m:
            flags = lines[i + 1].strip() if i + 1 < len(lines) else ""
            secs.append(dict(name=m.group(1), size=int(m.group(2), 16), vma=int(m.group(3), 16), lma=int(m.group(4), 16),
                             off=int(m.group(5), 16), align=1 << int(m.group(6)), flags=flags,
                             alloc="ALLOC" in flags, load="LOAD" in flags))
    return secs


def parse_nm(text):
    syms = []
    for l in text.splitlines():
        t = l.split(None, 3)
        if len(t) == 4 and re.fullmatch(r"[0-9a-f]+", t[0]) and re.fullmatch(r"[0-9a-f]+", t[1]):
            syms.append(dict(addr=int(t[0], 16), size=int(t[1], 16), type=t[2], name=t[3].strip()))
        elif len(t) == 3 and re.fullmatch(r"[0-9a-f]+", t[0]):
            syms.append(dict(addr=int(t[0], 16), size=None, type=t[1], name=t[2].strip()))
    return syms


def parse_segments(text):
    segs = []
    for l in text.splitlines():
        m = re.match(r"\s*(LOAD|EXIDX|NOTE|TLS)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(.*?)\s+0x([0-9a-f]+)\s*$", l)
        if m:
            segs.append(dict(type=m.group(1), off=int(m.group(2), 16), vaddr=int(m.group(3), 16), paddr=int(m.group(4), 16),
                             filesz=int(m.group(5), 16), memsz=int(m.group(6), 16), flags=m.group(7)))
    return segs


# ------------------------------------------------------------------------------------------------ classification
BSW_NAMES = {"com": "Com", "pdur": "PduR", "ecum": "EcuM", "bswm": "BswM", "det": "Det", "schm": "SchM"}
MODULE_RULES = [
    (r"^os__src__Trace", "Trace"),
    (r"^os__src__", "OS"),
    (r"^os__port__cm33__(Trace|Mini_Time)", "Trace/Time port"),
    (r"^os__port__", "OS port (cm33)"),
    (r"^gen__\w+?__Os_Cfg", "Os_Cfg (generated)"),
    (r"^gen__\w+?__Rte", "RTE (generated)"),
    (r"^swc__(\w+?)__", lambda m: "SWC " + m.group(1)),
    (r"^bsw__(\w+?)__", lambda m: BSW_NAMES.get(m.group(1), m.group(1))),
    (r"^ecual__canif__", "CanIf"),
    (r"^ecual__iohwab__", "IoHwAb"),
    (r"^mcal__can__", "Can"),
    (r"^mcal__mcu__", "Mcu"),
    (r"^mcal__port__", "Port"),
    (r"^mcal__dio__", "Dio"),
    (r"^mcal__adc__", "Adc"),
    (r"^gen__\w+?__(\w+?)_Cfg", lambda m: m.group(1) + "_Cfg (generated)"),
    (r"^integration__", "Integration (main/hooks)"),
    (r"^target__stm32l552__startup", "Startup/vectors"),
    (r"^(target__restbus|sim__restbus)", "RestBus"),
    (r"^lib\w*\.a\(", "libc/libgcc"),
]


def module_of(obj):
    if obj in ("(fill)", "(none)"):
        return "(alignment fill)" if obj == "(fill)" else "(linker generated)"
    for rx, name in MODULE_RULES:
        m = re.match(rx, obj)
        if m:
            return name(m) if callable(name) else name
    return obj


def kind_of(secname):
    for prefix, k in ((".text", "code"), (".rodata", "rodata"), (".isr_vector", "rodata"), (".data", "data"),
                      (".bss", "bss"), ("COMMON", "bss"), (".os_stack", "stack"), (".noinit", "bss"), (".stack", "stack"),
                      (".ARM", "rodata")):
        if secname.startswith(prefix):
            return k
    return "other"


def region_of(addr, regions):
    for n, (o, l, _) in regions.items():
        if o <= addr < o + l:
            return n
    return "?"


def fmt_addr(a):
    return f"0x{a:08x}"


# ------------------------------------------------------------------------------------------------ analysis
def analyze(elf, mapf, ld, ecu, top, xrefs, outdir):
    warnings, infos = [], []
    map_text = pathlib.Path(mapf).read_text(encoding="utf-8", errors="replace")
    ld_regions, ld_sections = parse_ld(ld)
    map_regions = parse_map_regions(map_text)
    regions = ld_regions or map_regions
    if ld_regions and map_regions and {k: v[:2] for k, v in ld_regions.items()} != {k: v[:2] for k, v in map_regions.items()}:
        warnings.append("MEMORY{} of the linker script differs from the 'Memory Configuration' of the map (stale map?)")
    outs = parse_map_sections(map_text)
    objh = parse_objdump_h(run_tool("objdump", "-h", elf))
    segs = parse_segments(run_tool("readelf", "-l", "-W", elf))
    nm_all = parse_nm(run_tool("nm", "-S", "-n", elf))
    nm_sorted = parse_nm(run_tool("nm", "-S", "--size-sort", "-C", elf))
    size_berk = run_tool("size", elf)
    size_a = run_tool("size", "-A", elf)
    syms = {s["name"]: s for s in nm_all}
    by_addr = collections.defaultdict(list)
    for s in nm_all:
        if s["type"] in "TtWwRr" or s["type"].upper() in "TRWD":
            by_addr[s["addr"] & ~1].append(s["name"])

    rep = Report(f"Image analysis: {ecu}")
    rep.p(f"ELF `{pathlib.Path(elf).name}`, map `{pathlib.Path(mapf).name}`, linker script `{pathlib.Path(ld).name}`. "
          f"Toolchain: arm-none-eabi binutils from `{TOOLCHAIN.relative_to(REPO).as_posix()}`.")
    rep.code(size_berk.strip())

    # ---- sections (objdump -h is the authority for VMA/LMA/size; the map gives the input-section breakdown)
    alloc = [s for s in objh if s["alloc"] and s["size"] > 0 or (s["alloc"] and s["name"] in ld_sections)]
    for s in alloc:
        s["vreg"] = region_of(s["vma"], regions)
        s["has_lma"] = s["load"]
        s["lreg"] = region_of(s["lma"], regions) if s["load"] else "-"

    # ---- 1 regions
    rep.h("1. Memory regions (linker script MEMORY{} vs usage)")
    rows, region_used = [], {}
    for n, (o, l, attr) in regions.items():
        ends = [s["vma"] + s["size"] for s in alloc if s["vreg"] == n and s["size"] > 0]
        ends += [s["lma"] + s["size"] for s in alloc if s["lreg"] == n and s["lma"] != s["vma"] and s["size"] > 0]
        used = (max(ends) - o) if ends else 0           # GNU ld definition: high-water mark from the origin
        region_used[n] = used
        pct = 100.0 * used / l
        bar = "#" * int(round(pct / 5)) + "." * (20 - int(round(pct / 5)))
        rows.append([n, attr or "-", fmt_addr(o), f"{l} ({l // 1024} KiB)", used, l - used, f"{pct:.2f}%", f"[{bar}]"])
        if pct > FULL_WARN_PCT:
            warnings.append(f"region {n} is {pct:.1f}% full (> {FULL_WARN_PCT:.0f}%)")
    rep.table(["Region", "Attr", "Origin", "Length", "Used B", "Free B", "Used", "0..100%"], rows,
              ["l", "l", "l", "r", "r", "r", "r", "l"])
    rep.p("Used = highest end address of any section placed in the region minus its origin (the number `--print-memory-usage` "
          "prints). FLASH also holds the load image of `.data` (LMA); RAM2 is only used by `.noinit` (MINI_VAR_NOINIT).")

    # ---- 2 output sections
    rep.h("2. Output sections (VMA / LMA / size / region)")
    rows = []
    for s in alloc:
        note = []
        if s["load"] and s["lma"] != s["vma"]:
            note.append("LMA != VMA: load image in %s, copied to %s by Reset_Handler" % (s["lreg"], s["vreg"]))
        if not s["load"]:
            note.append("NOLOAD/NOBITS: no flash image")
        if s["name"] not in ld_sections:
            note.append("ORPHAN (not named in linker script)")
        flag = "**" if (s["load"] and s["lma"] != s["vma"]) else ""
        rows.append([f"{flag}{s['name']}{flag}", fmt_addr(s["vma"]), fmt_addr(s["lma"]) if s["load"] else "-", s["size"],
                     s["vreg"], s["lreg"], s["align"], "; ".join(note)])
    rep.table(["Section", "VMA", "LMA", "Size B", "VMA in", "LMA in", "Align", "Notes"], rows, ["l", "l", "l", "r", "l", "l", "r", "l"])
    for s in alloc:
        if s["name"] not in ld_sections:
            if s["size"] > 0:
                warnings.append(f"orphan output section {s['name']} ({s['size']} B at {fmt_addr(s['vma'])}) is placed by ld defaults, not by the linker script")
            else:
                infos.append(f"empty linker-generated section {s['name']} (0 B, harmless)")
    data = next((s for s in alloc if s["name"] == ".data"), None)
    if data and data["size"] > 0:
        if not data["load"] or data["lma"] == data["vma"] or data["lreg"] != "FLASH":
            warnings.append(".data has no separate load address in FLASH: initialised variables would be lost after reset")
        else:
            si, sd = syms.get("_sidata"), syms.get("_sdata")
            if si and si["addr"] != data["lma"]:
                warnings.append(f"_sidata ({fmt_addr(si['addr'])}) != .data LMA ({fmt_addr(data['lma'])})")
            if sd and sd["addr"] != data["vma"]:
                warnings.append(f"_sdata ({fmt_addr(sd['addr'])}) != .data VMA ({fmt_addr(data['vma'])})")
    # map vs objdump consistency
    for o in outs:
        s = next((x for x in alloc if x["name"] == o["name"]), None)
        if s and (s["size"] != o["size"] or s["vma"] != o["vma"]):
            warnings.append(f"map and ELF disagree about {o['name']}")

    # ---- 3 segments
    rep.h("3. Program headers (readelf -l): what is loaded where")
    rows = [[sg["type"], fmt_addr(sg["off"]), fmt_addr(sg["vaddr"]), fmt_addr(sg["paddr"]), sg["filesz"], sg["memsz"], sg["flags"],
             "PhysAddr != VirtAddr" if sg["paddr"] != sg["vaddr"] else ""] for sg in segs]
    rep.table(["Type", "Offset", "VirtAddr", "PhysAddr", "FileSiz", "MemSiz", "Flg", "Note"], rows, ["l", "l", "l", "l", "r", "r", "l", "l"])

    # ---- 4 per module
    rep.h("4. Per-module contribution (from the map, per input object)")
    acc = collections.defaultdict(lambda: collections.Counter())
    for o in outs:
        so = next((x for x in alloc if x["name"] == o["name"]), None)
        if not so:
            continue
        for i in o["inputs"]:
            if i["size"] == 0:
                continue
            mod = module_of(i["obj"])
            if i["name"] == "*fill*" and o["name"] == ".stack":
                mod = "Main stack (.stack, linker)"
            k = kind_of(o["name"] if i["name"] == "*fill*" else i["name"])
            if k == "other":
                k = kind_of(o["name"])
            acc[mod][k] += i["size"]
    rows, tot = [], collections.Counter()
    for mod, c in acc.items():
        flash, ram = c["code"] + c["rodata"] + c["data"], c["data"] + c["bss"] + c["stack"]
        rows.append((flash + ram, [mod, c["code"], c["rodata"], c["data"], c["bss"], c["stack"], flash, ram]))
        for k in ("code", "rodata", "data", "bss", "stack"):
            tot[k] += c[k]
    rows = [r for _, r in sorted(rows, key=lambda x: -x[0])]
    flash_t, ram_t = tot["code"] + tot["rodata"] + tot["data"], tot["data"] + tot["bss"] + tot["stack"]
    rows.append(["**TOTAL**", tot["code"], tot["rodata"], tot["data"], tot["bss"], tot["stack"], flash_t, ram_t])
    rep.table(["Module", "Code", "RO data", ".data", ".bss", "Stacks", "Flash B", "RAM B"], rows, ["l"] + ["r"] * 7)
    rep.p("Flash = code + RO data + `.data` load image; RAM = `.data` + `.bss` + OS stacks + main stack. Object files are mapped to "
          "modules by name (os/src -> OS, gen/*Rte* -> RTE, swc/* -> SWC, bsw/com -> Com ...); generated configuration "
          "tables are listed separately as `<Module>_Cfg (generated)`. `(alignment fill)` is padding between input sections.")
    if "FLASH" in region_used and abs(flash_t - region_used["FLASH"]) > 0:
        infos.append(f"module table FLASH sum {flash_t} B vs region high-water mark {region_used['FLASH']} B")

    # ---- 5 top symbols
    rep.h(f"5. Top {top} largest symbols (nm -S --size-sort)")
    rows = []
    for s in list(reversed([x for x in nm_sorted if x["size"]]))[:top]:
        rows.append([s["name"], s["size"], s["type"], fmt_addr(s["addr"]), region_of(s["addr"], regions)])
    rep.table(["Symbol", "Size B", "Type", "Address", "Region"], rows, ["l", "r", "l", "l", "l"])
    rep.p("Type: T/t code, R/r read-only data, D/d initialised data, B/b zero-initialised data (upper case = global, lower = static).")

    # ---- 6 MemMap sections
    rep.h("6. MemMap-style sections: where did they end up?")
    memmap = [(".text.fast", "MINI_CODE_FAST", "ISR + dispatcher code"), (".rodata.cfg", "MINI_CONST_CFG", "generated config tables"),
              (".bss.rte", "MINI_VAR_RTE_BUF", "RTE buffers"), (".bss.com", "MINI_VAR_COM_BUF", "Com I-PDU buffers"),
              (".os_stack", "MINI_VAR_OS_STACK", "OS task stacks"), (".bss.noinit", "MINI_VAR_NOINIT", "survives warm reset"),
              (".isr_vector", "(startup.c attribute)", "vector table")]
    rows = []
    for pref, macro, what in memmap:
        items = [(o, i) for o in outs for i in o["inputs"] if i["name"] == pref or i["name"].startswith(pref + ".")]
        if not items:
            rows.append([pref, macro, what, "-", 0, "-", "-"])
            if pref in (".bss.noinit",):
                infos.append(f"{pref} ({macro}) is not used by any module of this image; .noinit stays empty")
            continue
        lo = min(i["addr"] for _, i in items)
        hi = max(i["addr"] + i["size"] for _, i in items)
        mods = sorted({module_of(i["obj"]) for _, i in items})
        rows.append([pref, macro, what, items[0][0]["name"] + " / " + region_of(lo, regions), sum(i["size"] for _, i in items),
                     f"{fmt_addr(lo)}..{fmt_addr(hi)}", ", ".join(mods)[:90]])
    rep.table(["Input section", "MemMap macro", "Purpose", "Output / region", "Size B", "Range", "Contributors"], rows,
              ["l", "l", "l", "l", "r", "l", "l"])
    marks = [n for n in ("__rte_buf_start", "__rte_buf_end", "__com_buf_start", "__com_buf_end", "__os_stack_start", "__os_stack_end",
                         "__noinit_start", "__noinit_end", "_sdata", "_edata", "_sidata", "_sbss", "_ebss", "__stack_start", "_estack") if n in syms]
    rep.table(["Linker symbol", "Value", "Region"], [[n, fmt_addr(syms[n]["addr"]), region_of(syms[n]["addr"], regions)] for n in marks], ["l", "l", "l"])

    # ---- 7 stacks
    rep.h("7. Stacks")
    st = [s for s in nm_all if s["name"].startswith("Os_Stack_") and s["size"]]
    rows = [[s["name"][len("Os_Stack_"):], s["size"] // 4, s["size"], fmt_addr(s["addr"])] for s in sorted(st, key=lambda x: x["addr"])]
    extra = [s for s in nm_all if s["name"] in ("s_idleStack", "s_bootStack") and s["size"]]
    os_stack_syms = [s for s in st + extra if "__os_stack_start" in syms and syms["__os_stack_start"]["addr"] <= s["addr"] < syms["__os_stack_end"]["addr"]]
    rows += [[s["name"], s["size"] // 4, s["size"], fmt_addr(s["addr"])] for s in extra]
    if rows:
        rep.table(["Task stack (Os_Cfg.c / kernel)", "Words", "Bytes", "Address"], rows, ["l", "r", "r", "l"])
    else:
        rep.p("No Os_Stack_* symbols (image without OS).")
    if "__os_stack_start" in syms and "__os_stack_end" in syms:
        span = syms["__os_stack_end"]["addr"] - syms["__os_stack_start"]["addr"]
        total = sum(s["size"] for s in os_stack_syms)
        rep.p(f".os_stack spans {span} B (sum of the stack arrays inside it {total} B; s_bootStack lives in .bss, the boot stack of StartOS).")
        if total > span:
            warnings.append(".os_stack arrays are larger than the section span")
    if "_estack" in syms and "__stack_start" in syms:
        ms = syms["_estack"]["addr"] - syms["__stack_start"]["addr"]
        rep.p(f"Main stack (MSP, `.stack`): {ms} B from {fmt_addr(syms['__stack_start']['addr'])} to {fmt_addr(syms['_estack']['addr'])} "
              f"(idle phase before the first task, all ISRs, PendSV). Tasks run on PSP inside `.os_stack`; stack watermarks are "
              f"measured at runtime by Os_GetTaskStackUsage() (0xDEADBEEF paint).")

    # ---- 8 vector table
    rep.h("8. Vector table, Reset_Handler, _estack")
    vt = syms.get("g_vectors")
    if vt:
        raw = run_tool("objdump", "-s", "-j", ".isr_vector", elf)
        buf = bytearray()
        for l in raw.splitlines():
            m = re.match(r"\s([0-9a-f]{4,8})((?:\s[0-9a-f]{1,8}){1,4})\s", l + " ")
            if m:
                for w in m.group(2).split():
                    buf += bytes.fromhex(w.rjust(8, "0"))
        words = [int.from_bytes(buf[i:i + 4], "little") for i in range(0, len(buf) - 3, 4)]
        rep.p(f"`g_vectors` at {fmt_addr(vt['addr'])}, {vt['size']} B = {vt['size'] // 4} entries, "
              f"alignment check for VTOR: {'OK (512-byte aligned)' if vt['addr'] % 512 == 0 else 'NOT 512-byte aligned'}.")
        if vt["addr"] % 512:
            warnings.append("vector table is not aligned to 512 B (VTOR requirement for 125 entries)")
        rh, es = syms.get("Reset_Handler"), syms.get("_estack")
        rows = [["[0] initial MSP", fmt_addr(words[0]), "_estack = " + (fmt_addr(es["addr"]) if es else "?"),
                 "OK" if es and words[0] == es["addr"] else "MISMATCH"],
                ["[1] Reset vector", fmt_addr(words[1]), "Reset_Handler = " + (fmt_addr(rh["addr"]) + " (+1 Thumb bit)" if rh else "?"),
                 "OK" if rh and words[1] == (rh["addr"] | 1) else "MISMATCH"]]
        rep.table(["Entry", "Word", "Expected", "Check"], rows)
        for r in rows:
            if r[3] != "OK":
                warnings.append(f"vector {r[0]} does not match the symbol table")
        hist = collections.Counter()
        names = collections.defaultdict(list)
        for s in nm_all:
            if s["type"] in "TtWw":
                names[s["addr"]].append(s["name"])
        for w in words[1:]:
            al = names.get(w & ~1, [])
            hist["(reserved, 0)" if w == 0 else "Default_Handler" if "Default_Handler" in al else al[0] if al else fmt_addr(w)] += 1
        rep.table(["Handler", "Vector entries"], [[k, v] for k, v in hist.most_common()], ["l", "r"])
        for core in ("PendSV_Handler", "SysTick_Handler"):
            if core not in hist:
                warnings.append(f"{core} is not in the vector table")
        if "Default_Handler" in hist and hist["Default_Handler"] > 12:
            infos.append("many vector entries still point to Default_Handler")
        ep = re.search(r"Entry point (0x[0-9a-f]+)", run_tool("readelf", "-h", elf))
        if ep and rh and int(ep.group(1), 16) != (rh["addr"] | 1) and int(ep.group(1), 16) != rh["addr"]:
            warnings.append("ELF entry point is not Reset_Handler")
        dis = run_tool("objdump", "-d", "--disassemble=Reset_Handler", elf).splitlines()
        rep.p("Start of `Reset_Handler` (objdump -d --disassemble=Reset_Handler):")
        rep.code("\n".join(l for l in dis if re.match(r"^[0-9a-f]+ <|^\s+[0-9a-f]+:", l))[:1800].rsplit("\n", 1)[0])
    else:
        rep.p("No g_vectors symbol.")

    # ---- 9 cross reference
    rep.h("9. Cross reference (-Wl,--cref table of the map)")
    cref = parse_cref(map_text)
    wanted = xrefs or ["Os_Config", "Com_SendSignal", "Can_Write", "EcuM_Init", "Rte_Start"]
    rows = []
    for s in wanted:
        files = cref.get(s)
        if not files:
            rows.append([s, "(not in cref: static, discarded by --gc-sections or not linked)", ""])
            continue
        rows.append([s, files[0], ", ".join(files[1:]) if len(files) > 1 else "(no other object references it)"])
    rep.table(["Symbol", "Defined in", "Referenced by"], rows)
    rep.p("The first object after the symbol in the cref table defines it; the following lines are the objects that reference it. "
          f"The table has {len(cref)} symbols. Use `--xref NAME` (repeatable) to look up others.")

    # ---- 10 warnings
    rep.h("10. Warnings and notes")
    if warnings:
        rep.table(["#", "Warning"], [[i + 1, w] for i, w in enumerate(warnings)], ["r", "l"])
    else:
        rep.p("No warnings: no orphan sections with content, all regions <= %d%% full, .data has an LMA in FLASH, vector table sane." % FULL_WARN_PCT)
    if infos:
        rep.p("Notes:\n" + "\n".join(f"- {i}" for i in infos))

    # ---- appendix
    rep.h("Appendix: binutils output excerpts and commands run")
    rep.p("`size -A` (section sizes as the linker placed them):")
    rep.code(size_a.strip())
    seen, cmds = set(), []
    for c in COMMANDS:
        if c not in seen:
            seen.add(c)
            cmds.append(c)
    rep.p("Commands executed by analyze_image.py (see tools/BINUTILS_CHEATSHEET.md for what each shows):")
    rep.code("\n".join(cmds))
    return rep, warnings


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ecu", required=True, help="image name, e.g. SensorEcu / LightEcu (default paths derive from it)")
    ap.add_argument("--elf")
    ap.add_argument("--map")
    ap.add_argument("--ld", default=str(LD_DEFAULT))
    ap.add_argument("--out", default=str(OUT_DEFAULT), help="output directory for <ecu>.md / <ecu>.txt")
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--xref", action="append", help="symbol to cross-reference (repeatable)")
    ap.add_argument("--quiet", action="store_true", help="do not print the text report")
    a = ap.parse_args()
    tdir = REPO / "artifacts/mini-autosar/target" / a.ecu
    elf = pathlib.Path(a.elf) if a.elf else tdir / f"{a.ecu}.elf"
    mapf = pathlib.Path(a.map) if a.map else tdir / f"{a.ecu}.map"
    for p in (elf, mapf, pathlib.Path(a.ld)):
        if not p.exists():
            print(f"analyze_image: missing {p}", file=sys.stderr)
            return 1
    try:
        rep, warnings = analyze(elf, mapf, a.ld, a.ecu, a.top, a.xref, a.out)
    except (ToolError, ValueError, StopIteration) as e:
        print(f"analyze_image: {e!r}", file=sys.stderr)
        return 1
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{a.ecu}.md").write_text(rep.markdown(), encoding="utf-8")
    txt = rep.text()
    (out / f"{a.ecu}.txt").write_text(txt, encoding="utf-8")
    if not a.quiet:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        print(txt)
    print(f"analyze_image: {a.ecu}: {len(warnings)} warning(s); report {out / (a.ecu + '.md')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
