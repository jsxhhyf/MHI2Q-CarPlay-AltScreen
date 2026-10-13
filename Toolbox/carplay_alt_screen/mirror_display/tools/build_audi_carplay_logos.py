"""Render the logos-only startup splash (Audi rings + CarPlay icon, no text)
as 1440x455 RGBA frames.

usage: build_audi_carplay_logos.py carplay_icon_124.png OUTDIR
       then patch_startup_logo_slot.py build OUTDIR logo_frames.z

The CarPlay icon was cropped from the upstream intro's final frame, so it stays
pixel-identical to what the project shipped.  The firmware draws this canvas at
70% scale, centred, then fades it into CarPlay over 550 ms.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

W, H = 1440, 455
CROP_BG = np.array([7, 11, 15], dtype=np.float32)  # upstream intro background
FRAMES = 75                                         # 2.5 s at 30 fps
SS = 4                                              # supersampling for rings

RING_D = 112
RING_STEP = RING_D * 0.705
GAP = 80                                            # rings -> icon


def ring_mask(cx, cy, d):
    """One flat Audi ring centred at (cx, cy), outer diameter d."""
    big = Image.new("L", (W * SS, H * SS), 0)
    r = d / 2 * SS
    ImageDraw.Draw(big).ellipse((cx * SS - r, cy * SS - r, cx * SS + r, cy * SS + r),
                                outline=255, width=int(round(d * 0.105 * SS)))
    return np.asarray(big.resize((W, H), Image.LANCZOS), dtype=np.float32) / 255.0


def build_layers(icon):
    """-> list of (start_frame, fade_frames, premultiplied rgb, alpha or None)."""
    rings_w = 3 * RING_STEP + RING_D
    x0 = (W - (rings_w + GAP + icon.size[0])) / 2
    cy = H / 2

    # Rings: silver vertical gradient, staggered reveal.
    grad = np.linspace(1.0, 0.0, H, dtype=np.float32)[:, None, None]
    silver = (np.array([236, 238, 240], np.float32) * grad +
              np.array([150, 156, 163], np.float32) * (1 - grad))
    layers = []
    for i in range(4):
        m = ring_mask(x0 + RING_D / 2 + i * RING_STEP, cy, RING_D)
        layers.append((3 * i, 10, silver * m[..., None], m))

    # CarPlay icon: remove the background it was cropped on, add its own colours.
    ix = int(round(x0 + rings_w + GAP))
    iy = int(round(cy - icon.size[1] / 2))
    rgb = np.zeros((H, W, 3), np.float32)
    rgb[iy:iy + icon.size[1], ix:ix + icon.size[0]] = np.clip(
        np.asarray(icon.convert("RGB"), np.float32) - CROP_BG, 0, None)
    layers.append((14, 12, rgb, None))
    return layers


def ramp(f, start, length):
    t = min(max((f - start) / float(length), 0.0), 1.0)
    return t * t * (3 - 2 * t)                           # smoothstep


def render(layers, f):
    out = np.zeros((H, W, 3), np.float32)                # pure black background
    for start, length, rgb, alpha in layers:
        k = ramp(f, start, length)
        if alpha is None:
            out += rgb * k
        else:
            out = out * (1 - alpha[..., None] * k) + rgb * k
    rgba = np.empty((H, W, 4), np.uint8)
    rgba[..., :3] = np.clip(np.rint(out), 0, 255)
    rgba[..., 3] = 255
    return Image.fromarray(rgba, "RGBA")


def main():
    icon = Image.open(sys.argv[1]).convert("RGB")
    outdir = Path(sys.argv[2])
    outdir.mkdir(parents=True, exist_ok=True)
    layers = build_layers(icon)
    for f in range(FRAMES):
        render(layers, f).save(outdir / f"{f:04d}.png")
    print(f"frames={FRAMES}")


if __name__ == "__main__":
    main()
