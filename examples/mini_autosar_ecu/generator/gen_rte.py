#!/usr/bin/env python3
"""
gen_rte.py - command line entry of the mini RTE / OS / BSW-configuration generator.

[Educational Implementation]
Real AUTOSAR counterpart: running the RTE generator + the BSW configurators for one ECU. Usage (DESIGN.md section 7):

    python generator/gen_rte.py --swc config/swc/SwcTypes.arxml --system config/system/System.arxml \
                                --ecuc config/ecuc/LightEcu.ecuc.json --out gen/LightEcu

Output is deterministic (no time stamps, sorted/ordered by input order) so that the generated files can be committed and
diffed. Exit code 1 and all validation errors on stderr if the inputs are inconsistent; nothing is written in that case.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from arxml_model import load_inputs, resolve, GenError          # noqa: E402
import emit_rte                                                  # noqa: E402
import emit_os                                                   # noqa: E402
import emit_bsw                                                  # noqa: E402


def generate(swc_path, system_path, ecuc_path):
    """returns an ordered {file name: text} dict (raises GenError)."""
    inp = load_inputs(swc_path, system_path, ecuc_path)
    ecu = resolve(inp)
    errs = emit_bsw.validate_bsw(ecu)
    if errs:
        raise GenError(errs)
    files = {}
    files["Rte_Type.h"] = emit_rte.gen_rte_type_h(ecu)
    files["Rte.h"] = emit_rte.gen_rte_h(ecu)
    files["Rte_Cbk.h"] = emit_rte.gen_rte_cbk_h(ecu)
    for p in ecu.apps:
        if p.type.pims:
            fn, txt = emit_rte.gen_swc_type_h(ecu, p)
            files[fn] = txt
        fn, txt = emit_rte.gen_swc_h(ecu, p)
        files[fn] = txt
    files["Rte.c"] = emit_rte.gen_rte_c(ecu)
    files["Rte_Tasks.c"] = emit_rte.gen_rte_tasks_c(ecu)
    files["Os_Cfg.h"] = emit_os.gen_os_cfg_h(ecu)
    files["Os_Cfg.c"] = emit_os.gen_os_cfg_c(ecu)
    files.update(emit_bsw.gen_all_bsw(ecu))
    return files


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--swc", required=True)
    ap.add_argument("--system", required=True)
    ap.add_argument("--ecuc", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    try:
        files = generate(a.swc, a.system, a.ecuc)
    except GenError as e:
        sys.stderr.write(str(e) + "\n")
        sys.stderr.write("generation aborted: %d error(s)\n" % len(e.errors))
        return 1
    os.makedirs(a.out, exist_ok=True)
    # remove stale generated files of earlier runs (only ours: .c/.h)
    for f in os.listdir(a.out):
        if f.endswith((".c", ".h")) and f not in files:
            os.remove(os.path.join(a.out, f))
    for name, text in files.items():
        with open(os.path.join(a.out, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print("generated %d files into %s" % (len(files), a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
