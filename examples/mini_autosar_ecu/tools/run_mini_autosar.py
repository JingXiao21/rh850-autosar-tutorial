#!/usr/bin/env python3
"""[Educational Implementation] run_mini_autosar.py - build + test driver of the mini AUTOSAR ECU project.

Real AUTOSAR counterpart: the integrator's build chain (generator invocation -> compile -> link -> map file
inspection -> test run). Steps (run all by default, or select with --step, repeatable):

    headers  compile tests/header_check.c with host gcc and arm-none-eabi-gcc (contract check)
    ldcheck  link bringup/stage0_hello with target/stm32l552/linker/stm32l552_autosar.ld (linker script check)
    gen      run generator/gen_rte.py for both ECUs (RTE, OS and BSW configuration -> gen/<Ecu>/, committed to the repo)
    gencc    compile every generated file + SWC with host gcc AND arm-none-eabi-gcc (-pedantic), no link: the "contract" check
    host     build host executables SensorEcu.exe / LightEcu.exe (+ map files)
    target   build target ELFs for ECU_A, ECU_B and the REST node (+ map files, --print-memory-usage)
    unit     build+run host unit tests tests/unit/test_*.c   (see "unit test convention" below)
    sim      run the two host executables (scripted rest-bus, ECU_A TX log -> ECU_B RX script) and check traces
    renode   run the three-machine Renode scenario headless and check the UART logs
    gdb      Renode GDB server + scripted arm-none-eabi-gdb batch session (Reset_Handler -> main -> task -> Can_Write)
             -> artifacts/mini-autosar/gdb_session.txt (tools/gdb/, target/renode/debug_ecu.resc)
    analyze  tools/analyze_image.py on the SensorEcu / LightEcu target ELF + map + linker script
             -> artifacts/mini-autosar/analysis/<Ecu>.{md,txt} (memory regions, per-module sizes, vectors, warnings)

Everything not yet implemented by Phase-2 agents is reported as SKIP (missing module sources: the image is compiled but
not linked) or FAIL (compile/link error) in artifacts/mini-autosar/summary.txt but never aborts the other steps.
Per image the build writes into artifacts/mini-autosar/<host|target>/<Ecu>/: objects, <Ecu>.map (-Wl,-Map, cross reference
table via -Wl,--cref), <Ecu>.link.log (target: --print-memory-usage FLASH/RAM numbers) and <Ecu>.elf / <Ecu>.exe.

Naming conventions used for source selection (documented in DESIGN.md section 13):
    *_Stm32*.c, *_Target*.c, os/port/cm33/*, target/**  -> target only
    *_Sim*.c,   *_Host*.c,   os/port/host/*, sim/host/* -> host only
    everything else                                    -> both builds
Unit test convention: tests/unit/test_<name>.c starts with comment lines
    // SOURCES: bsw/com/Com.c bsw/pdur/PduR.c ...      (paths relative to the project root; may use globs)
    // INCLUDES: tests/unit/cfg_com                     (optional extra include dirs, relative)
and is built for the host with -DMINI_PLATFORM_HOST; exit code 0 = pass.
Python tests: tests/unit/test_*.py are run with the current interpreter (generator self-tests); exit code 0 = pass.
"""
import argparse
import concurrent.futures as cf
import fnmatch
import glob
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]          # examples/mini_autosar_ecu
REPO = ROOT.parents[1]                                       # repository root
OUT = REPO / "artifacts" / "mini-autosar"
ARM_GCC = REPO / "tools/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc.exe"
RENODE = REPO / "tools/toolchains/renode_1.17.0-portable/renode.exe"
HOST_GCC = shutil.which("gcc") or "gcc"
LD_SCRIPT = ROOT / "target/stm32l552/linker/stm32l552_autosar.ld"
SIM_MS = 4000                                                # scenario length (DESIGN 11)

