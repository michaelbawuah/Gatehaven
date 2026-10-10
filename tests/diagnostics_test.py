"""Structured display reports must identify headless runs and leave user settings alone."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

app = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="gatehaven diagnostics ") as directory:
    root = Path(directory)
    marker = root / "preferences.ghp"; marker.write_bytes(b"user settings")
    env = {**os.environ, "SDL_VIDEODRIVER": "dummy", "XDG_CONFIG_HOME": directory}
    result = subprocess.run([app, "--diagnostics"], capture_output=True, text=True, env=env, check=True, timeout=20)
    report = json.loads(result.stdout)
    assert report["schema"] == 1 and report["product"] == "Gatehaven"
    assert report["physical_device_verified"] is False
    assert report["display"]["video_driver"] == "dummy" and report["display"]["renderer"] == "software"
    for key in ("window_width", "window_height", "pixel_width", "pixel_height", "output_width", "output_height"):
        assert report["display"][key] > 0
    assert report["source_revision"] in report["build_info"]
    for mode in ("--self-test-display", "--benchmark-display"):
        result = subprocess.run([app, mode], capture_output=True, text=True, env=env, timeout=20)
        assert result.returncode == 1 and "native video backend" in result.stderr
    assert marker.read_bytes() == b"user settings" and list(root.iterdir()) == [marker]
    result = subprocess.run([app, "--diagnostics", "extra"], capture_output=True, env=env, timeout=10)
    assert result.returncode == 2
print("Diagnostics JSON, dummy-backend rejection and settings isolation passed")
