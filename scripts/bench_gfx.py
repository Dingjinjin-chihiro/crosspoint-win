#!/usr/bin/env python3
"""Summarize a Crosspoint emulator serial log for performance work.

The benchmark is "run a fixed CROSSPOINT_TOUCH_SCRIPT, capture stdout". This file
only reads that result, so a run is reproducible without touching the emulator:

    python scripts/bench_gfx.py build/emu-bench.log

Prints:
  * the [GFX] render-time series and its statistics,
  * the largest pauses between consecutive log lines (where time actually went),
  * EPUB-load and activity-transition deltas.
"""
import sys
import re

LINE = re.compile(r"^\[(\d+)\] \[(?:DBG|INF|ERR|WARN)\] \[([A-Z0-9]+)\] (.*)$")
GFX = re.compile(r"\[(\d+)\] \[DBG\] \[GFX\] Time = (\d+) ms")


def main(path):
    gfx = []
    rows = []
    for raw in open(path, encoding="utf-8", errors="replace"):
        raw = raw.rstrip("\n")
        m = LINE.match(raw)
        if not m:
            continue
        ts, mod, msg = int(m.group(1)), m.group(2), m.group(3)
        rows.append((ts, mod, msg))
        gm = GFX.match(raw)
        if gm:
            gfx.append(int(gm.group(2)))

    print("== GFX render time (ms) ==")
    if gfx:
        s = sorted(gfx)
        n = len(s)

        def pct(q):
            return s[min(n - 1, int(n * q))]
        print(f"  n={n} min={s[0]} max={s[-1]} mean={sum(s)/n:.2f} "
              f"p50={pct(.5)} p95={pct(.95)} p99={pct(.99)}")
        print(f"  series: {gfx}")
    else:
        print("  (none)")

    print("\n== largest pauses between consecutive log lines (>20 ms) ==")
    gaps = []
    for (t0, m0, x0), (t1, m1, x1) in zip(rows, rows[1:]):
        d = t1 - t0
        if d > 20:
            gaps.append((d, t0, m0, x0, t1, m1, x1))
    gaps.sort(reverse=True)
    if not gaps:
        print("  (none)")
    for d, t0, m0, x0, t1, m1, x1 in gaps[:15]:
        print(f"  {d:6d} ms  [{t0:6d}] [{m0}] {x0[:72]}")
        print(f"          -> [{t1:6d}] [{m1}] {x1[:72]}")

    print("\n== EPUB / activity transitions (>10 ms) ==")
    seen = 0
    for (t0, m0, x0), (t1, m1, x1) in zip(rows, rows[1:]):
        d = t1 - t0
        if m0 in ("ACT", "EBP") and d > 10:
            print(f"  +{d:5d} ms  [{t0:6d}] [{m0}] {x0[:72]}")
            seen += 1
    if not seen:
        print("  (none)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1])