ECUS = {
    "A": dict(name="SensorEcu", gen="gen/SensorEcu", define="MINI_ECU_A",
              swc=["SpeedSensorSWC"], ecuc="config/ecuc/SensorEcu.ecuc.json"),
    "B": dict(name="LightEcu", gen="gen/LightEcu", define="MINI_ECU_B",
              swc=["LightControlSWC", "LightActuatorSWC", "OdometerSWC"], ecuc="config/ecuc/LightEcu.ecuc.json"),
}
RESULTS = []        # (step, item, status, detail)


# ----------------------------------------------------------------------------------------------- helpers
def record(step, item, status, detail=""):
    RESULTS.append((step, item, status, detail))
    print(f"[{status:4}] {step:8} {item}  {detail}".rstrip())


def run(cmd, cwd=None, timeout=600, log=None):
    p = subprocess.run([str(c) for c in cmd], cwd=cwd, capture_output=True, text=True, timeout=timeout)
    text = p.stdout + p.stderr
    if log:
        pathlib.Path(log).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(log).write_text(text, encoding="utf-8", errors="replace")
    return p.returncode, text


def include_dirs(ecu_key=None):
    dirs = []
    if ecu_key:
        dirs.append(ROOT / ECUS[ecu_key]["gen"])             # generated config first
    dirs += [ROOT / d for d in ["include", "os/include", "mcal/mmio", "mcal/mcu", "mcal/port", "mcal/dio", "mcal/adc",
                                "mcal/can", "ecual/canif", "ecual/iohwab", "bsw/det", "bsw/schm", "bsw/ecum",
                                "bsw/bswm", "bsw/com", "bsw/pdur", "rte", "sim/include"]]
    if ecu_key:
        dirs += [ROOT / "swc" / s for s in ECUS[ecu_key]["swc"]]
    return dirs


def is_target_only(rel):
    n = rel.name
    return bool(re.search(r"_(Stm32|Target)", n)) or rel.as_posix().startswith(("os/port/cm33/", "target/"))


def is_host_only(rel):
    n = rel.name
    return bool(re.search(r"_(Sim|Host)", n)) or rel.as_posix().startswith(("os/port/host/", "sim/host/"))


def collect(ecu_key, platform):
    """Source list of one ECU image for 'host' or 'target'."""
    pats = ["os/src/*.c", "bsw/*/*.c", "ecual/*/*.c", "mcal/mcu/*.c", "mcal/port/*.c", "mcal/dio/*.c",
            "mcal/adc/*.c", "mcal/can/*.c", "integration/*.c", ECUS[ecu_key]["gen"] + "/*.c"]
    pats += [f"swc/{s}/*.c" for s in ECUS[ecu_key]["swc"]]
    if platform == "target":
        pats += ["os/port/cm33/*.c", "os/port/cm33/*.S", "target/stm32l552/startup/*.c", "target/stm32l552/startup/*.S"]
    else:
        pats += ["os/port/host/*.c", "sim/host/*.c", "sim/restbus/*.c"]
    files = []
    for p in pats:
        files += [pathlib.Path(f).relative_to(ROOT) for f in sorted(glob.glob(str(ROOT / p)))]
    keep = []
    for f in files:
        if platform == "host" and is_target_only(f):
            continue
        if platform == "target" and is_host_only(f):
            continue
        if f.name.endswith("_Test.c"):
            continue
        keep.append(f)
    return sorted(set(keep))


def collect_rest_target():
    """Rest-bus node image (target only, no OS): RestBus + MCAL Can + minimal startup/trace."""
    pats = ["sim/restbus/*.c", "target/restbus/*.c", "mcal/mcu/*.c", "mcal/port/*.c", "mcal/can/*.c",
            "os/src/Trace.c", "os/port/cm33/Trace_Target.c", "os/port/cm33/Mini_Time_Target.c",
            "target/stm32l552/startup/*.c"]
    files = []
    for p in pats:
        files += [pathlib.Path(f).relative_to(ROOT) for f in sorted(glob.glob(str(ROOT / p)))]
    return [f for f in sorted(set(files)) if not is_host_only(f)]


