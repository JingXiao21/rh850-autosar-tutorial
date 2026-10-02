"""Build and run reference C tests with the host GCC; never accesses target MMIO."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "examples/rh850_mcal_reference"
OUT = ROOT / "artifacts/host-build"


def main():
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host gcc not found on PATH. RH850 target tools are not used here.")
    OUT.mkdir(parents=True, exist_ok=True)
    exe = OUT / ("test_reference.exe" if os.name == "nt" else "test_reference")
    includes = ["platform", "mcal/gpt", "mcal/can", "integration"]
    sources = ["tests/test_reference.c", "platform/Rh850_Mmio.c",
               "mcal/gpt/Ostm.c", "mcal/can/Can_BitTiming.c",
               "integration/Tick_Accumulator.c"]
    cmd = [compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic"]
    for include in includes:
        cmd += ["-I", str(BASE / include)]
    cmd += [str(BASE / source) for source in sources] + ["-o", str(exe)]
    version = subprocess.run([compiler, "--version"], capture_output=True, text=True, check=True).stdout
    build = subprocess.run(cmd, capture_output=True, text=True)
    log = version + "\nBUILD\n" + subprocess.list2cmdline(cmd) + "\n" + build.stdout + build.stderr
    if build.returncode:
        (OUT / "results.txt").write_text(log, encoding="utf-8")
        raise SystemExit(log)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    log += "\nRUN\n" + run.stdout + run.stderr + f"\nExit code: {run.returncode}\n"
    (OUT / "results.txt").write_text(log, encoding="utf-8")
    print(run.stdout, end="")
    if run.stderr:
        print(run.stderr, end="")
    raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()
