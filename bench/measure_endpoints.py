#!/usr/bin/env python3
"""Alternate cached and uncached live endpoint measurements on the same host."""
import argparse
import datetime
import json
from pathlib import Path
import platform
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("--runs", type=int, default=5)
parser.add_argument("--cells", type=int, default=10000)
parser.add_argument("--bytes", type=int, default=512)
args = parser.parse_args()
if not 3 <= args.runs <= 20 or not 1 <= args.cells <= 100000 or not 1 <= args.bytes <= 65536:
    parser.error("invalid measurement budget")
pairs = []
revision = None
for iteration in range(args.runs):
    order = ("cached", "uncached") if iteration % 2 == 0 else ("uncached", "cached")
    row = {}
    for mode in order:
        result = subprocess.run([str(args.binary.resolve()), str(args.cells), str(args.bytes), "1",
                                 *(["uncached"] if mode == "uncached" else [])],
                                check=True, capture_output=True, text=True, timeout=120)
        report = json.loads(result.stdout)
        if not report["verified_bytes"]: raise ValueError("unverified byte stream")
        if revision is not None and revision != report["source_revision"]: raise ValueError("binary changed during measurement")
        revision = report["source_revision"]
        row[mode] = report["runs_ms"][0]
    pairs.append(row)
medians = {mode: statistics.median(row[mode] for row in pairs) for mode in ("cached", "uncached")}
print(json.dumps({"schema": 1, "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  "host": platform.platform(), "source_revision": revision, "group_cells": args.cells,
                  "input_bytes_per_run": args.bytes, "output_bytes_per_run": args.bytes,
                  "medians_ms": medians, "runs": pairs,
                  "limits": ["Temporary regular files on this host; storage speed and scheduling vary",
                             "Compares the same endpoint engine with and without routing cache",
                             "Not an external-engine benchmark or a disk durability guarantee"]}, indent=2))
