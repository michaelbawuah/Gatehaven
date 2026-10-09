#!/usr/bin/env python3
"""Alternate fresh-process measurements of both engines on the same corpus."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from compare_reference import compare

def profile(binary, sample, ticks):
    return json.loads(subprocess.run([str(binary), "profile", str(sample), str(ticks)],
                                    capture_output=True, text=True, check=True, timeout=120).stdout)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--ticks", type=int, default=10000)
    parser.add_argument("--require-no-regression", action="store_true")
    args = parser.parse_args()
    if not 3 <= args.runs <= 30 or not 100 <= args.ticks <= 1000000:
        parser.error("use 3..30 runs and 100..1000000 ticks")
    reference, candidate = args.reference.resolve(), args.candidate.resolve()
    samples = sorted(args.corpus.glob("*.ccsb"))
    if not samples: parser.error("corpus contains no .ccsb files")
    records, regressions = [], []
    for sample in samples:
        sample = sample.resolve()
        correctness = compare(reference, candidate, sample, args.ticks)
        if correctness["mismatches"]: raise RuntimeError(f"state mismatch for {sample.name}")
        pairs = []
        for iteration in range(args.runs):
            binaries = [("reference", reference), ("candidate", candidate)]
            if iteration % 2: binaries.reverse()
            pairs.append({label: profile(binary, sample, args.ticks) for label, binary in binaries})
        medians = {phase: {label: statistics.median(run[label][phase] for run in pairs)
                           for label in ("reference", "candidate")} for phase in ("compile_ms", "steps_ms")}
        for phase, values in medians.items():
            if values["candidate"] > values["reference"]: regressions.append({"sample": sample.name, "phase": phase})
        records.append({"sample": sample.name, "sha256": hashlib.sha256(sample.read_bytes()).hexdigest(),
                        "correctness": correctness, "medians": medians, "runs": pairs})
    metadata = subprocess.run([str(candidate), "--build-info"], check=True, capture_output=True, text=True).stdout
    print(json.dumps({"schema": 1, "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      "host": platform.platform(), "machine": platform.machine(), "candidate_toolchain": metadata,
                      "reference_revision": "19c00d0bd794c3dd558d501939e55f186f58c9e6", "reference_flags": "C++17 -O3 -DNDEBUG -pthread",
                      "ticks": args.ticks, "samples": records, "median_regressions": regressions,
                      "limits": ["Shared host; timing noise is possible", "Headless simulation only", "No attached file streams or rendering"]}, indent=2))
    return 1 if args.require_no_regression and regressions else 0
if __name__ == "__main__": raise SystemExit(main())
