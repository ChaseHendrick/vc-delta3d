#!/usr/bin/env bash
# Builds the variant harness and times it (bench-neon/main), then vc-delta3d's own benchmark for ctx and lpc.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
uname -a; (sysctl -n machdep.cpu.brand_string 2>/dev/null || lscpu | head -20) ; cc --version | head -1
D3D="$HERE/.." bash "$HERE/build.sh" "$HERE/out" base v1 v2 v3
for r in 1 2 3; do "$HERE/out/main" "$HERE/chunks" 9; done
