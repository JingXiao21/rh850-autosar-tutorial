#!/usr/bin/env python3
"""[Educational Implementation] OS-only Cortex-M33 smoke test: build + run in Renode + check the UART trace.

Real AUTOSAR counterpart: none (integration test of the OS port alone).  Builds the portable kernel (os/src),
the cm33 port, the startup code and the hand-written configuration/app in this directory with
arm-none-eabi-gcc (-Wall -Wextra -Werror), runs the ELF headless in Renode (board ramn.repl, USART1 -> file)
and checks the order of the trace lines (preemption by priority, priority ceiling, event wake-up, Cat2 ISR).

    python examples/mini_autosar_ecu/os/port/cm33/selftest/run_selftest.py
Outputs go to artifacts/mini-autosar/os-selftest/ (gitignored).
"""
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]                              # examples/mini_autosar_ecu
REPO = ROOT.parents[1]
OUT = REPO / "artifacts" / "mini-autosar" / "os-selftest"
GCC = REPO / "tools/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc.exe"
RENODE = REPO / "tools/toolchains/renode_1.17.0-portable/renode.exe"
LD = ROOT / "target/stm32l552/linker/stm32l552_autosar.ld"

SOURCES = (sorted((ROOT / "os/src").glob("*.c")) + sorted((ROOT / "os/port/cm33").glob("*.c"))
           + [ROOT / "target/stm32l552/startup/startup.c", HERE / "Os_Cfg.c", HERE / "main.c"])


def run(cmd, **kw):
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True, **kw)
    return p.returncode, p.stdout + p.stderr


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    inc = [HERE, ROOT / "include", ROOT / "os/include", ROOT / "os/src"]
    objs = []
    for src in SOURCES:
        std = "-std=gnu99" if src.parent != ROOT / "os/src" else "-std=c99"
        obj = OUT / (src.name + ".o")
        cmd = [GCC, "-mcpu=cortex-m33", "-mthumb", "-Os", "-g", "-ffunction-sections", "-fdata-sections", std,
               "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-DMINI_PLATFORM_TARGET", "-DMINI_ECU_A"]
        cmd += [f"-I{d}" for d in inc] + ["-c", src, "-o", obj]
        rc, txt = run(cmd)
        if rc != 0:
            print(f"COMPILE FAIL {src}\n{txt}")
            sys.exit(1)
        objs.append(obj)
    elf = OUT / "os_selftest.elf"
    rc, txt = run([GCC, *objs, "-o", elf, "-mcpu=cortex-m33", "-mthumb", "-nostartfiles", "--specs=nano.specs",
                   "--specs=nosys.specs", "-Wl,--gc-sections", "-Wl,--print-memory-usage",
                   f"-Wl,-Map={OUT / 'os_selftest.map'}", f"-T{LD}"])
    print(txt.strip())
    if rc != 0:
        print("LINK FAIL")
        sys.exit(1)
    return elf


def simulate(elf):
    uart = OUT / "uart.log"
    uart.write_text("")
    resc = HERE / "selftest.resc"
    cmd = f'$elf=@{elf.as_posix()}; $uartlog=@{uart.as_posix()}; include @{resc.as_posix()}; ' \
          f'emulation RunFor "0.4"; quit'
    rc, txt = run([RENODE, "--disable-gui", "--console", "-e", cmd], timeout=300)
    (OUT / "renode.log").write_text(txt, encoding="utf-8", errors="replace")
    return uart.read_text(encoding="utf-8", errors="replace")


EXPECT = [   # (regex, description) - must appear in this order
    r"OS\s+STARTOS mode=0",
    r"OS\s+TASK_START Task_Ext prio=3",
    r"SIM\s+EXT start",
    r"OS\s+TASK_WAIT Task_Ext mask=0x3",
    r"OS\s+TASK_START Task_Low prio=1",
    r"SIM\s+LOW got Res_X",
    r"OS\s+ALARM Alarm_Hi",                          # Hi activated at 20 ms ...
    r"SIM\s+LOW releasing Res_X",                    # ... but cannot run before the release (ceiling)
    r"OS\s+TASK_START Task_Hi prio=5",               # preempts exactly at ReleaseResource
    r"SIM\s+HI run",
    r"OS\s+TASK_END Task_Hi",
    r"SIM\s+LOW resumed after Hi",
    r"OS\s+ALARM Alarm_Ev",                          # 50 ms: event for Ext preempts Low
    r"SIM\s+EXT woken ev=0x1",
    r"SIM\s+LOW pends IRQ",
    r"OS\s+ISR_ENTER Isr_Sw",
    r"OS\s+ISR_EXIT Isr_Sw",
    r"SIM\s+EXT woken ev=0x2",                       # Ext runs right after the ISR, before Low continues
    r"SIM\s+LOW after IRQ",
    r"SIM\s+EXT woken ev=0x1",                       # 100 ms
    r"SIM\s+LOW end",
    r"SIM\s+EXT woken ev=0x1",                       # 150 ms
    r"SIM\s+SELFTEST PASS",
    r"SIM\s+ShutdownHook err=0",
]


def check(text):
    lines = text.splitlines()
    pos = 0
    ok = True
    for pat in EXPECT:
        for i in range(pos, len(lines)):
            if re.search(pat, lines[i]):
                pos = i + 1
                print(f"  OK   {pat}   <- {lines[i].strip()}")
                break
        else:
            print(f"  FAIL {pat}")
            ok = False
            break
    # the Hi task must not start before the release line (priority ceiling)
    t_rel = next((i for i, l in enumerate(lines) if "LOW releasing Res_X" in l), None)
    t_hi = next((i for i, l in enumerate(lines) if "TASK_START Task_Hi" in l), None)
    if t_rel is None or t_hi is None or t_hi < t_rel:
        print("  FAIL Hi started before Low released Res_X")
        ok = False
    return ok


def main():
    elf = build()
    text = simulate(elf)
    print("---- UART ----")
    print(text)
    print("---- checks ----")
    ok = check(text)
    print("OS target selftest:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
