"""ALTLOGO1 asset helper: locate in the mirror binary, decode, build, patch.

usage:
  patch_startup_logo_slot.py locate BINARY ASSET
  patch_startup_logo_slot.py frames ASSET OUTDIR IDX[,IDX...]
  patch_startup_logo_slot.py build FRAMES_DIR ASSET          (PNG frames 0000.png.. -> asset)
  patch_startup_logo_slot.py patch BINARY OLD_ASSET NEW_ASSET OUT_BINARY
"""
import struct
import sys
import zlib
from pathlib import Path

WIDTH, HEIGHT, FPS = 1440, 455, 30
FRAME_BYTES = WIDTH * HEIGHT * 4
MAGIC = b"ALTLOGO1"


def parse(blob):
    """Mirror StartupLogo::open/next_frame exactly; return raw RGBA frames."""
    assert blob[:8] == MAGIC, "bad magic"
    w, h, fps, count = struct.unpack_from("<IIII", blob, 8)
    assert (w, h, fps) == (WIDTH, HEIGHT, FPS) and 0 < count <= 300, (w, h, fps, count)
    pos, frames = 24, []
    for _ in range(count):
        (n,) = struct.unpack_from("<I", blob, pos)
        assert 0 < n <= FRAME_BYTES and pos + 4 + n <= len(blob)
        raw = zlib.decompress(blob[pos + 4:pos + 4 + n])
        assert len(raw) == FRAME_BYTES
        frames.append(raw)
        pos += 4 + n
    return frames, pos


def locate(binary, asset):
    b, a = Path(binary).read_bytes(), Path(asset).read_bytes()
    off = b.find(a)
    assert off >= 0 and b.find(a, off + 1) < 0, "asset not found exactly once"
    print(f"offset={off} length={len(a)} end={off + len(a)} binary={len(b)}")
    return off


def frames(asset, outdir, idx):
    from PIL import Image
    fr, used = parse(Path(asset).read_bytes())
    print(f"frames={len(fr)} parsed_bytes={used}")
    Path(outdir).mkdir(parents=True, exist_ok=True)
    for i in idx:
        Image.frombytes("RGBA", (WIDTH, HEIGHT), fr[i]).save(f"{outdir}/frame_{i:03d}.png")


def build(frames_dir, asset):
    from PIL import Image
    files = sorted(Path(frames_dir).glob("*.png"))
    assert 0 < len(files) <= 300
    out = bytearray(MAGIC + struct.pack("<IIII", WIDTH, HEIGHT, FPS, len(files)))
    for f in files:
        im = Image.open(f).convert("RGBA")
        assert im.size == (WIDTH, HEIGHT), f
        z = zlib.compress(im.tobytes(), 9)
        out += struct.pack("<I", len(z)) + z
    Path(asset).write_bytes(out)
    print(f"frames={len(files)} size={len(out)}")


def patch(binary, old_asset, new_asset, out_binary):
    b = bytearray(Path(binary).read_bytes())
    old, new = Path(old_asset).read_bytes(), Path(new_asset).read_bytes()
    off = locate(binary, old_asset)
    assert len(new) <= len(old), f"new asset {len(new)} > slot {len(old)}"
    parse(new)
    b[off:off + len(old)] = new + bytes(len(old) - len(new))
    assert len(b) == len(Path(binary).read_bytes())
    Path(out_binary).write_bytes(bytes(b))
    print(f"patched slot={len(old)} used={len(new)} pad={len(old) - len(new)}")


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "locate":
        locate(*sys.argv[2:4])
    elif cmd == "frames":
        frames(sys.argv[2], sys.argv[3], [int(x) for x in sys.argv[4].split(",")])
    elif cmd == "build":
        build(*sys.argv[2:4])
    elif cmd == "patch":
        patch(*sys.argv[2:6])
    else:
        sys.exit(__doc__)
