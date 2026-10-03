#!/bin/sh
# [Educational Implementation] Build and run the headless Renode MCAL/CAN smoke test (two machines + CANHub), print the UART logs
# and return non-zero unless both nodes report PASS.  usage: target/renode/run_smoke.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; REPO="$(cd "$ROOT/../.." && pwd)"
RENODE="${RENODE:-$REPO/tools/toolchains/renode_1.17.0-portable/renode.exe}"
OUT="$REPO/artifacts/mini-autosar/renode-bringup"
sh "$HERE/build_bringup.sh" "$OUT" >/dev/null
: > "$OUT/uart_smoke_a.log"; : > "$OUT/uart_smoke_b.log"
# Windows Renode wants drive-letter paths with forward slashes
W() { (cd "$(dirname "$1")" && printf '%s/%s' "$(pwd -W 2>/dev/null || pwd)" "$(basename "$1")"); }
"$RENODE" --disable-gui --console -e "\$elf_a=@$(W "$OUT/smoke_a.elf"); \$elf_b=@$(W "$OUT/smoke_b.elf"); \$uart_a=@$(W "$OUT/uart_smoke_a.log"); \$uart_b=@$(W "$OUT/uart_smoke_b.log"); include @$(W "$HERE/smoke/mcal_can_smoke.resc"); quit" > "$OUT/renode_smoke.log" 2>&1 || true
echo "=== node A ==="; cat "$OUT/uart_smoke_a.log"; echo "=== node B ==="; cat "$OUT/uart_smoke_b.log"
grep -q "SMOKE A PASS" "$OUT/uart_smoke_a.log" && grep -q "SMOKE B PASS" "$OUT/uart_smoke_b.log" && echo "RENODE CAN SMOKE: PASS"
