#!/usr/bin/env python3
"""Off-screen circuit growth must not increase the number of rendered cells."""
import json
import os
import subprocess
import sys

reports = []
for extra in (0, 20000):
    run = subprocess.run([sys.argv[1], "--benchmark-render", str(extra), "10"],
                         env={**os.environ, "SDL_VIDEODRIVER": "dummy"},
                         capture_output=True, text=True, check=True, timeout=40)
    report = json.loads(run.stdout)
    assert report["total_cells"] == 3072 + extra
    assert report["frames"] == 10 and report["renderer"] == "software"
    for scenario in report["scenarios"]:
        assert len(scenario["frame_ms"]) == 10
        assert 0 < scenario["visible_min"] <= scenario["visible_max"] <= 3072
        assert 0 <= scenario["median_ms"] <= scenario["p95_ms"] <= scenario["max_ms"]
    reports.append(report)
assert [(r["visible_min"], r["visible_max"]) for r in reports[0]["scenarios"]] == [(r["visible_min"], r["visible_max"]) for r in reports[1]["scenarios"]]
for args in (("-1",), ("1", "0"), ("1000001",), ("0", "10bad")):
    assert subprocess.run([sys.argv[1], "--benchmark-render", *args], capture_output=True).returncode == 2
print("Render culling and benchmark report checks passed")
