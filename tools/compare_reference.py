#!/usr/bin/env python3
"""Compare observable states with a separately built reference engine."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile
from audit_legacy import ELEMENTS

def states(executable, path, ticks, reference=False):
    result = subprocess.run([str(executable), "state", str(path), str(ticks)], check=True,
                            capture_output=True, text=True, encoding="utf-8", timeout=120)
    values = {}
    for row in csv.DictReader(io.StringIO(result.stdout)):
        point = (int(row["x"]), int(row["y"]))
        if point in values:
            raise ValueError("duplicate observed coordinate")
        element = ELEMENTS[int(row["element"])] if reference else row["element"]
        values[point] = (element, int(row["powered"]), int(row["conductive"]))
    return values

def compare(reference, candidate, path, ticks):
    expected, actual = states(reference, path, ticks, True), states(candidate, path, ticks)
    mismatches = sorted(p for p in expected.keys() | actual.keys() if expected.get(p) != actual.get(p))
    payload = json.dumps(sorted((x, y, *value) for (x, y), value in expected.items()), separators=(",", ":")).encode()
    return {"sample": path.name, "ticks": ticks, "cells": len(expected), "mismatches": len(mismatches),
            "reference_state_sha256": hashlib.sha256(payload).hexdigest(),
            "first_mismatches": [{"point": p, "reference": expected.get(p), "candidate": actual.get(p)} for p in mismatches[:5]]}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--ticks", nargs="+", type=int, default=[0, 1, 2, 10, 100])
    parser.add_argument("--seeded", type=int, default=100)
    args = parser.parse_args()
    if args.seeded < 0 or args.seeded > 10000 or any(t < 0 or t > 1000000 for t in args.ticks):
        parser.error("invalid trial or tick budget")
    reference, candidate = args.reference.resolve(), args.candidate.resolve()
    paths = sorted(args.corpus.glob("*.ccsb"))
    if not paths:
        parser.error("corpus contains no .ccsb files")
    records = [compare(reference, candidate, p.resolve(), t) for p in paths for t in args.ticks]
    with tempfile.TemporaryDirectory(prefix="gatehaven-seeded-") as directory:
        rng = random.Random(20261009)
        for trial in range(args.seeded):
            path = Path(directory) / f"seeded-{trial:04}.ccsb"
            width, height = rng.randrange(1, 18), rng.randrange(1, 14)
            payload = bytes((rng.randrange(14) << 2) | rng.randrange(4) for _ in range(width * height))
            path.write_bytes(struct.pack("<4siii", b"CCPG", 0, width, height) + payload)
            for tick in (0, 1, 2, 7):
                records.append(compare(reference, candidate, path, tick))
    report = {"schema": 1, "seed": 20261009, "observations": records,
              "passed": all(row["mismatches"] == 0 for row in records),
              "limits": ["No user-driven screen input", "No attached file streams", "Boolean crossing power only; axis isolation has separate core tests"]}
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
