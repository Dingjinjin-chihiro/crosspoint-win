#!/usr/bin/env python3
"""Analyze a -finstrument-functions trace produced by the Crosspoint emulator.

    python scripts/trace_analyze.py trace.log build-trace/crosspoint_emulator.exe
        [--from MS] [--to MS] [--top N] [--tree] [--tree-lines N] [--dump out.txt]

Resolves every traced address to "function (file:line)" via addr2line, then
prints call counts, per-function self time, and (with --tree) an indented call
tree for the selected time window. --dump writes the resolved flat timeline.
"""
import argparse
import collections
import os
import subprocess
import sys

ADDR2LINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                         "tools", "msys64", "mingw64", "bin", "addr2line.exe")
NM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                  "tools", "msys64", "mingw64", "bin", "nm.exe")


def load_delta(trace_path, exe):
    """The trace header records the runtime address of sim_trace_anchor; its
    file address (from nm) is the preferred-base address. Their difference is
    the ASLR load delta every traced address must be adjusted by."""
    anchor_runtime = None
    with open(trace_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith("# anchor 0x"):
                anchor_runtime = int(line.split("0x")[1], 16)
                break
    if anchor_runtime is None:
        return 0
    out = subprocess.run([NM, "-C", os.path.abspath(exe)],
                         capture_output=True).stdout.decode("utf-8", "replace")
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and "sim_trace_anchor" in parts[-1]:
            anchor_file = int(parts[0], 16)
            return anchor_runtime - anchor_file
    return 0


def read_events(path, t0, t1):
    events = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 3:
                continue
            kind, ms, addr = parts[0], int(parts[1]), int(parts[2], 16)
            if t0 is not None and ms < t0:
                continue
            if t1 is not None and ms > t1:
                continue
            events.append((ms, kind, addr))
    return events


def resolve(addrs, exe):
    names = {}
    if not addrs:
        return names
    proc = subprocess.run(
        [ADDR2LINE, "-f", "-C", "-e", os.path.abspath(exe)],
        input="".join(f"0x{a:x}\n" for a in addrs).encode(),
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    lines = proc.stdout.decode("utf-8", "replace").splitlines()
    for i, a in enumerate(addrs):
        if 2 * i + 1 < len(lines):
            func = lines[2 * i].strip()
            loc = lines[2 * i + 1].strip()
            if func and func != "??":
                name = f"{func} ({loc})" if loc not in ("??:0", "??") else func
            else:
                name = f"0x{a:x}"
            names[a] = name
    return names


def clip(s, n=96):
    return s if len(s) <= n else s[:n - 3] + "..."


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("exe")
    ap.add_argument("--from", dest="t0", type=int, default=None)
    ap.add_argument("--to", dest="t1", type=int, default=None)
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--tree", action="store_true")
    ap.add_argument("--tree-lines", type=int, default=3000)
    ap.add_argument("--dump")
    args = ap.parse_args()

    events = read_events(args.trace, args.t0, args.t1)
    if not events:
        print("no trace records in that window")
        sys.exit(1)

    delta = load_delta(args.trace, args.exe)
    if delta:
        print(f"aslr delta=+{delta:#x} (runtime addresses adjusted before resolution)")
    addrs = sorted({a - delta for _, _, a in events})
    names = resolve(addrs, args.exe)
    by_rt = {a: names[a - delta] for a in {e[2] for e in events}}
    print(f"records={len(events)} unique_functions={len(addrs)} "
          f"window={events[0][0]}..{events[-1][0]}ms")

    counts = collections.Counter(a for _, k, a in events if k == "E")
    print(f"\n== top {args.top} by call count ==")
    for a, n in counts.most_common(args.top):
        print(f"  {n:8d}  {clip(by_rt.get(a, hex(a)))}")

    # self time: stack-based enter/exit matching (recursion-safe).
    stack = []  # (addr, enter_ms)
    total = collections.defaultdict(int)
    self_time = collections.defaultdict(int)
    for ms, kind, addr in events:
        if kind == "E":
            stack.append((addr, ms))
        else:
            hit = -1
            for i in range(len(stack) - 1, -1, -1):
                if stack[i][0] == addr:
                    hit = i
                    break
            if hit < 0:
                continue
            enter_ms = stack[hit][1]
            dur = ms - enter_ms
            total[addr] += dur
            self_time[addr] += dur
            if hit > 0:
                self_time[stack[hit - 1][0]] -= dur
            del stack[hit:]

    print(f"\n== top {args.top} by self time ==")
    for a, t in sorted(self_time.items(), key=lambda kv: -kv[1])[:args.top]:
        print(f"  {t:8d} ms  (total {total[a]:8d} ms)  {clip(by_rt.get(a, hex(a)))}")

    if args.tree:
        print(f"\n== call tree (first {args.tree_lines} events) ==")
        depth = 0
        shown = 0
        for ms, kind, addr in events:
            if kind == "X":
                depth = max(0, depth - 1)
                continue
            if shown >= args.tree_lines:
                print(f"  ... ({len(events) - shown} more)")
                break
            print(f"  {ms:7d}  {'  ' * depth}{clip(by_rt.get(addr, hex(addr)))}")
            depth += 1
            shown += 1

    if args.dump:
        with open(args.dump, "w", encoding="utf-8") as f:
            depth = 0
            for ms, kind, addr in events:
                name = by_rt.get(addr, hex(addr))
                if kind == "X":
                    depth = max(0, depth - 1)
                f.write(f"{ms}\t{kind}\t{'  ' * depth}{name}\n")
                if kind == "E":
                    depth += 1
        print(f"\nresolved timeline dumped to {args.dump}")


if __name__ == "__main__":
    main()
