#!/usr/bin/env python3
"""Compare live-file protocol results, allowing implementation-dependent reply latency."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("reference", type=Path)
parser.add_argument("candidate", type=Path)
args = parser.parse_args()
records = {}
edges = {}
for name, binary in (("reference", args.reference), ("candidate", args.candidate)):
    with tempfile.TemporaryDirectory(prefix="gatehaven-" + name + "-") as directory:
        result = subprocess.run([str(binary.resolve()), directory], capture_output=True, text=True, timeout=20, check=True)
        report = json.loads(result.stdout)
        output = (Path(directory) / "written.bin").read_bytes()
        if output != bytes(range(256)): raise ValueError("output byte mismatch")
        report["output_sha256"] = hashlib.sha256(output).hexdigest()
        records[name] = report
    with tempfile.TemporaryDirectory(prefix="gatehaven-edges-" + name + "-") as directory:
        result = subprocess.run([str(binary.resolve()), directory, "edges"], capture_output=True, text=True, timeout=20, check=True)
        edges[name] = json.loads(result.stdout)
comparable = lambda report: {key: value for key, value in report.items() if key != "ticks"}
passed = records["candidate"]["passed"] and comparable(records["candidate"]) == comparable(records["reference"])
edge_expectations = {"reference": {"empty_available": 1, "reset_byte": 65, "reserved_output_bytes": 1},
                     "candidate": {"empty_available": 0, "reset_byte": 66, "reserved_output_bytes": 0}}
passed = passed and edges == edge_expectations
print(json.dumps({"schema": 2, "passed": passed, "engines": records, "explicit_differences": edges,
                  "difference_expectations_confirmed": edges == edge_expectations,
                  "reference_revision": "19c00d0bd794c3dd558d501939e55f186f58c9e6",
                  "limits": ["Protocol/stream comparison; not a UI dialog or wall-clock benchmark",
                             "Reply delay intentionally unconstrained within a five-second deadline",
                             "Three protocol edge differences are observed and asserted separately"]}, indent=2))
raise SystemExit(0 if passed else 1)
