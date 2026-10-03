#!/bin/sh
# [Educational Implementation] Build the Renode bring-up programs with arm-none-eabi-gcc -Werror:
#   probe.elf        register-behaviour probe (RCC / GPIO / ADC1 / FDCAN1 read-backs)
#   smoke_a.elf      MCAL smoke test, node A (sender, polling RX)
#   smoke_b.elf      MCAL smoke test, node B (receiver, RX interrupt)
# usage: target/renode/build_bringup.sh [outdir]       (default outdir artifacts/mini-autosar/renode-bringup)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; REPO="$(cd "$ROOT/../.." && pwd)"
GCC="${ARM_GCC:-$REPO/tools/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc}"
OUT="${1:-$REPO/artifacts/mini-autosar/renode-bringup}"; mkdir -p "$OUT"
CF="-mcpu=cortex-m33 -mthumb -Os -std=gnu99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -g -DMINI_PLATFORM_TARGET"
INC="-I$ROOT/include -I$ROOT/os/include -I$ROOT/mcal/mmio -I$ROOT/mcal/mcu -I$ROOT/mcal/port -I$ROOT/mcal/dio -I$ROOT/mcal/adc -I$ROOT/mcal/can -I$ROOT/ecual/canif -I$ROOT/bsw/det -I$ROOT/bsw/schm -I$ROOT/gen/LightEcu"
LD="-nostartfiles -T $ROOT/bringup/stage0_hello/stm32l552.ld -Wl,--gc-sections"
MCAL="$ROOT/mcal/can/Can.c $ROOT/mcal/can/Can_Hw_Stm32.c $ROOT/mcal/mcu/Mcu.c $ROOT/mcal/port/Port.c $ROOT/mcal/dio/Dio.c $ROOT/mcal/adc/Adc.c"
$GCC $CF "$HERE/common/startup_vec.c" "$HERE/probe/probe.c" $LD -o "$OUT/probe.elf"
$GCC $CF -DSMOKE_ROLE=0 $INC "$HERE/common/startup_vec.c" "$HERE/smoke/mcal_can_smoke.c" $MCAL $LD -Wl,-Map="$OUT/smoke_a.map" -o "$OUT/smoke_a.elf"
$GCC $CF -DSMOKE_ROLE=1 $INC "$HERE/common/startup_vec.c" "$HERE/smoke/mcal_can_smoke.c" $MCAL $LD -Wl,-Map="$OUT/smoke_b.map" -o "$OUT/smoke_b.elf"
echo "built probe.elf smoke_a.elf smoke_b.elf in $OUT"