def cflags(platform, ecu_define, extra_inc):
    common = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-g", f"-D{ecu_define}"]
    for d in extra_inc:
        common.append(f"-I{d}")
    if platform == "target":
        return ["-mcpu=cortex-m33", "-mthumb", "-Os", "-ffunction-sections", "-fdata-sections",
                "-DMINI_PLATFORM_TARGET"] + common
    return ["-O0", "-DMINI_PLATFORM_HOST"] + common


def compiler(platform):
    return ARM_GCC if platform == "target" else HOST_GCC


def missing_modules(platform):
    """Module source directories that do not contain any .c yet (other Phase-2 agents are still working on them)."""
    need = ["os/src", "bsw/det", "bsw/schm", "bsw/ecum", "bsw/bswm", "bsw/com", "bsw/pdur", "ecual/canif", "ecual/iohwab",
            "mcal/mcu", "mcal/port", "mcal/dio", "mcal/adc", "mcal/can", "integration"]
    need += ["os/port/cm33", "target/stm32l552/startup"] if platform == "target" else ["os/port/host", "sim/host"]
    return [d for d in need if not any((ROOT / d).glob("*.c")) and not any((ROOT / d).glob("*.S"))]


def build_image(step, label, files, platform, ecu_define, inc, outdir, ldflags_extra=None, check_modules=False, need_dirs=None):
    """Compile every file, link, write map. Returns ELF/EXE path or None."""
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / f"{label}.compile_errors.txt").unlink(missing_ok=True)       # no stale error report from an earlier run
    if not files:
        record(step, label, "SKIP", "no sources yet")
        return None
    cc = compiler(platform)
    flags = cflags(platform, ecu_define, inc)
    objs, fails = [], []

    def one(f):
        # os/port/*, sim/host/* may need GNU extensions (windows.h / inline asm)
        fl = list(flags)
        if f.as_posix().startswith(("os/port/", "sim/host/", "target/")):
            fl = [("-std=gnu99" if x == "-std=c99" else x) for x in fl]
        else:
            fl.append("-pedantic")                   # portable sources: strict ISO C99
        o = outdir / (f.as_posix().replace("/", "__") + ".o")
        rc, txt = run([cc, *fl, "-c", ROOT / f, "-o", o])
        return f, o, rc, txt

    with cf.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for f, o, rc, txt in ex.map(one, files):
            if rc != 0:
                fails.append((f, txt))
            else:
                objs.append(o)
    if fails:
        (outdir / f"{label}.compile_errors.txt").write_text("\n".join(f"== {f}\n{t}" for f, t in fails), encoding="utf-8")
        record(step, label, "FAIL", f"{len(fails)} file(s) failed to compile: {', '.join(str(f) for f, _ in fails[:4])} (see {label}.compile_errors.txt)")
        return None
    exe = outdir / (label + (".elf" if platform == "target" else ".exe"))
    mapf = outdir / (label + ".map")
    miss = missing_modules(platform) if check_modules else []
    if need_dirs is not None:                       # image with its own, smaller module list (rest-bus node)
        miss = [d for d in need_dirs if not any((ROOT / d).glob("*.c")) and not any((ROOT / d).glob("*.S"))]
    if miss:
        record(step, label, "SKIP", "compiled %d files OK, link skipped: modules not ready: %s" % (len(objs), ", ".join(miss)))
        return None
    link = [cc, *objs, "-o", exe, f"-Wl,-Map={mapf}", "-Wl,--cref"]
    if platform == "target":
        link += ["-mcpu=cortex-m33", "-mthumb", "-nostartfiles", "--specs=nano.specs", "--specs=nosys.specs",
                 "-Wl,--gc-sections", "-Wl,--print-memory-usage", f"-T{LD_SCRIPT}"]
    link += ldflags_extra or []
    rc, txt = run(link, log=outdir / f"{label}.link.log")
    if rc != 0:
        first = next((l for l in txt.splitlines() if "undefined reference" in l or "error" in l), txt[:200])
        record(step, label, "FAIL", f"link: {first.strip()[:160]}")
        return None
    mem = " | ".join(l.strip() for l in txt.splitlines() if re.match(r"\s*(FLASH|RAM|RAM2):", l))
    record(step, label, "OK", mem or str(exe.relative_to(REPO)))
    return exe


