#!/usr/bin/env python3
"""Drive live streams in an isolated directory with a bounded peer deadline."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="gatehaven-active-") as directory:
    result = subprocess.run([str(Path(sys.argv[1]).resolve()), directory], check=True,
                            capture_output=True, text=True, timeout=20)
    report = json.loads(result.stdout)
    assert report["passed"] and report["ordered_reload"]
    assert report["input_bytes"] == 257 and report["output_bytes"] == 256
    assert report["acknowledgements"] == 256
    assert (Path(directory) / "written.bin").read_bytes() == bytes(range(256))
    print(result.stdout, end="")
with tempfile.TemporaryDirectory(prefix="gatehaven-edges-") as directory:
    result = subprocess.run([str(Path(sys.argv[1]).resolve()), directory, "edges"], check=True,
                            capture_output=True, text=True, timeout=20)
    assert json.loads(result.stdout) == {"empty_available": 0, "reset_byte": 66, "reserved_output_bytes": 0}
    print("Protocol edge observations passed")
