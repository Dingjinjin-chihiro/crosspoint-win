#!/usr/bin/env python3
"""Generate a large EPUB for pagination benchmarks (a derived artifact, not committed).

    python scripts/make_bench_book.py [out.epub] [chapters] [paragraphs-per-chapter]

Defaults to sdcard/Benchmark Book.epub with 5 chapters x 40 paragraphs, enough
pages for a page-turn benchmark that the 3-chapter sample book cannot provide.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_sample_epub import build_epub  # noqa: E402


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join("sdcard", "Benchmark Book.epub")
    chapters = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    paras = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    sentence = ("The quick brown fox jumps over the lazy dog while the quiet shelf holds a "
                "hundred unread stories, each one waiting for a reader and a lamp. ")
    chs = [(f"Benchmark Chapter {c}", [sentence * (1 + (i % 4)) for i in range(paras)])
           for c in range(1, chapters + 1)]
    build_epub(out, "Benchmark Book", "Bench Author", chs,
               uuid="urn:uuid:11111111-2222-4333-8444-555555555555")
    print(f"wrote {out} ({chapters} chapters x {paras} paragraphs)")


if __name__ == "__main__":
    main()