# ----------------------------------------------------------------------------------------------- steps
def step_headers():
    src = ROOT / "tests/header_check.c"
    for plat in ("host", "target"):
        for key, e in ECUS.items():
            cc = compiler(plat)
            fl = cflags(plat, e["define"], include_dirs(key)) + ["-pedantic"]    # gen/<Ecu> supplies the *_Cfg.h of that ECU
            o = OUT / "headers" / f"hc_{plat}_{key}.o"
            o.parent.mkdir(parents=True, exist_ok=True)
            rc, txt = run([cc, *fl, "-c", src, "-o", o])
            record("headers", f"{plat}/ECU_{key}", "OK" if rc == 0 else "FAIL", "" if rc == 0 else txt.strip().splitlines()[0])


def step_ldcheck():
    sources = [ROOT / "bringup/stage0_hello/startup.c", ROOT / "bringup/stage0_hello/main.c"]
    out = OUT / "ldcheck"
    out.mkdir(parents=True, exist_ok=True)
    elf = out / "ldcheck.elf"
    rc, txt = run([ARM_GCC, "-mcpu=cortex-m33", "-mthumb", "-Os", "-ffreestanding", "-nostdlib", "-Wall", "-Wextra",
                   f"-Wl,-Map={out / 'ldcheck.map'}", "-Wl,--print-memory-usage", f"-T{LD_SCRIPT}", *sources, "-o", elf])
    record("ldcheck", "stm32l552_autosar.ld", "OK" if rc == 0 else "FAIL", " | ".join(l.strip() for l in txt.splitlines() if "B " in l)[:150])


def step_gen():
    gen = ROOT / "generator/gen_rte.py"
    if not gen.exists():
        record("gen", "gen_rte.py", "SKIP", "generator not implemented yet (Phase 2, agent D)")
        return
    for key, e in ECUS.items():
        rc, txt = run([sys.executable, gen, "--swc", ROOT / "config/swc/SwcTypes.arxml", "--system",
                       ROOT / "config/system/System.arxml", "--ecuc", ROOT / e["ecuc"], "--out", ROOT / e["gen"]])
        record("gen", e["name"], "OK" if rc == 0 else "FAIL", "" if rc == 0 else txt.strip()[-200:])


def step_gencc():
    """Compile every generated .c and every SWC .c for host + target, per ECU, -pedantic, no link."""
    for plat in ("host", "target"):
        for key, e in ECUS.items():
            if not need_gen(key):
                record("gencc", f"{plat}/{e['name']}", "SKIP", f"{e['gen']} missing (run step gen)")
                continue
            files = sorted(pathlib.Path(f).relative_to(ROOT) for f in glob.glob(str(ROOT / e["gen"] / "*.c")))
            files += [pathlib.Path(f).relative_to(ROOT) for s_ in e["swc"] for f in sorted(glob.glob(str(ROOT / "swc" / s_ / "*.c")))]
            fl = cflags(plat, e["define"], include_dirs(key)) + ["-pedantic"]
            bad = []
            for f in files:
                o = OUT / "gencc" / plat / e["name"] / (f.as_posix().replace("/", "__") + ".o")
                o.parent.mkdir(parents=True, exist_ok=True)
                rc, txt = run([compiler(plat), *fl, "-c", ROOT / f, "-o", o])
                if rc != 0:
                    bad.append((f, txt.strip().splitlines()[0] if txt.strip() else ""))
            record("gencc", f"{plat}/{e['name']}", "OK" if not bad else "FAIL",
                   f"{len(files)} files" if not bad else "; ".join(f"{f}: {t}" for f, t in bad[:2]))


def need_gen(key):
    return any((ROOT / ECUS[key]["gen"]).glob("Os_Cfg.c"))


