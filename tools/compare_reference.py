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

def states(executable, path, ticks, reference=False, screens=False):
    result = subprocess.run([str(executable), "screen-state" if reference and screens else "state", str(path), str(ticks)], check=True,
                            capture_output=True, text=True, encoding="utf-8", timeout=120)
    values = {}
    for row in csv.DictReader(io.StringIO(result.stdout)):
        point = (int(row["x"]), int(row["y"]))
        if point in values:
            raise ValueError("duplicate observed coordinate")
        element = ELEMENTS[int(row["element"])] if reference else row["element"]
        values[point] = (element, int(row["powered"]), int(row["conductive"]))
    return values

def compare(reference, candidate, path, ticks, screens=False):
    expected, actual = states(reference, path, ticks, True, screens), states(candidate, path, ticks)
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
    parser.add_argument("--screens", action="store_true", help="drive deterministic screen holds; candidate must be gatehaven_screen_peer")
    args = parser.parse_args()
    if args.seeded < 0 or args.seeded > 10000 or any(t < 0 or t > 1000000 for t in args.ticks):
        parser.error("invalid trial or tick budget")
    reference, candidate = args.reference.resolve(), args.candidate.resolve()
    paths = sorted(args.corpus.glob("*.ccsb"))
    if not paths:
        parser.error("corpus contains no .ccsb files")
    records = [compare(reference, candidate, p.resolve(), t, args.screens) for p in paths for t in args.ticks]
    with tempfile.TemporaryDirectory(prefix="gatehaven-seeded-") as directory:
        rng = random.Random(20261009)
        for trial in range(args.seeded):
            path = Path(directory) / f"seeded-{trial:04}.ccsb"
            width, height = rng.randrange(1, 18), rng.randrange(1, 14)
            payload = bytearray((rng.randrange(14) << 2) | rng.randrange(4) for _ in range(width * height))
            # Exclude the one explicitly documented spec/reference conflict.
            for index, byte in enumerate(payload):
                if byte >> 2 != 3: continue
                x, y = index % width, index // width
                adjacent = [index + delta for delta, valid in ((-1, x > 0), (1, x + 1 < width), (-width, y > 0), (width, y + 1 < height)) if valid]
                if any(payload[next_] >> 2 == 4 for next_ in adjacent): payload[index] = (1 << 2) | (byte & 3)
            payload = bytes(payload)
            path.write_bytes(struct.pack("<4siii", b"CCPG", 0, width, height) + payload)
            for tick in (0, 1, 2, 7):
                records.append(compare(reference, candidate, path, tick, args.screens))
        conflict = Path(directory) / "source-signal-spec-difference.ccsb"
        conflict.write_bytes(struct.pack("<4siii", b"CCPG", 0, 2, 1) + bytes([16, 12]))
        divergence = compare(reference, candidate, conflict, 0, args.screens)
        known_difference = divergence["mismatches"] == 1 and divergence["first_mismatches"] == [
            {"point": (1, 0), "reference": ("signal", 1, 0), "candidate": ("signal", 0, 0)}]
    report = {"schema": 2, "seed": 20261009, "observations": records, "spec_difference": divergence,
              "spec_difference_confirmed": known_difference,
              "passed": known_difference and all(row["mismatches"] == 0 for row in records),
              "limits": ["Matching seeded fixtures exclude direct Source/Signal adjacency; the difference is asserted separately", "Deterministic per-tick screen holds" if args.screens else "No user-driven screen input", "No attached file streams", "Boolean crossing power only; axis isolation has separate core tests"]}
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
