"""Build and run the educational UDS diagnostic stack (examples/uds_diag_demo) on the host.

[Educational Implementation] Compiles with host gcc (C99, -Wall -Wextra -Werror -pedantic),
runs the host tests and the scripted demo. Never touches target hardware.

Outputs (artifacts/uds-demo/):
  results.txt  compiler version, build commands, test output, exit codes
  trace.txt    layer-by-layer trace of the scripted UDS session (main_demo)
"""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "examples/uds_diag_demo"
OUT = ROOT / "artifacts/uds-demo"

INCLUDES = ["general", "sim", "mcal", "ecual", "com", "diag", "mem", "rte", "swc", "integration"]

STACK_SOURCES = [
    "general/Det.c", "general/UdsTrace.c", "general/SimClock.c",
    "sim/VirtualCanBus.c", "sim/UdsTester.c", "sim/SimHarness.c",
    "mcal/Can.c", "mcal/Can_Cfg.c",
    "ecual/CanIf.c", "ecual/CanIf_Cfg.c",
    "com/CanTp.c", "com/CanTp_Cfg.c", "com/PduR.c", "com/PduR_Cfg.c",
    "diag/Dcm.c", "diag/Dcm_Dsl.c", "diag/Dcm_Dsd.c", "diag/Dcm_Dsp.c", "diag/Dcm_Cfg.c", "diag/Dem.c",
    "mem/NvM.c",
    "rte/Rte_Dcm.c",
    "swc/VehicleInfoSWC.c", "swc/SecurityAccessSWC.c",
    "integration/EcuM.c", "integration/BswScheduler.c",
]

FLAGS = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic"]


def find_gcc():
    compiler = shutil.which("gcc")
    if not compiler:
        fallback = Path("C:/D_disk/software/TDM_GCC/bin/gcc.exe")
        if fallback.exists():
            compiler = str(fallback)
    if not compiler:
        raise SystemExit("Host gcc not found on PATH. RH850 target tools are not used here.")
    return compiler


def build(compiler, main_source, exe, log):
    cmd = [compiler] + FLAGS
    for inc in INCLUDES:
        cmd += ["-I", str(BASE / inc)]
    cmd += [str(BASE / s) for s in STACK_SOURCES + [main_source]] + ["-o", str(exe)]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    log.append("BUILD " + exe.name + "\n" + subprocess.list2cmdline(cmd) + "\n" + proc.stdout + proc.stderr)
    log.append(f"build exit code: {proc.returncode}\n")
    return proc.returncode


def main():
    compiler = find_gcc()
    OUT.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    test_exe = OUT / ("test_uds_demo" + suffix)
    demo_exe = OUT / ("uds_demo" + suffix)
    log = [subprocess.run([compiler, "--version"], capture_output=True, text=True, check=True).stdout]

    rc = build(compiler, "tests/test_uds_demo.c", test_exe, log)
    rc |= build(compiler, "integration/main_demo.c", demo_exe, log)
    if rc:
        text = "\n".join(log)
        (OUT / "results.txt").write_text(text, encoding="utf-8")
        raise SystemExit(text)

    test = subprocess.run([str(test_exe)], capture_output=True, text=True)
    log.append("RUN TESTS\n" + test.stdout + test.stderr + f"test exit code: {test.returncode}\n")

    demo = subprocess.run([str(demo_exe)], capture_output=True, text=True)
    (OUT / "trace.txt").write_text(demo.stdout + demo.stderr, encoding="utf-8")
    trace_lines = demo.stdout.count("\n")
    log.append(f"RUN DEMO\ndemo exit code: {demo.returncode}, trace lines: {trace_lines} -> artifacts/uds-demo/trace.txt\n")

    (OUT / "results.txt").write_text("\n".join(log), encoding="utf-8")
    print(test.stdout, end="")
    print(f"demo exit code {demo.returncode}, {trace_lines} trace lines written to {OUT / 'trace.txt'}")
    raise SystemExit(test.returncode or demo.returncode)


if __name__ == "__main__":
    main()