def step_build(platform):
    for key, e in ECUS.items():
        if not need_gen(key):
            record(platform, e["name"], "SKIP", f"{e['gen']}/Os_Cfg.c missing (run step gen)")
            continue
        files = collect(key, platform)
        if platform == "target":
            build_image("target", f"{e['name']}", files, "target", e["define"], include_dirs(key), OUT / "target" / e["name"], check_modules=True)
        else:
            build_image("host", f"{e['name']}", files, "host", e["define"], include_dirs(key), OUT / "host" / e["name"], check_modules=True)
    if platform == "target":
        files = collect_rest_target()
        if any(f.parts[0] == "target" and f.parts[1] == "restbus" for f in files):
            build_image("target", "RestBus", files, "target", "MINI_ECU_REST", [ROOT / "target/restbus"] + include_dirs(None),
                        OUT / "target" / "RestBus", check_modules=True, need_dirs=["os/port/cm33", "target/stm32l552/startup", "mcal/can"])
        else:
            record("target", "RestBus", "SKIP", "target/restbus/main_restbus.c missing (agent C)")


def step_unit():
    tests = sorted((ROOT / "tests/unit").glob("test_*.c"))
    if not tests:
        record("unit", "tests/unit", "SKIP", "no unit tests yet")
    for t in sorted((ROOT / "tests/unit").glob("test_*.py")):
        rc, txt = run([sys.executable, t], timeout=300, log=OUT / "unit" / t.stem / "run.log")
        last = txt.strip().splitlines()[-1][:120] if txt.strip() else ""
        record("unit", t.stem + " (python)", "OK" if rc == 0 else "FAIL", last)
    for t in tests:
        head = t.read_text(encoding="utf-8", errors="replace").splitlines()[:12]
        srcs, incs = [], []
        for l in head:
            m = re.match(r"//\s*SOURCES:\s*(.*)", l)
            if m:
                for pat in m.group(1).split():
                    srcs += [pathlib.Path(g).relative_to(ROOT) for g in sorted(glob.glob(str(ROOT / pat)))]
            m = re.match(r"//\s*INCLUDES:\s*(.*)", l)
            if m:
                incs += [ROOT / p for p in m.group(1).split()]
        inc = incs + include_dirs("B")
        exe = build_image("unit", t.stem, [t.relative_to(ROOT)] + srcs, "host", "MINI_UNIT_TEST", inc, OUT / "unit" / t.stem)
        if exe:
            rc, txt = run([exe], timeout=60, log=OUT / "unit" / t.stem / "run.log")
            record("unit", t.stem + " (run)", "OK" if rc == 0 else "FAIL", "" if rc == 0 else txt.strip().splitlines()[-1][:120])


# ----------------------------------------------------------------------------------------------- scenario checks
LINE = re.compile(r"^\[(\d{9})\] (\w{4}) (\w+)\s+(.*)$")


def parse_trace(path):
    ev = []
    for l in pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        m = LINE.match(l.strip())
        if m:
            ev.append((int(m.group(1)), m.group(2), m.group(3), m.group(4)))
    return ev


def first_t(ev, cat, pattern, not_before=0):
    for t, _, c, msg in ev:
        if t >= not_before and c == cat and re.search(pattern, msg):
            return t
    return None


