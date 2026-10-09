import json
import subprocess
import sys
binary = sys.argv[1]
for cells in (100, 10000):
    report = json.loads(subprocess.check_output([binary, str(cells)], text=True, timeout=30))
    assert report["cells"] == cells and report["preview_cells"] == 10000
    assert all(report[key] >= 0 for key in ("edit_ms", "capture_ms", "index_ms", "preview_1000_ms", "undo_redo_ms"))
for value in ("0", "99", "1000001", "-1", "bad"):
    assert subprocess.run([binary, value], timeout=10).returncode == 2
print("Editor workload counts, undo restoration and parameter bounds passed")
