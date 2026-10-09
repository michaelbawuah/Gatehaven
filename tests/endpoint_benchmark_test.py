import json
import subprocess
import sys
for mode in ([], ["uncached"]):
    report = json.loads(subprocess.run([sys.argv[1], "100", "257", "2", *mode],
                        check=True, capture_output=True, text=True, timeout=20).stdout)
    assert report["verified_bytes"] and len(report["runs_ms"]) == 2
    assert report["cached"] == (not mode)
for args in (("0", "100", "1"), ("1", "0", "1"), ("x", "1", "1")):
    assert subprocess.run([sys.argv[1], *args], capture_output=True).returncode == 2
print("Live I/O benchmark verifies bytes and ordered acknowledgements in both routing paths")