def check_scenario(log_a, log_b, label):
    """Expectations of DESIGN section 11.3; times in us, tolerance +-60 ms unless stated."""
    ok = True
    try:
        a, b = parse_trace(log_a), parse_trace(log_b)
    except FileNotFoundError as e:
        record(label, "traces", "SKIP", f"missing {e.filename}")
        return
    checks = [
        ("A: EcuM startup reaches RUN",           a, "BSW",  r"ECUM STATE RUN",              0,      200_000),
        ("A: OS StartOS",                         a, "OS",   r"STARTOS",                     0,      100_000),
        ("A: first VehicleSpeed frame 0x101",     a, "CAN",  r"^TX id=0x101 ",               0,      60_000),
        ("B: EcuM startup reaches RUN",           b, "BSW",  r"ECUM STATE RUN",              0,      200_000),
        ("B: first AmbientLight RX 0x301",        b, "CAN",  r"^RX id=0x301 ",               0,      250_000),
        ("B: headlight LOW  (dusk, speed>0)",     b, "SWC",  r"LightCtl cmd=1 ",             500_000,  800_000),
        ("B: headlight HIGH (dark, speed>=60)",   b, "SWC",  r"LightCtl cmd=2 ",             1_500_000, 1_800_000),
        ("B: mode POST_RUN",                      b, "RTE",  r"MODE EcuMode=POST_RUN",       3_000_000, 3_200_000),
        ("B: headlight OFF in POST_RUN",          b, "SWC",  r"LightCtl cmd=0 .*postrun=1",  3_000_000, 3_200_000),
        ("B: mode back to RUN",                   b, "RTE",  r"MODE EcuMode=RUN",            3_500_000, 3_700_000),
        ("B: HeadlightStatus frame 0x201",        b, "CAN",  r"^TX id=0x201 ",               0,      300_000),
        ("B: Odometer GetDistance served",        b, "RTE",  r"CALL R_Odometer_GetDistance", 0,      400_000),
    ]
    for name, ev, cat, pat, lo, hi in checks:
        t = first_t(ev, cat, pat, lo)
        good = t is not None and t <= hi + 60_000
        ok &= good
        record(label, name, "OK" if good else "FAIL", f"t={t}us" if t is not None else "event not found")
    return ok


def step_sim():
    out = OUT / "host"
    exe_a, exe_b = out / "SensorEcu/SensorEcu.exe", out / "LightEcu/LightEcu.exe"
    if not (exe_a.exists() and exe_b.exists()):
        record("sim", "two-ECU host run", "SKIP", "host executables not built")
        return
    txlog = out / "ecuA_can_tx.txt"
    rc1, _ = run([exe_a, "--run-ms", SIM_MS, "--log", out / "ecuA.log", "--can-tx-log", txlog], timeout=120)
    rc2, _ = run([exe_b, "--run-ms", SIM_MS, "--log", out / "ecuB.log", "--can-rx-script", txlog, "--restbus",
                  "--can-tx-log", out / "ecuB_can_tx.txt"], timeout=120)
    record("sim", "SensorEcu.exe", "OK" if rc1 == 0 else "FAIL", f"rc={rc1}")
    record("sim", "LightEcu.exe", "OK" if rc2 == 0 else "FAIL", f"rc={rc2}")
    check_scenario(out / "ecuA.log", out / "ecuB.log", "sim")


def write_profile(path):
    """Wheel-speed profile for Renode, identical to SimAdc_ProfileRaw(): raw = 0 (t<=200 ms) else min(3000, 3*(t-200))."""
    lines = ['mach set "ECU_A"']
    for t in range(0, SIM_MS, 10):
        raw = 0 if t <= 200 else min(3000, 3 * (t - 200))
        uv = round(raw * 3300000 / 4095)     # SetVoltage takes MICROVOLT (verified in Renode 1.17.0, see mcal/adc/Adc.c); raw ~ floor(uV*4095/3.3e6), +-1 LSB
        lines += [f"sysbus.adc1 SetVoltage {uv} 6", 'emulation RunFor "0.010"']
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def step_analyze():
    """Map/ELF/linker-script analysis of both ECU target images (tools/analyze_image.py)."""
    tool = ROOT / "tools/analyze_image.py"
    for e in ECUS.values():
        elf = OUT / "target" / e["name"] / f"{e['name']}.elf"
        if not elf.exists():
            record("analyze", e["name"], "SKIP", "target ELF not built")
            continue
        rc, txt = run([sys.executable, tool, "--ecu", e["name"], "--quiet"], log=OUT / "analysis" / f"{e['name']}.run.log")
        last = txt.strip().splitlines()[-1][:140] if txt.strip() else ""
        record("analyze", e["name"], "OK" if rc == 0 else "FAIL", last)


def step_gdb():
    """Renode GDB server + scripted arm-none-eabi-gdb batch session (tools/gdb/run_gdb_session.py) -> artifacts/mini-autosar/gdb_session.txt."""
    elf = OUT / "target/LightEcu/LightEcu.elf"
    if not elf.exists():
        record("gdb", "LightEcu debug session", "SKIP", "target ELF not built")
        return
    rc, txt = run([sys.executable, ROOT / "tools/gdb/run_gdb_session.py"], timeout=400, log=OUT / "gdb" / "run.log")
    record("gdb", "Reset->main->TASK->Can_Write", "OK" if rc == 0 else "FAIL", txt.strip().splitlines()[-1][:140] if txt.strip() else "")


