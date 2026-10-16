import numcodecs
import numpy as np

from vc_delta3d import (
    Delta3D,
    decompress_into_with_magic,
    decompress_with_magic,
)


def test_roundtrip_and_registration():
    source = np.arange(4 * 5 * 6, dtype=np.uint16).reshape(4, 5, 6)

    codec = Delta3D()
    encoded = codec.encode(source)
    assert encoded[:4] == b"D3D1"
    decoded = np.frombuffer(codec.decode(encoded), dtype=np.uint16)
    np.testing.assert_array_equal(decoded.reshape(source.shape), source)
    assert numcodecs.get_codec(codec.get_config()).codec_id == codec.codec_id


def test_decode_into():
    source = np.arange(3 * 4 * 5, dtype=np.uint8).reshape(3, 4, 5)
    codec = Delta3D()
    output = np.empty_like(source)
    assert codec.decode(codec.encode(source), out=output) is output
    np.testing.assert_array_equal(output, source)


def test_alternate_wire_magic():
    source = np.arange(3 * 4 * 5, dtype=np.uint8).reshape(3, 4, 5)
    encoded = Delta3D().encode(source)
    alternate = b"ALT1" + encoded[4:]

    decoded = decompress_with_magic(alternate, source.nbytes, b"ALT1")
    np.testing.assert_array_equal(
        np.frombuffer(decoded, dtype=np.uint8).reshape(source.shape), source
    )

    output = np.empty_like(source)
    decompress_into_with_magic(alternate, output.view(np.uint8), b"ALT1")
    np.testing.assert_array_equal(output, source)


def test_concurrent_roundtrip():
    from concurrent.futures import ThreadPoolExecutor

    rng = np.random.default_rng(0)
    chunks = [
        rng.integers(0, 4096, size=(16, 32, 32), dtype=np.uint16)
        for _ in range(32)
    ]
    codec = Delta3D()

    def roundtrip(chunk):
        decoded = np.frombuffer(codec.decode(codec.encode(chunk)), np.uint16)
        return np.array_equal(decoded.reshape(chunk.shape), chunk)

    with ThreadPoolExecutor(max_workers=8) as pool:
        assert all(pool.map(roundtrip, chunks))
