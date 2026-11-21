"""Inspect a PBM (P4) panel framebuffer: report ink extents and optionally write a PNG."""

import struct
import sys
import zlib


def read_pbm(path):
    data = open(path, "rb").read()
    # Parse P4 header: magic, width, height, single whitespace
    assert data[:2] == b"P4", "not a P4 PBM"
    idx = 2
    vals = []
    while len(vals) < 2:
        while data[idx : idx + 1].isspace():
            idx += 1
        if data[idx : idx + 1] == b"#":
            while data[idx : idx + 1] not in (b"\n", b""):
                idx += 1
            continue
        start = idx
        while not data[idx : idx + 1].isspace():
            idx += 1
        vals.append(int(data[start:idx]))
    idx += 1
    width, height = vals
    stride = (width + 7) // 8
    rows = []
    for y in range(height):
        base = idx + y * stride
        rows.append(data[base : base + stride])
    return width, height, rows


def bit(rows, x, y):
    return (rows[y][x // 8] >> (7 - (x % 8))) & 1


def write_png(path, width, height, rows):
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0
        for x in range(width):
            v = 255 if bit(rows, x, y) else 0  # e-ink: bit 1 = white, 0 = black ink
            raw.extend((v, v, v))

    def chunk(tag, payload):
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


if __name__ == "__main__":
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else None
    w, h, rows = read_pbm(src)
    minx, maxx, miny, maxy = w, -1, h, -1
    per_col = [0] * w
    for y in range(h):
        for x in range(w):
            if bit(rows, x, y):
                per_col[x] += 1
                minx = min(minx, x)
                maxx = max(maxx, x)
                miny = min(miny, y)
                maxy = max(maxy, y)
    print(f"panel: {w}x{h}")
    print(f"ink bbox: x={minx}..{maxx}  y={miny}..{maxy}")
    print("rightmost 12 columns ink:", per_col[-12:])
    print("leftmost 12 columns ink:", per_col[:12])
    if out:
        write_png(out, w, h, rows)
        print("wrote", out)
