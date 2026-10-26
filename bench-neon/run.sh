#!/usr/bin/env bash
# Times the PR head's header (pr) against the old one (base), then vc-delta3d's own
# benchmark (benchmark/run.py) for lpc and ctx on the PR head's code.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
uname -a; (sysctl -n machdep.cpu.brand_string 2>/dev/null || lscpu | grep 'Model name') ; cc --version | head -1
cp "$HERE/../src/volcodec/volcodec_ng_lossless.h" "$HERE/pr.h"
D3D="$HERE/.." bash "$HERE/build.sh" "$HERE/out" base pr
for r in 1 2 3; do "$HERE/out/main" "$HERE/chunks" 9; done
S3=s3://vesuvius-challenge-open-data/PHercParis4/volumes/20260411134726-2.400um-0.2m-78keV-masked.zarr
for r in 1 2 3; do for p in lpc ctx; do echo "== run.py $p round $r"; python benchmark/run.py $S3 --chunks 63 --levels 1 --predictor $p --reps 5 | tail -4; done; done
