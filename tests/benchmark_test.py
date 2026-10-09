import json
import subprocess
import sys

binary = sys.argv[1]
for workload in ("wire-chain", "wire-grid", "gates", "screens", "pulsed-screens", "feedback"):
    result = subprocess.run([binary, "400", "20", workload], capture_output=True, text=True, check=True)
    report = json.loads(result.stdout)
    assert report["workload"] == workload and report["cells"] == 400
    assert report["topology_builds"] == 1
    assert report["cold_ms"] >= 0 and report["ms_per_step"] >= 0
    if workload in ("screens", "pulsed-screens", "feedback"):
        assert report["propagations"] == 21 and report["settled_ticks"] == 0
    else:
        assert report["settled_ticks"] > 0
for args in (("0",), ("200", "0"), ("200", "2", "unknown"), ("bad",)):
    assert subprocess.run([binary, *args], capture_output=True).returncode == 2
print("Benchmark reports and workload invariants passed")
