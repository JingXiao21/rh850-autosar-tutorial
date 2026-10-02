"""构建 ECU 启动及 CAN FIFO / EI190 教学项目，执行测试并保存逐层日志。"""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "examples" / "can_irq_demo"
OUT = ROOT / "artifacts" / "can-irq-demo"
FLAGS = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic"]
SOURCES = ["src/can_rx.c", "src/sim_hw.c", "src/sim_cpu.c", "src/demo_setup.c"]


def main():
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("需要 PATH 中的主机 gcc；本项目不使用 RH850 编译器。")
    OUT.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    log = []

    def run(command):
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        log.append(subprocess.list2cmdline(command) + "\n" + result.stdout +
                   result.stderr + f"exit={result.returncode}\n")
        if result.returncode:
            (OUT / "results.txt").write_text("\n".join(log), encoding="utf-8")
            raise SystemExit(log[-1])
        return result.stdout

    run([compiler, "--version"])
    common = [compiler, *FLAGS, "-I", str(BASE / "include")]
    for entry, name in [("tests/test_demo.c", "tests"), ("src/main.c", "demo")]:
        run([*common, *[str(BASE / source) for source in SOURCES],
             str(BASE / entry), "-o", str(OUT / (name + suffix))])
    for entry, name in [("test_boot.c", "boot_tests"), ("boot_main.c", "boot_demo")]:
        run([*common, "-I", str(BASE / "startup"),
             str(BASE / "startup/boot_model.c"), str(BASE / "startup" / entry),
             "-o", str(OUT / (name + suffix))])
    # 仅检查 C 语法；不链接、更不运行含真实 MMIO 地址的目标适配层。
    run([*common, "-DCAN_IRQ_DEMO_TARGET_PORT", "-c",
         str(BASE / "target/rh850_mmio_port.c"), "-o", str(OUT / "target_port_syntax.o")])
    run([*common, "-DCAN_IRQ_DEMO_TARGET_BOOT", "-c",
         str(BASE / "startup/target/boot_target.c"), "-o", str(OUT / "boot_target_syntax.o")])
    tests = run([str(OUT / ("tests" + suffix))])
    boot_tests = run([str(OUT / ("boot_tests" + suffix))])
    trace = run([str(OUT / ("demo" + suffix))])
    boot_trace = run([str(OUT / ("boot_demo" + suffix))])
    (OUT / "trace.txt").write_text(trace, encoding="utf-8")
    (OUT / "boot_trace.txt").write_text(boot_trace, encoding="utf-8")
    (OUT / "results.txt").write_text("\n".join(log), encoding="utf-8")
    print(tests, end="")
    print(boot_tests, end="")
    print(f"Trace: {OUT / 'trace.txt'}")
    print(f"Boot trace: {OUT / 'boot_trace.txt'}")
    print("目标 C 文件仅做主机语法检查；启动汇编未经目标工具链验证，未生成 RH850 镜像。")


if __name__ == "__main__":
    main()
