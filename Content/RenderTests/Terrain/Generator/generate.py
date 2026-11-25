"""Generates the render tests terrain: Content/RenderTests/Terrain/Source/{Heightmap.exr, MaterialIDs.dds}.

Smooth hills around the origin (Terrain and Grass tests look along +X from there):
grass (Grass, GrassType 0) everywhere, a checker band across the view (Checker) and tiled hilltops (Tiles).

Usage: python Content/RenderTests/Terrain/Generator/generate.py
"""
import struct
from pathlib import Path

import numpy as np

SOURCE_DIR = Path(__file__).resolve().parent.parent / "Source"

RES = 512                   # texels per side, multiple of 4
SPAN = 2048.0               # meters, renderer terrain span (-1024..1024)
MAX_HEIGHT = 1200.0         # meters, renderer maps height map [0, 1] to [0, 1200] m

GRASS, CHECKER, TILES = 0, 1, 2  # entries of TerrainMaterial.sptasset


def write_exr_y(path, data):
    """Single channel 'Y' float32 scanline EXR, no compression."""
    h, w = data.shape
    def attr(name, typ, payload):
        return name.encode() + b"\0" + typ.encode() + b"\0" + struct.pack("<i", len(payload)) + payload
    chlist = b"Y\0" + struct.pack("<iB3xii", 2, 0, 1, 1) + b"\0"
    header = b"".join([
        attr("channels", "chlist", chlist),
        attr("compression", "compression", b"\0"),
        attr("dataWindow", "box2i", struct.pack("<4i", 0, 0, w - 1, h - 1)),
        attr("displayWindow", "box2i", struct.pack("<4i", 0, 0, w - 1, h - 1)),
        attr("lineOrder", "lineOrder", b"\0"),
        attr("pixelAspectRatio", "float", struct.pack("<f", 1.0)),
        attr("screenWindowCenter", "v2f", struct.pack("<2f", 0.0, 0.0)),
        attr("screenWindowWidth", "float", struct.pack("<f", 1.0)),
    ]) + b"\0"
    prefix = struct.pack("<ii", 20000630, 2) + header
    line_size = 8 + w * 4
    data_start = len(prefix) + 8 * h
    offsets = struct.pack(f"<{h}Q", *[data_start + y * line_size for y in range(h)])
    rows = data.astype("<f4")
    with open(path, "wb") as f:
        f.write(prefix)
        f.write(offsets)
        for y in range(h):
            f.write(struct.pack("<ii", y, w * 4))
            f.write(rows[y].tobytes())


def write_dds_r8_uint(path, data):
    h, w = data.shape
    DXGI_FORMAT_R8_UINT = 62
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000  # caps, height, width, pixelformat, linearsize
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)
    header = struct.pack("<IIIIIII", 124, flags, h, w, w * h, 0, 1) + b"\0" * 44 + pixel_format + struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)
    dx10 = struct.pack("<IIIII", DXGI_FORMAT_R8_UINT, 3, 0, 1, 0)
    with open(path, "wb") as f:
        f.write(b"DDS " + header + dx10 + np.ascontiguousarray(data, dtype=np.uint8).tobytes())


def main():
    # Texel centers; row 0 is world -Y, column 0 is world -X
    coords = -SPAN * 0.5 + (np.arange(RES) + 0.5) * (SPAN / RES)
    x, y = np.meshgrid(coords, coords)

    h = 20.0 + 6.0 * np.sin(x / 70.0) * np.sin(y / 55.0 + 1.0)
    h += 45.0 * np.exp(-((x - 220.0) ** 2 + (y + 40.0) ** 2) / 90.0 ** 2)
    h += 30.0 * np.exp(-((x - 160.0) ** 2 + (y - 140.0) ** 2) / 70.0 ** 2)

    ids = np.full((RES, RES), GRASS, np.uint8)
    ids[(x > 48.0) & (x < 80.0)] = CHECKER
    ids[h > 40.0] = TILES

    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    write_exr_y(SOURCE_DIR / "Heightmap.exr", (h / MAX_HEIGHT).astype(np.float32))
    write_dds_r8_uint(SOURCE_DIR / "MaterialIDs.dds", ids)
    print(f"height at origin: {h[RES // 2, RES // 2]:.2f} m")


if __name__ == "__main__":
    main()
