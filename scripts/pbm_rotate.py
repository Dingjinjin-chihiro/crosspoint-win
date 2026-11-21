import sys
sys.path.insert(0, "scripts")
from pbm_info import read_pbm, bit, write_png
src, out = sys.argv[1], sys.argv[2]
w, h, rows = read_pbm(src)
dst = [bytearray((h + 7) // 8) for _ in range(w)]
for y in range(h):
    for x in range(w):
        if bit(rows, x, y):
            rx = h - 1 - y
            ry = x
            dst[ry][rx // 8] |= 0x80 >> (rx % 8)
write_png(out, h, w, dst)
print("wrote", out)
