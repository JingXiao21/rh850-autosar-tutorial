#!/usr/bin/env python3
"""[Educational Implementation] run_gdb_session.py - start Renode headless with a GDB server, run a scripted arm-none-eabi-gdb
batch session against it and save the transcript to artifacts/mini-autosar/gdb_session.txt.

    python tools/gdb/run_gdb_session.py [--ecu LightEcu] [--port 3333] [--script tools/gdb/demo_session.gdb]

Exit code 0 when every expected stop (Reset_Handler, main, EcuM_Init, StartOS, Os_Kernel_SelectNext, Task_LightCtl,
LightCtl_Run20ms, Com_SendSignal, Can_Write) shows up in the transcript.
"""
import argparse
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
REPO = ROOT.parents[1]
OUT = REPO / "artifacts/mini-autosar"
GDB = REPO / "tools/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gdb.exe"
RENODE = REPO / "tools/toolchains/renode_1.17.0-portable/renode.exe"
EXPECT = [r"Breakpoint \d+, main \(\)", r"Breakpoint \d+, EcuM_Init", r"Breakpoint \d+, StartOS", r"Breakpoint \d+, Os_Kernel_SelectNext",
          r"Temporary breakpoint \d+, Os_Task_Task_LightCtl", r"Temporary breakpoint \d+, LightCtl_Run20ms",
          r"Temporary breakpoint \d+, Com_SendSignal", r"Temporary breakpoint \d+, Can_Write", r"pc=0x[0-9a-f]+ sp=0x[0-9a-f]+"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ecu", default="LightEcu")
    ap.add_argument("--port", type=int, default=3333)
    ap.add_argument("--script", default=str(ROOT / "tools/gdb/demo_session.gdb"))
    ap.add_argument("--out", default=str(OUT / "gdb_session.txt"))
    a = ap.parse_args()
    elf = OUT / "target" / a.ecu / f"{a.ecu}.elf"
    if not elf.exists():
        print(f"missing {elf} (run: python tools/run_mini_autosar.py --step target)", file=sys.stderr)
        return 1
    wdir = OUT / "gdb"
    wdir.mkdir(parents=True, exist_ok=True)
    uart, rlog = wdir / f"uart_{a.ecu}.log", wdir / "renode_gdb.log"
    uart.write_text("")
    resc = ROOT / "target/renode/debug_ecu.resc"
    cmd = f"$elf=@{elf.as_posix()}; $uart=@{uart.as_posix()}; $gdb_port={a.port}; include @{resc.as_posix()}"
    with open(rlog, "w") as lf:
        rp = subprocess.Popen([str(RENODE), "--disable-gui", "--console", "-e", cmd], stdin=subprocess.PIPE, stdout=lf,
                              stderr=subprocess.STDOUT, text=True)
        try:
            for _ in range(200):                                  # wait until the GDB server is up (max 40 s)
                if "GDB server with all CPUs started" in rlog.read_text(errors="replace"):
                    break
                if rp.poll() is not None:
                    break
                time.sleep(0.2)
            else:
                print("Renode GDB server did not start", file=sys.stderr)
            gcmd = [str(GDB), "-batch", "-nx", "-ex", f"set $mini_port = {a.port}", "-x", str(ROOT / "tools/gdb/mini_autosar.gdb"),
                    "-x", a.script, str(elf)]
            g = subprocess.run(gcmd, capture_output=True, text=True, errors="replace", timeout=240, cwd=REPO)
            err = [l for l in g.stderr.splitlines() if l.strip() and "could not convert" not in l and "This normally should not happen" not in l]
            gtxt = g.stdout + (("\n[stderr]\n" + "\n".join(err)) if err else "")    # the CP1252->UTF-32 iconv warning of this gdb build is harmless
        finally:
            try:
                rp.stdin.write("quit\n"); rp.stdin.flush()
                rp.wait(timeout=10)
            except Exception:
                rp.kill()
    ver = subprocess.run([str(GDB), "--version"], capture_output=True, text=True).stdout.splitlines()[0]
    ok = all(re.search(p, gtxt) for p in EXPECT)
    header = [f"mini_autosar_ecu GDB session ({time.strftime('%Y-%m-%d %H:%M:%S')})", f"gdb: {ver}", "renode: 1.17.0 (headless, machine StartGdbServer)",
              f"image: {elf.relative_to(REPO).as_posix()}", f"renode script: target/renode/debug_ecu.resc, gdb scripts: tools/gdb/mini_autosar.gdb + {pathlib.Path(a.script).name}",
              "result: " + ("all expected stops reached" if ok else "MISSING stops: " + ", ".join(p for p in EXPECT if not re.search(p, gtxt))), "",
              "=" * 100, ""]
    uart_txt = uart.read_text(errors="replace").strip().splitlines()
    tail = ["", "=" * 100, "UART log of the debugged ECU up to the point where the debugger detached (first 12 lines):", ""] + uart_txt[:12]
    pathlib.Path(a.out).write_text("\n".join(header) + gtxt + "\n".join(tail) + "\n", encoding="utf-8")
    print(f"gdb session: {'OK' if ok else 'FAIL'} -> {a.out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
