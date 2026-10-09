"""Run each bounded workload in fresh processes and preserve raw JSON evidence."""
import argparse
import datetime
import json
import platform
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary")
parser.add_argument("--cells", type=int, default=10000)
parser.add_argument("--steps", type=int, default=30)
parser.add_argument("--runs", type=int, default=5)
parser.add_argument("--label", default="See build configuration for compiler and optimization flags")
args = parser.parse_args()
if not 1 <= args.runs <= 30 or not 2 <= args.cells <= 1000000 or not 1 <= args.steps <= 10000:
    parser.error("Runs must be 1..30, cells 2..1000000, and steps 1..10000")
results = {}
for workload in ("wire-chain", "wire-grid", "gates", "screens", "pulsed-screens", "feedback"):
    runs = []
    for _ in range(args.runs):
        output = subprocess.run([args.binary, str(args.cells), str(args.steps), workload],
                                capture_output=True, text=True, check=True, timeout=120)
        runs.append(json.loads(output.stdout))
    results[workload] = {
        "median_ms_per_step": statistics.median(run["ms_per_step"] for run in runs),
        "median_cold_ms": statistics.median(run["cold_ms"] for run in runs),
        "runs": runs,
    }
print(json.dumps({"format_version": 1, "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  "host": platform.platform(), "machine": platform.machine(), "label": args.label,
                  "results": results}, indent=2))