def step_renode():
    t = OUT / "target"
    elfs = {k: t / n / f"{n}.elf" for k, n in (("a", "SensorEcu"), ("b", "LightEcu"), ("rest", "RestBus"))}
    if not all(p.exists() for p in elfs.values()):
        record("renode", "three-machine scenario", "SKIP", "target ELFs not built: " + ", ".join(k for k, p in elfs.items() if not p.exists()))
        return
    rdir = OUT / "renode"
    rdir.mkdir(parents=True, exist_ok=True)
    prof = rdir / "adc_profile.resc"
    write_profile(prof)
    logs = {k: rdir / f"uart_{k}.log" for k in elfs}
    for p in logs.values():
        p.write_text("")
    cmd = f'$elf_a=@{elfs["a"].as_posix()}; $elf_b=@{elfs["b"].as_posix()}; $elf_rest=@{elfs["rest"].as_posix()}; ' \
          f'$uart_a=@{logs["a"].as_posix()}; $uart_b=@{logs["b"].as_posix()}; $uart_rest=@{logs["rest"].as_posix()}; ' \
          f'$profile=@{prof.as_posix()}; include @{(ROOT / "target/renode/mini_autosar_2ecu.resc").as_posix()}; quit'
    rc, txt = run([RENODE, "--disable-gui", "--console", "-e", cmd], timeout=900, log=rdir / "renode.log")
    record("renode", "renode.exe run", "OK" if rc == 0 else "FAIL", f"rc={rc}")
    check_scenario(logs["a"], logs["b"], "renode")


# ----------------------------------------------------------------------------------------------- main
def write_summary():
    OUT.mkdir(parents=True, exist_ok=True)
    n = {s: sum(1 for r in RESULTS if r[2] == s) for s in ("OK", "FAIL", "SKIP")}
    lines = [f"mini_autosar_ecu build/test summary  ({time.strftime('%Y-%m-%d %H:%M:%S')})",
             f"OK={n['OK']} FAIL={n['FAIL']} SKIP={n['SKIP']}", ""]
    lines += [f"{st:5}{step:9}{item:46}{detail}" for step, item, st, detail in RESULTS]
    (OUT / "summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines[:2]), f"\nsummary: {OUT / 'summary.txt'}")
    return n["FAIL"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--step", action="append", choices=["headers", "ldcheck", "gen", "gencc", "host", "target", "unit", "sim", "renode", "analyze", "gdb", "all"])
    ap.add_argument("--clean", action="store_true", help="delete the build/run directories under artifacts/mini-autosar first")
    a = ap.parse_args()
    steps = a.step or ["all"]
    if "all" in steps:
        steps = ["gen", "headers", "ldcheck", "gencc", "host", "target", "analyze", "unit", "sim", "renode", "gdb"]
    if a.clean:                                      # only the directories this script owns (not artifacts/mini-autosar/stage0)
        for d in ("headers", "ldcheck", "gencc", "host", "target", "unit", "renode", "analysis", "gdb"):
            shutil.rmtree(OUT / d, ignore_errors=True)
    OUT.mkdir(parents=True, exist_ok=True)
    table = {"headers": step_headers, "ldcheck": step_ldcheck, "gen": step_gen, "gencc": step_gencc, "host": lambda: step_build("host"),
             "target": lambda: step_build("target"), "unit": step_unit, "sim": step_sim, "renode": step_renode, "analyze": step_analyze, "gdb": step_gdb}
    for s in steps:
        try:
            table[s]()
        except Exception as ex:                      # a broken step must not stop the others
            record(s, "exception", "FAIL", repr(ex)[:160])
    sys.exit(1 if write_summary() else 0)


if __name__ == "__main__":
    main()
