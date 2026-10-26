#!/usr/bin/env bash
# build.sh OUT VARIANT...   (each VARIANT names VARIANT.h next to this script)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
D3D="${D3D:-$HERE/vc-delta3d}"
CC="${CC:-cc}" CXX="${CXX:-c++}"
OUT="$1"; shift
mkdir -p "$OUT"
{ printf '#define VARIANTS'; for v in "$@"; do printf ' V(%s)' "$v"; done; printf '\n'; } > "$OUT/variants.inc"
objs=()
for v in "$@"; do
  $CC -std=gnu11 -O3 -g0 -w ${CFLAGS_EXTRA:-} -DPFX=$v -DHDR="\"$HERE/$v.h\"" -c "$HERE/tu.c" -o "$OUT/$v.o"
  objs+=("$OUT/$v.o")
done
$CC -std=gnu11 -O3 -g0 -w ${CFLAGS_EXTRA:-} -c "$D3D/src/volcodec/llctx.c" -o "$OUT/llctx.o"
$CXX -std=c++20 -O3 -g0 ${CFLAGS_EXTRA:-} -I"$D3D/include" -I"$OUT" "$HERE/main.cpp" "$D3D/src/codec.cpp" "${objs[@]}" "$OUT/llctx.o" -lm -pthread -o "$OUT/main"
