"""Fetch the chunks benchmark/run.py samples (same store, seed and filter) into bench-neon/chunks."""
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "benchmark"))
import run  # noqa: E402
import s3fs  # noqa: E402

STORE = "vesuvius-challenge-open-data/PHercParis4/volumes/20260411134726-2.400um-0.2m-78keV-masked.zarr"
fs = s3fs.S3FileSystem(anon=True)
meta = run.load_array_meta(fs, f"{STORE}/0")
out = Path(__file__).resolve().parent / "chunks"
run.fetch_level(fs, meta, f"{STORE}/0", out, 63, 0.05, 1234)
h = hashlib.sha256()
for f in sorted(out.glob("*.raw")):
    h.update(f.name.encode()), h.update(f.read_bytes())
print("chunks", len(list(out.glob("*.raw"))), "sha256", h.hexdigest()[:16])
