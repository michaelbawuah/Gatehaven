"""Check the installed command interface using actual files and parsed outputs."""
import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile

exe = sys.argv[1]

def run(*args, code=0):
    result = subprocess.run([exe, *map(str, args)], text=True, capture_output=True, timeout=15)
    assert result.returncode == code, (args, result.stdout, result.stderr)
    return result.stdout

with tempfile.TemporaryDirectory(prefix="gatehaven cli ") as directory:
    path = Path(directory) / "starter circuit.ghv"
    run("example", path)
    assert "49 cells" in run("check", path)
    stats = json.loads(run("stats", path))
    assert stats["cells"] == 49 and stats["elements"]["source"] == 3
    rows = list(csv.DictReader(io.StringIO(run("trace", path, 2))))
    output = [r for r in rows if r["x"] == "8" and r["y"] == "0"]
    assert [r["ports"] for r in output] == ["0", "15"]
    assert len(rows) == 98
    assert list(csv.DictReader(io.StringIO(run("trace", path, 0)))) == []
    run("run", path, "-1", code=2)
    run("run", path, "1000001", code=2)
    run("check", Path(directory) / "missing", code=1)
print("CLI: sample creation, statistics, traces and failure exit codes passed")
